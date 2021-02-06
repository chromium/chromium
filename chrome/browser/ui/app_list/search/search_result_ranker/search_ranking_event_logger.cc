// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/search_result_ranker/search_ranking_event_logger.h"

#include <utility>

#include "ash/public/cpp/app_list/app_list_types.h"
#include "base/containers/flat_map.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/app_list/search/chrome_search_result.h"
#include "chrome/browser/ui/app_list/search/omnibox_result.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/search_ranking_event.pb.h"
#include "chrome/grit/browser_resources.h"
#include "chromeos/services/machine_learning/public/cpp/service_connection.h"
#include "chromeos/services/machine_learning/public/mojom/machine_learning_service.mojom.h"
#include "components/assist_ranker/example_preprocessing.h"
#include "components/crx_file/id_util.h"
#include "components/omnibox/browser/autocomplete_match_type.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "ui/base/resource/resource_bundle.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace app_list {
namespace {

using chromeos::machine_learning::mojom::BuiltinModelId;
using chromeos::machine_learning::mojom::BuiltinModelSpec;
using chromeos::machine_learning::mojom::CreateGraphExecutorResult;
using chromeos::machine_learning::mojom::ExecuteResult;
using chromeos::machine_learning::mojom::FloatList;
using chromeos::machine_learning::mojom::Int64List;
using chromeos::machine_learning::mojom::LoadModelResult;
using chromeos::machine_learning::mojom::Tensor;
using chromeos::machine_learning::mojom::TensorPtr;
using chromeos::machine_learning::mojom::ValueList;
using ukm::GetExponentialBucketMinForCounts1000;

// How long to wait for a URL to enter the history service before querying it
// for a UKM source ID.
constexpr base::TimeDelta kDelayForHistoryService =
    base::TimeDelta::FromSeconds(15);

// Chosen so that the bucket at the 24 hour mark is ~60 minutes long. The bucket
// exponent used for counts that are not seconds is 1.15 (via
// ukm::GetExponentialBucketMinForCounts1000). The first value skipped by
// bucketing is 10.
constexpr float kBucketExponentForSeconds = 1.045f;

// The UMA histogram that logs the error occurs in inference code.
constexpr char kInferenceError[] = "Apps.AppList.AggregatedSearchRankerError";

// Represents type of error in inference. These values are persisted to logs.
// Entries should not be renumbered and numeric values should never be reused.
enum class InferenceError {
  kUnknown = 0,
  kLoadModelFailed = 1,
  kCreateGraphFailed = 2,
  kLoadExamplePreprocessorConfigFailed = 3,
  kVectorizeFeaturesFailed = 4,
  kInferenceExecutionFailed = 5,
  kMaxValue = kInferenceExecutionFailed,
};

// Represents the type of a search result. The indices of these values
// persist to logs, so existing values should not be modified.
enum class Category {
  UNKNOWN = 0,
  FILE = 1,
  HISTORY = 2,
  NAV_SUGGEST = 3,
  SEARCH = 4,
  BOOKMARK = 5,
  DOCUMENT = 6,
  OMNIBOX_DEPRECATED = 7,
  OMNIBOX_GENERIC = 8
};

int ExtensionTypeFromFileName(const std::string& file_name) {
  // This is a limited list of commonly used extensions. The index of an
  // extension in this list persists to logs, so existing values should not be
  // modified and new values should only be added to the end. This should be
  // kept in sync with AppListNonAppImpressionFileExtension in
  // histograms/enums.xml
  static const base::NoDestructor<std::vector<std::string>> known_extensions(
      {".3ga",        ".3gp",    ".aac",     ".alac", ".asf",  ".avi",
       ".bmp",        ".csv",    ".doc",     ".docx", ".flac", ".gif",
       ".jpeg",       ".jpg",    ".log",     ".m3u",  ".m3u8", ".m4a",
       ".m4v",        ".mid",    ".mkv",     ".mov",  ".mp3",  ".mp4",
       ".mpg",        ".odf",    ".odp",     ".ods",  ".odt",  ".oga",
       ".ogg",        ".ogv",    ".pdf",     ".png",  ".ppt",  ".pptx",
       ".ra",         ".ram",    ".rar",     ".rm",   ".rtf",  ".wav",
       ".webm",       ".webp",   ".wma",     ".wmv",  ".xls",  ".xlsx",
       ".crdownload", ".crx",    ".dmg",     ".exe",  ".html", ".htm",
       ".jar",        ".ps",     ".torrent", ".txt",  ".zip",  ".mhtml",
       ".gdoc",       ".gsheet", ".gslides"});

  size_t found = file_name.find_last_of(".");
  if (found == std::string::npos)
    return -1;
  return std::distance(
      known_extensions->begin(),
      std::find(known_extensions->begin(), known_extensions->end(),
                file_name.substr(found)));
}

Category CategoryFromResultType(ash::AppListSearchResultType type,
                                int subtype) {
  if (type == ash::AppListSearchResultType::kLauncher)
    return Category::FILE;

  if (type == ash::AppListSearchResultType::kOmnibox) {
    switch (static_cast<AutocompleteMatchType::Type>(subtype)) {
      case AutocompleteMatchType::Type::HISTORY_URL:
      case AutocompleteMatchType::Type::HISTORY_TITLE:
      case AutocompleteMatchType::Type::HISTORY_BODY:
      case AutocompleteMatchType::Type::HISTORY_KEYWORD:
        return Category::HISTORY;
      case AutocompleteMatchType::Type::NAVSUGGEST:
      case AutocompleteMatchType::Type::NAVSUGGEST_PERSONALIZED:
        return Category::NAV_SUGGEST;
      case AutocompleteMatchType::Type::SEARCH_HISTORY:
      case AutocompleteMatchType::Type::SEARCH_SUGGEST:
      case AutocompleteMatchType::Type::SEARCH_SUGGEST_ENTITY:
      case AutocompleteMatchType::Type::SEARCH_SUGGEST_TAIL:
      case AutocompleteMatchType::Type::SEARCH_SUGGEST_PERSONALIZED:
      case AutocompleteMatchType::Type::SEARCH_SUGGEST_PROFILE:
      case AutocompleteMatchType::Type::SEARCH_OTHER_ENGINE:
        return Category::SEARCH;
      case AutocompleteMatchType::Type::BOOKMARK_TITLE:
        return Category::BOOKMARK;
      case AutocompleteMatchType::Type::DOCUMENT_SUGGESTION:
        return Category::DOCUMENT;
      case AutocompleteMatchType::Type::EXTENSION_APP_DEPRECATED:
      case AutocompleteMatchType::Type::CONTACT_DEPRECATED:
      case AutocompleteMatchType::Type::PHYSICAL_WEB_DEPRECATED:
      case AutocompleteMatchType::Type::PHYSICAL_WEB_OVERFLOW_DEPRECATED:
      case AutocompleteMatchType::Type::TAB_SEARCH_DEPRECATED:
        return Category::OMNIBOX_DEPRECATED;
      default:
        return Category::OMNIBOX_GENERIC;
    }
  }

  return Category::UNKNOWN;
}

void LogInferenceError(const InferenceError& error) {
  UMA_HISTOGRAM_ENUMERATION(kInferenceError, error);
}

int GetExponentialBucketMinForSeconds(int64_t sample) {
  return ukm::GetExponentialBucketMin(sample, kBucketExponentForSeconds);
}
void LoadModelCallback(LoadModelResult result) {
  if (result != LoadModelResult::OK) {
    LOG(ERROR) << "Failed to load Search Ranker model.";
    LogInferenceError(InferenceError::kLoadModelFailed);
  }
}

void CreateGraphExecutorCallback(CreateGraphExecutorResult result) {
  if (result != CreateGraphExecutorResult::OK) {
    LOG(ERROR) << "Failed to create a Search Ranker Graph Executor.";
    LogInferenceError(InferenceError::kCreateGraphFailed);
  }
}

// Populates |example| using |features|.
void PopulateRankerExample(const SearchRankingItem::Features& features,
                           assist_ranker::RankerExample* example) {
  CHECK(example);

  auto& ranker_example_features = *example->mutable_features();
  ranker_example_features["QueryLength"].set_int32_value(
      features.query_length());
  ranker_example_features["RelevanceScore"].set_int32_value(
      features.relevance_score());
  ranker_example_features["Category"].set_int32_value(features.category());
  ranker_example_features["HourOfDay"].set_int32_value(features.hour_of_day());
  ranker_example_features["DayOfWeek"].set_int32_value(features.day_of_week());
  ranker_example_features["LaunchesThisSession"].set_int32_value(
      features.launches_this_session());
  if (features.has_file_extension()) {
    ranker_example_features["FileExtension"].set_int32_value(
        features.file_extension());
  }
  if (features.has_time_since_last_launch()) {
    ranker_example_features["TimeSinceLastLaunch"].set_int32_value(
        features.time_since_last_launch());
    ranker_example_features["TimeOfLastLaunch"].set_int32_value(
        features.time_of_last_launch());
  }
  const auto& launches = features.launches_at_hour();
  for (int hour = 0; hour < launches.size(); hour++) {
    ranker_example_features["LaunchesAtHour" + base::StringPrintf("%02d", hour)]
        .set_int32_value(launches[hour]);
  }
  if (features.has_domain()) {
    ranker_example_features["Domain"].set_string_value(features.domain());
    ranker_example_features["HasDomain"].set_int32_value(1);
  }
}

// Loads the preprocessor config protobuf, which will be used later to convert
// a RankerExample to a vectorized float for inactivity score calculation.
// Returns nullptr if cannot load or parse the config.
std::unique_ptr<assist_ranker::ExamplePreprocessorConfig>
LoadExamplePreprocessorConfig() {
  auto config = std::make_unique<assist_ranker::ExamplePreprocessorConfig>();

  const int res_id = IDR_SEARCH_RANKER_20190923_EXAMPLE_PREPROCESSOR_CONFIG_PB;

  scoped_refptr<base::RefCountedMemory> raw_config =
      ui::ResourceBundle::GetSharedInstance().LoadDataResourceBytes(res_id);
  if (!raw_config || !raw_config->front()) {
    LOG(ERROR) << "Failed to load SearchRanker example preprocessor config.";
    LogInferenceError(InferenceError::kLoadExamplePreprocessorConfigFailed);
    return nullptr;
  }

  if (!config->ParseFromArray(raw_config->front(), raw_config->size())) {
    LOG(ERROR) << "Failed to parse SearchRanker example preprocessor config.";
    LogInferenceError(InferenceError::kLoadExamplePreprocessorConfigFailed);
    return nullptr;
  }

  return config;
}
}  // namespace

