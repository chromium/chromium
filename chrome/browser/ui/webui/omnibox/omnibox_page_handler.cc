// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/omnibox/omnibox_page_handler.h"

#include <stddef.h>

#include <algorithm>
#include <string>
#include <utility>

#include "base/base64.h"
#include "base/check.h"
#include "base/containers/to_vector.h"
#include "base/functional/bind.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/autocomplete/autocomplete_scoring_model_service_factory.h"
#include "chrome/browser/autocomplete/chrome_autocomplete_provider_client.h"
#include "chrome/browser/autocomplete/chrome_autocomplete_scheme_classifier.h"
#include "chrome/browser/bitmap_fetcher/bitmap_fetcher_service.h"
#include "chrome/browser/bitmap_fetcher/bitmap_fetcher_service_factory.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/omnibox/autocomplete_controller_emitter_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/webui/omnibox/omnibox_internals.mojom-forward.h"
#include "chrome/browser/ui/webui/omnibox/omnibox_mojom_traits.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/url_database.h"
#include "components/omnibox/browser/actions/omnibox_pedal.h"
#include "components/omnibox/browser/autocomplete_classifier.h"
#include "components/omnibox/browser/autocomplete_controller.h"
#include "components/omnibox/browser/autocomplete_controller_config.h"
#include "components/omnibox/browser/autocomplete_controller_emitter.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_provider.h"
#include "components/omnibox/browser/autocomplete_result.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/common/omnibox_feature_configs.h"
#include "components/optimization_guide/machine_learning_tflite_buildflags.h"
#include "components/search_engines/template_url.h"
#include "content/public/browser/web_ui.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"
#include "third_party/metrics_proto/omnibox_focus_type.pb.h"
#include "third_party/omnibox_proto/answer_data.pb.h"
#include "third_party/omnibox_proto/answer_type.pb.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/image/image.h"

#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
#include "components/omnibox/browser/autocomplete_scoring_model_service.h"
#endif

OmniboxPageHandler::OmniboxPageHandler(
    Profile* profile,
    mojo::PendingReceiver<mojom::OmniboxPageHandler> receiver)
    : profile_(profile), receiver_(this, std::move(receiver)) {
  observation_.Observe(
      AutocompleteControllerEmitterFactory::GetForBrowserContext(profile_));
  controller_ = CreateController(false);
  ml_disabled_controller_ = CreateController(true);
}

OmniboxPageHandler::~OmniboxPageHandler() = default;

void OmniboxPageHandler::OnStart(AutocompleteController* controller,
                                 const AutocompleteInput& input) {
  time_omnibox_started_ = base::Time::Now();
  input_ = input;
  auto type = GetAutocompleteControllerType(controller);
  page_->HandleNewAutocompleteQuery(type, base::UTF16ToUTF8(input.text()));
  // Kick off ml-disabled autocompletion to show a before/after comparison on
  // chrome://omnibox/ml.
  if (type == mojom::AutocompleteControllerType::kBrowser) {
    ml_disabled_controller_->Start(input);
  }
}

