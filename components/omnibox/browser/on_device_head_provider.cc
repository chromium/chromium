// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/on_device_head_provider.h"

#include <limits>

#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/i18n/case_conversion.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "components/omnibox/browser/autocomplete_provider_listener.h"
#include "components/omnibox/browser/base_search_provider.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/browser/on_device_model_update_listener.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/search/search.h"
#include "components/search_engines/search_terms_data.h"
#include "components/search_engines/template_url_service.h"
#include "net/base/url_util.h"
#include "third_party/metrics_proto/omnibox_focus_type.pb.h"
#include "third_party/metrics_proto/omnibox_input_type.pb.h"

#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
#include "components/omnibox/browser/on_device_tail_model_executor.h"
#include "components/omnibox/browser/on_device_tail_model_service.h"
#endif

namespace {
const int kBaseRelevanceForUrlInput = 99;
const int kTailBaseRelevance = 90;
const size_t kMaxRequestId = std::numeric_limits<size_t>::max() - 1;

int OnDeviceHeadSuggestMaxScoreForNonUrlInput(bool is_incognito) {
  const int kDefaultScore =
#if BUILDFLAG(IS_IOS)
      99;
#else
      is_incognito ? 99 : 1000;
#endif  // BUILDFLAG(IS_IOS)
  return kDefaultScore;
}

std::string SanitizeInput(const std::u16string& input) {
  std::u16string trimmed_input;
  base::TrimWhitespace(input, base::TRIM_ALL, &trimmed_input);
  return base::UTF16ToUTF8(base::i18n::ToLower(trimmed_input));
}

enum class SuggestionType {
  HEAD = 0,
  TAIL,
};

struct Suggestion {
  std::string text;
  SuggestionType type;

  Suggestion(std::string text, SuggestionType type) : text(text), type(type) {}
};

}  // namespace

struct OnDeviceHeadProvider::OnDeviceHeadProviderParams {
  // The id assigned during request creation, which is used to trace this
  // request and determine whether it is current or obsolete.
  const size_t request_id;

  // AutocompleteInput provided by OnDeviceHeadProvider::Start.
  AutocompleteInput input;

  // The suggestions fetched from the on device model which matches the input.
  std::vector<Suggestion> suggestions;

  // Indicates whether this request failed or not.
  bool failed = false;

  // The time when this request is created.
  base::TimeTicks creation_time;

  OnDeviceHeadProviderParams(size_t request_id, const AutocompleteInput& input)
      : request_id(request_id), input(input) {}

  ~OnDeviceHeadProviderParams() = default;
  OnDeviceHeadProviderParams(const OnDeviceHeadProviderParams&) = delete;
  OnDeviceHeadProviderParams& operator=(const OnDeviceHeadProviderParams&) =
      delete;
};

// static
OnDeviceHeadProvider* OnDeviceHeadProvider::Create(
    AutocompleteProviderClient* client,
    AutocompleteProviderListener* listener) {
  DCHECK(client);
  DCHECK(listener);
  return new OnDeviceHeadProvider(client, listener);
}

OnDeviceHeadProvider::OnDeviceHeadProvider(
    AutocompleteProviderClient* client,
    AutocompleteProviderListener* listener)
    : AutocompleteProvider(AutocompleteProvider::TYPE_ON_DEVICE_HEAD),
      client_(client),
      worker_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN, base::MayBlock()})),
      on_device_search_request_id_(0) {
  AddListener(listener);
}

OnDeviceHeadProvider::~OnDeviceHeadProvider() {}

bool OnDeviceHeadProvider::IsOnDeviceHeadProviderAllowed(
    const AutocompleteInput& input) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);

  // Only accept asynchronous request.
  if (input.omit_asynchronous_matches() ||
      input.type() == metrics::OmniboxInputType::EMPTY)
    return false;

  // Check whether search suggest is enabled.
  if (!client()->SearchSuggestEnabled())
    return false;

  // Check if provider is allowed in incognito / non-incognito.
  if (client()->IsOffTheRecord() &&
      !OmniboxFieldTrial::IsOnDeviceHeadSuggestEnabledForIncognito())
    return false;
  if (!client()->IsOffTheRecord() &&
      !OmniboxFieldTrial::IsOnDeviceHeadSuggestEnabledForNonIncognito())
    return false;

  // Reject on focus request.
  if (input.focus_type() != metrics::OmniboxFocusType::INTERACTION_DEFAULT)
    return false;

  // Do not proceed if default search provider is not Google.
  return search::DefaultSearchProviderIsGoogle(
      client()->GetTemplateURLService());
}