SearchRankingEventLogger::SearchRankingEventLogger(
    Profile* profile,
    SearchController* search_controller)
    : search_controller_(search_controller),
      ukm_recorder_(ukm::UkmRecorder::Get()),
      ukm_background_recorder_(
          ukm::UkmBackgroundRecorderFactory::GetForProfile(profile)),
      weak_factory_(this) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(search_controller_);
}

SearchRankingEventLogger::~SearchRankingEventLogger() = default;

SearchRankingEventLogger::ResultState::ResultState() = default;
SearchRankingEventLogger::ResultState::~ResultState() = default;

void SearchRankingEventLogger::SetEventRecordedForTesting(
    base::OnceClosure closure) {
  event_recorded_for_testing_ = std::move(closure);
}

void SearchRankingEventLogger::PopulateSearchRankingItem(
    SearchRankingItem* proto,
    ChromeSearchResult* search_result,
    int query_length,
    bool use_for_logging) {
  const base::Time now = base::Time::Now();
  base::Time::Exploded now_exploded;
  now.LocalExplode(&now_exploded);

  auto& features = *proto->mutable_features();
  features.set_category(static_cast<int>(CategoryFromResultType(
      search_result->result_type(), search_result->result_subtype())));

  // Note this is the search provider's original relevance score, not
  // tweaked by any search ranking models. Scores are floats in 0 to 1, and
  // we map this to ints 0 to 100.
  features.set_relevance_score(
      static_cast<int>(100 * search_result->relevance()));
  features.set_hour_of_day(now_exploded.hour);
  features.set_day_of_week(now_exploded.day_of_week);
  features.set_query_length(GetExponentialBucketMinForCounts1000(query_length));

  if (features.category() == static_cast<int>(Category::FILE)) {
    features.set_file_extension(ExtensionTypeFromFileName(search_result->id()));
  }

  if (search_result->result_type() == ash::AppListSearchResultType::kOmnibox) {
    // The id metadata of an OmniboxResult is a stripped URL, which does not
    // correspond to the URL that will be navigated to.
    proto->set_target(
        static_cast<OmniboxResult*>(search_result)->DestinationURL().spec());
  } else {
    proto->set_target(search_result->id());
  }

  const std::string& domain = GURL(search_result->id()).host();
  if (!domain.empty()) {
    features.set_domain(domain);
  }

  // If the proto is created for logging purposes, create a new item in the map.
  // Otherwise lookup the map for event info and create a "dummy" event info if
  // doesn't nothing found.
  ResultState* event_info;
  ResultState dummy_event_info;
  if (use_for_logging) {
    event_info = &id_to_result_state_[proto->target()];
  } else {
    const auto& it = id_to_result_state_.find(proto->target());
    if (it != id_to_result_state_.end()) {
      event_info = &it->second;
    } else {
      event_info = &dummy_event_info;
    }
  }

  if (event_info->last_launch.has_value()) {
    base::Time last_launch = event_info->last_launch.value();
    base::Time::Exploded last_launch_exploded;
    last_launch.LocalExplode(&last_launch_exploded);

    features.set_time_since_last_launch(
        GetExponentialBucketMinForSeconds((now - last_launch).InSeconds()));
    features.set_time_of_last_launch(last_launch_exploded.hour);

    // Reset the number of launches this hour to 0 if this is the first
    // launch today of this event, to account for user sessions spanning
    // multiple days.
    if (features.has_is_launched() && features.is_launched() == 1 &&
        now - event_info->last_launch.value() >=
            base::TimeDelta::FromHours(23)) {
      event_info->launches_per_hour[now_exploded.hour] = 0;
    }
  }

  features.set_launches_this_session(
      GetExponentialBucketMinForCounts1000(event_info->launches_this_session));

  const auto& launches = event_info->launches_per_hour;
  features.add_launches_at_hour(
      GetExponentialBucketMinForCounts1000(launches[0]));
  features.add_launches_at_hour(
      GetExponentialBucketMinForCounts1000(launches[1]));
  features.add_launches_at_hour(
      GetExponentialBucketMinForCounts1000(launches[2]));
  features.add_launches_at_hour(
      GetExponentialBucketMinForCounts1000(launches[3]));
  features.add_launches_at_hour(
      GetExponentialBucketMinForCounts1000(launches[4]));
  features.add_launches_at_hour(
      GetExponentialBucketMinForCounts1000(launches[5]));
  features.add_launches_at_hour(
      GetExponentialBucketMinForCounts1000(launches[6]));
  features.add_launches_at_hour(
      GetExponentialBucketMinForCounts1000(launches[7]));
  features.add_launches_at_hour(
      GetExponentialBucketMinForCounts1000(launches[8]));
  features.add_launches_at_hour(
      GetExponentialBucketMinForCounts1000(launches[9]));
  features.add_launches_at_hour(
      GetExponentialBucketMinForCounts1000(launches[10]));
  features.add_launches_at_hour(
      GetExponentialBucketMinForCounts1000(launches[11]));
  features.add_launches_at_hour(
      GetExponentialBucketMinForCounts1000(launches[12]));
  features.add_launches_at_hour(
      GetExponentialBucketMinForCounts1000(launches[13]));
  features.add_launches_at_hour(
      GetExponentialBucketMinForCounts1000(launches[14]));
  features.add_launches_at_hour(
      GetExponentialBucketMinForCounts1000(launches[15]));
  features.add_launches_at_hour(
      GetExponentialBucketMinForCounts1000(launches[16]));
  features.add_launches_at_hour(
      GetExponentialBucketMinForCounts1000(launches[17]));
  features.add_launches_at_hour(
      GetExponentialBucketMinForCounts1000(launches[18]));
  features.add_launches_at_hour(
      GetExponentialBucketMinForCounts1000(launches[19]));
  features.add_launches_at_hour(
      GetExponentialBucketMinForCounts1000(launches[20]));
  features.add_launches_at_hour(
      GetExponentialBucketMinForCounts1000(launches[21]));
  features.add_launches_at_hour(
      GetExponentialBucketMinForCounts1000(launches[22]));
  features.add_launches_at_hour(
      GetExponentialBucketMinForCounts1000(launches[23]));

  if (features.has_is_launched() && features.is_launched() == 1) {
    event_info->last_launch = now;
    event_info->launches_this_session += 1;
    event_info->launches_per_hour[now_exploded.hour] += 1;
  }
}