void OmniboxPageHandler::OnResultChanged(AutocompleteController* controller,
                                         bool default_match_changed) {
  mojom::OmniboxResponsePtr response(mojom::OmniboxResponse::New());
  response->cursor_position = input_.cursor_position();
  response->time_since_omnibox_started_ms =
      (base::Time::Now() - time_omnibox_started_).InMilliseconds();
  response->done = controller->done();
  response->type = AutocompleteInput::TypeToString(input_.type());
  const std::u16string host =
      input_.text().substr(input_.parts().host.begin, input_.parts().host.len);
  response->host = base::UTF16ToUTF8(host);
  bool is_typed_host;
  if (!LookupIsTypedHost(host, &is_typed_host)) {
    is_typed_host = false;
  }
  response->is_typed_host = is_typed_host;
  response->input_text = base::UTF16ToUTF8(input_.text());

  // `AutocompleteMatch` cannot be serialized directly, since the Mojo type
  // includes fields that can only be calculated in `//chrome` and not
  // `//components`.
  bookmarks::BookmarkModel* bookmark_model =
      BookmarkModelFactory::GetForBrowserContext(profile_);
  response->combined_results = base::ToVector(
      controller->result(), [&](const ::AutocompleteMatch& match) {
        return ::AutocompleteMatchWrapper(bookmark_model, match);
      });
  std::vector<scoped_refptr<AutocompleteProvider>> providers = {};
  for (const auto& provider : controller->providers()) {
    if (controller->ShouldRunProvider(provider.get())) {
      providers.push_back(provider);
    }
  }
  response->results_by_provider = base::ToVector(
      providers, [&](const scoped_refptr<AutocompleteProvider>& input) {
        return mojom::AutocompleteResultsForProvider::New(
            input->GetName(),
            base::ToVector(
                input->matches(), [&](const ::AutocompleteMatch& match) {
                  return ::AutocompleteMatchWrapper(bookmark_model, match);
                }));
      });

  std::vector<GURL> image_urls;
  for (const auto& wrapper : response->combined_results) {
    image_urls.push_back(wrapper.wrapped_match().icon_url);
    image_urls.push_back(wrapper.wrapped_match().ImageUrl());
  }
  for (const auto& results_by_provider : response->results_by_provider) {
    for (const auto& wrapper : results_by_provider->results) {
      image_urls.push_back(wrapper.wrapped_match().icon_url);
      image_urls.push_back(wrapper.wrapped_match().ImageUrl());
    }
  }

  auto type = GetAutocompleteControllerType(controller);
  page_->HandleNewAutocompleteResponse(type, std::move(response));

  // Fill in image data
  BitmapFetcherService* bitmap_fetcher_service =
      BitmapFetcherServiceFactory::GetForBrowserContext(profile_);

  for (const GURL& image_url : image_urls) {
    if (!image_url.is_valid()) {
      continue;
    }
    bitmap_fetcher_service->RequestImage(
        image_url, base::BindOnce(&OmniboxPageHandler::OnBitmapFetched,
                                  weak_factory_.GetWeakPtr(), type, image_url));
  }
}

void OmniboxPageHandler::OnMlScored(AutocompleteController* controller,
                                    const AutocompleteResult& result) {
  auto combined_results =
      base::ToVector(result, [](const ::AutocompleteMatch& match) {
        return ::AutocompleteMatchWrapper(nullptr, match);
      });
  page_->HandleNewMlResponse(GetAutocompleteControllerType(controller),
                             base::UTF16ToUTF8(controller->input().text()),
                             std::move(combined_results));
}

void OmniboxPageHandler::OnBitmapFetched(mojom::AutocompleteControllerType type,
                                         const GURL& image_url,
                                         const SkBitmap& bitmap) {
  auto data = gfx::Image::CreateFrom1xBitmap(bitmap).As1xPNGBytes();
  std::string base_64 = base::Base64Encode(*data);
  const char kDataUrlPrefix[] = "data:image/png;base64,";
  std::string data_url = GURL(kDataUrlPrefix + base_64).spec();
  page_->HandleAnswerIconImageData(type, image_url, data_url);
}

bool OmniboxPageHandler::LookupIsTypedHost(const std::u16string& host,
                                           bool* is_typed_host) const {
  history::HistoryService* const history_service =
      HistoryServiceFactory::GetForProfile(profile_,
                                           ServiceAccessType::EXPLICIT_ACCESS);
  if (!history_service) {
    return false;
  }
  history::URLDatabase* url_db = history_service->InMemoryDatabase();
  if (!url_db) {
    return false;
  }
  *is_typed_host =
      url_db->IsTypedHost(base::UTF16ToUTF8(host), /*scheme=*/nullptr);
  return true;
}

void OmniboxPageHandler::SetClientPage(
    mojo::PendingRemote<mojom::OmniboxPage> page) {
  page_.Bind(std::move(page));
}