void OnDeviceHeadProvider::Start(const AutocompleteInput& input,
                                 bool minimal_changes) {
  TRACE_EVENT0("omnibox", "OnDeviceHeadProvider::Start");

  // Cancel any in-progress request.
  Stop(!minimal_changes, false);

  if (!IsOnDeviceHeadProviderAllowed(input)) {
    matches_.clear();
    return;
  }

  // If the input text has not changed, the result can be reused.
  if (minimal_changes)
    return;

  matches_.clear();
  if (input.text().empty() || GetOnDeviceHeadModelFilename().empty()) {
    return;
  }

  // Note |on_device_search_request_id_| has already been changed in |Stop| so
  // we don't need to change it again here to get a new id for this request.
  std::unique_ptr<OnDeviceHeadProviderParams> params = base::WrapUnique(
      new OnDeviceHeadProviderParams(on_device_search_request_id_, input));

  done_ = false;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&OnDeviceHeadProvider::DoSearch,
                     weak_ptr_factory_.GetWeakPtr(), std::move(params)));
}

void OnDeviceHeadProvider::Stop(bool clear_cached_results,
                                bool due_to_user_inactivity) {
  AutocompleteProvider::Stop(clear_cached_results, due_to_user_inactivity);

  // Increase the request_id so that any in-progress requests will become
  // obsolete.
  on_device_search_request_id_ =
      (on_device_search_request_id_ + 1) % kMaxRequestId;
  weak_ptr_factory_.InvalidateWeakPtrs();
}

// static
std::unique_ptr<OnDeviceHeadProvider::OnDeviceHeadProviderParams>
OnDeviceHeadProvider::GetSuggestionsFromHeadModel(
    const std::string& model_filename,
    const size_t provider_max_matches,
    std::unique_ptr<OnDeviceHeadProviderParams> params) {
  if (model_filename.empty() || !params) {
    if (params) {
      params->failed = true;
    }
    return params;
  }

  params->creation_time = base::TimeTicks::Now();
  std::string sanitized_input = SanitizeInput(params->input.text());

  auto results = OnDeviceHeadModel::GetSuggestionsForPrefix(
      model_filename, provider_max_matches, sanitized_input);
  params->suggestions.clear();

  for (const auto& item : results) {
    // The second member is the score which is not useful for provider.
    params->suggestions.emplace_back(
        Suggestion(item.first, SuggestionType::HEAD));
  }
  return params;
}

void OnDeviceHeadProvider::AddProviderInfo(ProvidersInfo* provider_info) const {
  provider_info->push_back(metrics::OmniboxEventProto_ProviderInfo());
  metrics::OmniboxEventProto_ProviderInfo& new_entry = provider_info->back();
  new_entry.set_provider(metrics::OmniboxEventProto::ON_DEVICE_HEAD);
  new_entry.set_provider_done(done_);
}

void OnDeviceHeadProvider::DoSearch(
    std::unique_ptr<OnDeviceHeadProviderParams> params) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  if (!params || params->request_id != on_device_search_request_id_) {
    AllSearchDone(std::move(params));
    return;
  }

  worker_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&OnDeviceHeadProvider::GetSuggestionsFromHeadModel,
                     GetOnDeviceHeadModelFilename(), provider_max_matches_,
                     std::move(params)),
      base::BindOnce(&OnDeviceHeadProvider::HeadModelSearchDone,
                     weak_ptr_factory_.GetWeakPtr()));
}

void OnDeviceHeadProvider::HeadModelSearchDone(
    std::unique_ptr<OnDeviceHeadProviderParams> params) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
  if (!OmniboxFieldTrial::IsOnDeviceTailSuggestEnabled() ||
      client()->GetOnDeviceTailModelService() == nullptr) {
    AllSearchDone(std::move(params));
    return;
  }

  bool should_fetch_tail_suggestions =
      (base::GetFieldTrialParamByFeatureAsBool(
           omnibox::kOnDeviceTailModel, "MixHeadAndTailSuggestions", false) ||
       params->suggestions.empty());
  if (!should_fetch_tail_suggestions) {
    AllSearchDone(std::move(params));
    return;
  }

  // Extract search query from current URL.
  std::string previous_query, query_str;
  const GURL& current_url = params->input.current_url();
  if (current_url.path() == "/search" &&
      net::GetValueForKeyInQuery(current_url, "q", &query_str)) {
    previous_query = query_str;
  }
  float probability_threshold = base::GetFieldTrialParamByFeatureAsDouble(
      omnibox::kOnDeviceTailModel, "ProbabilityThreshold", 0.01);

  OnDeviceTailModelExecutor::ModelInput input(
      /*prefix=*/SanitizeInput(params->input.text()),
      /*previous_query=*/previous_query,
      /*max_num_suggestions=*/provider_max_matches_, /*max_rnn_steps=*/20,
      /*probability_threshold=*/probability_threshold);

  client()->GetOnDeviceTailModelService()->GetPredictionsForInput(
      input, base::BindOnce(&OnDeviceHeadProvider::TailModelSearchDone,
                            weak_ptr_factory_.GetWeakPtr(), std::move(params)));