void SearchRankingEventLogger::Log(
    const base::string16& trimmed_query,
    const ash::SearchResultIdWithPositionIndices& search_results,
    int launched_index) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (const auto& id_index : search_results) {
    auto* result = search_controller_->FindSearchResult(id_index.id);
    if (!result)
      continue;

    SearchRankingItem proto;
    proto.mutable_features()->set_position(id_index.position_index);
    proto.set_event_id(next_event_id_);
    proto.mutable_features()->set_is_launched(
        id_index.position_index == launched_index ? 1 : 0);
    PopulateSearchRankingItem(&proto, result, trimmed_query.size(),
                              true /*use_for_logging*/);

    // Omnibox results have associated URLs, so are logged keyed on the URL
    // after validating that it exists in the history service. Other results
    // have no associated URL, so use a blank source ID.
    if (result->result_type() == ash::AppListSearchResultType::kOmnibox) {
      // When an omnibox result is launched, we need to retrieve a source ID
      // using the history service. This may be the first time the URL is used
      // and so it must be committed to the history service database before we
      // retrieve it, which happens once the page has loaded. So we delay our
      // check for long enough that most pages will have loaded.
      if (launched_index == id_index.position_index) {
        base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
            FROM_HERE,
            base::BindOnce(
                &SearchRankingEventLogger::GetBackgroundSourceIdAndLogEvent,
                weak_factory_.GetWeakPtr(), proto),
            kDelayForHistoryService);
      } else {
        GetBackgroundSourceIdAndLogEvent(proto);
      }
    } else {
      LogEvent(proto, base::nullopt);
    }
  }

  ++next_event_id_;
}