void OmniboxPageHandler::StartOmniboxQuery(const std::string& input_string,
                                           bool reset_autocomplete_controller,
                                           int32_t cursor_position,
                                           bool zero_suggest,
                                           bool prevent_inline_autocomplete,
                                           bool prefer_keyword,
                                           const std::string& current_url,
                                           int32_t page_classification) {
  // Reset the controller.  If we don't do this, then the
  // AutocompleteController might inappropriately set its |minimal_changes|
  // variable (or something else) and some providers will short-circuit
  // important logic and return stale results.  In short, we want the
  // actual results to not depend on the state of the previous request.
  if (reset_autocomplete_controller) {
    controller_ = CreateController(false);
  }
  AutocompleteInput input(
      base::UTF8ToUTF16(input_string), cursor_position,
      static_cast<metrics::OmniboxEventProto::PageClassification>(
          page_classification),
      ChromeAutocompleteSchemeClassifier(profile_));
  GURL current_url_gurl{current_url};
  if (current_url_gurl.is_valid()) {
    input.set_current_url(current_url_gurl);
  }
  input.set_current_title(base::UTF8ToUTF16(current_url));
  input.set_prevent_inline_autocomplete(prevent_inline_autocomplete);
  input.set_prefer_keyword(prefer_keyword);
  if (prefer_keyword) {
    input.set_keyword_mode_entry_method(metrics::OmniboxEventProto::TAB);
  }
  input.set_focus_type(zero_suggest
                           ? metrics::OmniboxFocusType::INTERACTION_FOCUS
                           : metrics::OmniboxFocusType::INTERACTION_DEFAULT);
  controller_->Start(input);
}

void OmniboxPageHandler::GetMlModelVersion(GetMlModelVersionCallback callback) {
#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
  if (auto* service = GetMlService()) {
    auto version = service->GetModelVersion();
    if (version == -1) {
      service->AddOnModelUpdatedCallback(
          base::BindOnce(&OmniboxPageHandler::GetMlModelVersion,
                         weak_factory_.GetWeakPtr(), std::move(callback)));
    } else {
      std::move(callback).Run(version);
    }
  } else {
    std::move(callback).Run(-1);
  }
#else
  std::move(callback).Run(-1);
#endif
}

void OmniboxPageHandler::StartMl(
    const AutocompleteMatch::ScoringSignals& signals,
    StartMlCallback callback) {
#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
  if (auto* service = GetMlService()) {
    std::vector<AutocompleteScoringModelService::Result> result =
        service->BatchScoreAutocompleteUrlMatchesSync({&signals});
    std::move(callback).Run(result.size() ? result[0].value_or(-1) : -1);
  } else {
    std::move(callback).Run(-1);
  }
#else
  std::move(callback).Run(-1);
#endif
}

std::unique_ptr<AutocompleteController> OmniboxPageHandler::CreateController(
    bool ml_disabled) {
  auto providers = AutocompleteClassifier::DefaultOmniboxProviders();
  // `HistoryEmbeddingsProvider` only supports 1 query at a time. Running it for
  // the traditional-scoring controller used in the ML before/after comparisons
  // would break history embeddings for the other, more important controllers.
  if (ml_disabled) {
    providers &= ~AutocompleteProvider::TYPE_HISTORY_EMBEDDINGS;
  }

  auto controller = std::make_unique<AutocompleteController>(
      std::make_unique<ChromeAutocompleteProviderClient>(profile_),
      AutocompleteControllerConfig{.provider_types = providers,
                                   .disable_ml = ml_disabled});
  // We will observe our internal AutocompleteController directly, so there's
  // no reason to hook it up to the profile-keyed AutocompleteControllerEmitter.
  controller->AddObserver(this);
  return controller;
}

mojom::AutocompleteControllerType
OmniboxPageHandler::GetAutocompleteControllerType(
    AutocompleteController* controller) {
  if (controller == controller_.get()) {
    return mojom::AutocompleteControllerType::kDebug;
  }
  if (controller == ml_disabled_controller_.get()) {
    return mojom::AutocompleteControllerType::kMlDisabledDebug;
  }
  return mojom::AutocompleteControllerType::kBrowser;
}

AutocompleteScoringModelService* OmniboxPageHandler::GetMlService() {
#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
  return OmniboxFieldTrial::IsMlUrlScoringEnabled()
             ? AutocompleteScoringModelServiceFactory::GetInstance()
                   ->GetForProfile(profile_)
             : nullptr;
#else
  return nullptr;
#endif
}