#else
  AllSearchDone(std::move(params));
#endif
}

#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
void OnDeviceHeadProvider::TailModelSearchDone(
    std::unique_ptr<OnDeviceHeadProviderParams> params,
    std::vector<OnDeviceTailModelExecutor::Prediction> predictions) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  for (const auto& prediction : predictions) {
    params->suggestions.emplace_back(
        Suggestion(prediction.suggestion, SuggestionType::TAIL));
  }
  AllSearchDone(std::move(params));
}
#endif

void OnDeviceHeadProvider::AllSearchDone(
    std::unique_ptr<OnDeviceHeadProviderParams> params) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  TRACE_EVENT0("omnibox", "OnDeviceHeadProvider::AllSearchDone");
  // Ignore this request if it has been stopped or a new one has already been
  // created.
  if (!params || params->request_id != on_device_search_request_id_)
    return;

  if (params->failed) {
    done_ = true;
    return;
  }

  const TemplateURLService* template_url_service =
      client()->GetTemplateURLService();

  if (search::DefaultSearchProviderIsGoogle(template_url_service)) {
    UMA_HISTOGRAM_CUSTOM_COUNTS("Omnibox.OnDeviceHeadSuggest.ResultCount",
                                params->suggestions.size(), 1, 5, 6);
    matches_.clear();

    int head_relevance = params->input.type() == metrics::OmniboxInputType::URL
                             ? kBaseRelevanceForUrlInput
                             : OnDeviceHeadSuggestMaxScoreForNonUrlInput(
                                   client()->IsOffTheRecord());
    int tail_relevance = kTailBaseRelevance;

    for (const auto& suggestion : params->suggestions) {
      if (suggestion.type == SuggestionType::HEAD) {
        matches_.push_back(BaseSearchProvider::CreateOnDeviceSearchSuggestion(
            /*autocomplete_provider=*/this, /*input=*/params->input,
            /*suggestion=*/base::UTF8ToUTF16(suggestion.text),
            /*relevance=*/head_relevance--,
            /*template_url=*/
            template_url_service->GetDefaultSearchProvider(),
            /*search_terms_data=*/
            template_url_service->search_terms_data(),
            /*accepted_suggestion=*/TemplateURLRef::NO_SUGGESTION_CHOSEN,
            /*is_tail_suggestion=*/false));
        head_relevance--;
      } else {
        matches_.push_back(BaseSearchProvider::CreateOnDeviceSearchSuggestion(
            /*autocomplete_provider=*/this, /*input=*/params->input,
            /*suggestion=*/base::UTF8ToUTF16(suggestion.text),
            /*relevance=*/tail_relevance--,
            /*template_url=*/
            template_url_service->GetDefaultSearchProvider(),
            /*search_terms_data=*/
            template_url_service->search_terms_data(),
            /*accepted_suggestion=*/TemplateURLRef::NO_SUGGESTION_CHOSEN,
            /*is_tail_suggestion=*/true));
        tail_relevance--;
      }
    }
    UMA_HISTOGRAM_TIMES("Omnibox.OnDeviceHeadSuggest.AsyncQueryTime",
                        base::TimeTicks::Now() - params->creation_time);
  }

  done_ = true;
  NotifyListeners(true);
}

// TODO(crbug.com/1372112): update head model class to take file path instead
// of the std::string file name.
// static
std::string OnDeviceHeadProvider::GetOnDeviceHeadModelFilename() const {
  auto* model_update_listener = OnDeviceModelUpdateListener::GetInstance();
  return model_update_listener != nullptr
             ? model_update_listener->head_model_filename()
             : "";
}