void SearchRankingEventLogger::GetBackgroundSourceIdAndLogEvent(
    const SearchRankingItem& result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ukm_background_recorder_->GetBackgroundSourceIdIfAllowed(
      url::Origin::Create(GURL(result.target())),
      base::BindOnce(&SearchRankingEventLogger::LogEvent,
                     base::Unretained(this), result));
}

void SearchRankingEventLogger::LogEvent(
    const SearchRankingItem& result,
    base::Optional<ukm::SourceId> source_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!source_id)
    source_id = ukm_recorder_->GetNewSourceID();

  ukm::builders::AppListNonAppImpression event(source_id.value());
  event.SetEventId(result.event_id())
      .SetPosition(result.features().position())
      .SetIsLaunched(result.features().is_launched())
      .SetQueryLength(result.features().query_length())
      .SetRelevanceScore(result.features().relevance_score())
      .SetCategory(result.features().category())
      .SetHourOfDay(result.features().hour_of_day())
      .SetDayOfWeek(result.features().day_of_week())
      .SetLaunchesThisSession(result.features().launches_this_session());

  if (result.features().has_file_extension()) {
    event.SetFileExtension(result.features().file_extension());
  }

  if (result.features().has_time_since_last_launch()) {
    event.SetTimeSinceLastLaunch(result.features().time_since_last_launch());
    event.SetTimeOfLastLaunch(result.features().time_of_last_launch());
  }

  const auto& launches = result.features().launches_at_hour();
  event.SetLaunchesAtHour00(launches[0]);
  event.SetLaunchesAtHour01(launches[1]);
  event.SetLaunchesAtHour02(launches[2]);
  event.SetLaunchesAtHour03(launches[3]);
  event.SetLaunchesAtHour04(launches[4]);
  event.SetLaunchesAtHour05(launches[5]);
  event.SetLaunchesAtHour06(launches[6]);
  event.SetLaunchesAtHour07(launches[7]);
  event.SetLaunchesAtHour08(launches[8]);
  event.SetLaunchesAtHour09(launches[9]);
  event.SetLaunchesAtHour10(launches[10]);
  event.SetLaunchesAtHour11(launches[11]);
  event.SetLaunchesAtHour12(launches[12]);
  event.SetLaunchesAtHour13(launches[13]);
  event.SetLaunchesAtHour14(launches[14]);
  event.SetLaunchesAtHour15(launches[15]);
  event.SetLaunchesAtHour16(launches[16]);
  event.SetLaunchesAtHour17(launches[17]);
  event.SetLaunchesAtHour18(launches[18]);
  event.SetLaunchesAtHour19(launches[19]);
  event.SetLaunchesAtHour20(launches[20]);
  event.SetLaunchesAtHour21(launches[21]);
  event.SetLaunchesAtHour22(launches[22]);
  event.SetLaunchesAtHour23(launches[23]);

  event.Record(ukm_recorder_);

  if (event_recorded_for_testing_)
    std::move(event_recorded_for_testing_).Run();
}

void SearchRankingEventLogger::CreateRankings(Mixer::SortedResults* results,
                                              int query_length) {
  for (const auto& result : *results) {
    if (!result.result) {
      continue;
    }
    SearchRankingItem proto;
    std::vector<float> vectorized_features;

    PopulateSearchRankingItem(&proto, result.result, query_length,
                              false /*use_for_logging*/);
    if (!PreprocessInput(proto.features(), &vectorized_features)) {
      return;
    }
    DoInference(vectorized_features, result.result->id());
  }
}

std::map<std::string, float> SearchRankingEventLogger::RetrieveRankings() {
  return prediction_;
}

void SearchRankingEventLogger::LazyInitialize() {
  if (!preprocessor_config_) {
    preprocessor_config_ = LoadExamplePreprocessorConfig();
  }
}

bool SearchRankingEventLogger::PreprocessInput(
    const SearchRankingItem::Features& features,
    std::vector<float>* vectorized_features) {
  DCHECK(vectorized_features);
  LazyInitialize();

  if (!preprocessor_config_) {
    LOG(ERROR) << "Failed to create preprocessor config.";
    LogInferenceError(InferenceError::kLoadExamplePreprocessorConfigFailed);
    return false;
  }

  assist_ranker::RankerExample ranker_example;
  PopulateRankerExample(features, &ranker_example);

  int preprocessor_error = assist_ranker::ExamplePreprocessor::Process(
      *preprocessor_config_, &ranker_example, true);
  // kNoFeatureIndexFound can occur normally (e.g., when the domain name
  // isn't known to the model or a rarely seen enum value is used).
  if (preprocessor_error != assist_ranker::ExamplePreprocessor::kSuccess &&
      preprocessor_error !=
          assist_ranker::ExamplePreprocessor::kNoFeatureIndexFound) {
    LOG(ERROR) << "Failed to vectorize features using ExamplePreprocessor.";
    LogInferenceError(InferenceError::kVectorizeFeaturesFailed);
    return false;
  }

  const auto& extracted_features =
      ranker_example.features()
          .at(assist_ranker::ExamplePreprocessor::kVectorizedFeatureDefaultName)
          .float_list()
          .float_value();
  vectorized_features->assign(extracted_features.begin(),
                              extracted_features.end());
  return true;
}

void SearchRankingEventLogger::DoInference(const std::vector<float>& features,
                                           const std::string& id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  BindGraphExecutorIfNeeded();

  // Prepare the input tensor.
  base::flat_map<std::string, TensorPtr> inputs;
  auto tensor = Tensor::New();
  tensor->shape = Int64List::New();
  tensor->shape->value = std::vector<int64_t>({1, features.size()});
  tensor->data = ValueList::New();
  tensor->data->set_float_list(FloatList::New());
  tensor->data->get_float_list()->value =
      std::vector<double>(std::begin(features), std::end(features));
  inputs.emplace(std::string("input"), std::move(tensor));

  const std::vector<std::string> outputs({std::string("output")});
  // Execute
  executor_->Execute(std::move(inputs), std::move(outputs),
                     base::BindOnce(&SearchRankingEventLogger::ExecuteCallback,
                                    weak_factory_.GetWeakPtr(), id));
}

void SearchRankingEventLogger::BindGraphExecutorIfNeeded() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!model_) {
    // Load the model.
    auto spec = BuiltinModelSpec::New(BuiltinModelId::SEARCH_RANKER_20190923);
    chromeos::machine_learning::ServiceConnection::GetInstance()
        ->GetMachineLearningService()
        .LoadBuiltinModel(std::move(spec), model_.BindNewPipeAndPassReceiver(),
                          base::BindOnce(&LoadModelCallback));
  }

  if (!executor_) {
    // Get the graph executor.
    model_->CreateGraphExecutor(executor_.BindNewPipeAndPassReceiver(),
                                base::BindOnce(&CreateGraphExecutorCallback));
    executor_.set_disconnect_handler(base::BindOnce(
        &SearchRankingEventLogger::OnConnectionError, base::Unretained(this)));
  }
}

void SearchRankingEventLogger::OnConnectionError() {
  LOG(WARNING) << "Mojo connection for ML service closed.";
  executor_.reset();
  model_.reset();
}

void SearchRankingEventLogger::ExecuteCallback(
    const std::string& id,
    ExecuteResult result,
    const base::Optional<std::vector<TensorPtr>> outputs) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (result != ExecuteResult::OK) {
    LOG(ERROR) << "Search Ranker inference execution failed.";
    LogInferenceError(InferenceError::kInferenceExecutionFailed);
    return;
  }
  prediction_[id] = outputs.value()[0]->data->get_float_list()->value[0];
}

}  // namespace app_list
