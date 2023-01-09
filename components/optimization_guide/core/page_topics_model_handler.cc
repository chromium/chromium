// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/page_topics_model_handler.h"

#include <ctype.h>

#include "base/barrier_closure.h"
#include "base/containers/contains.h"
#include "base/files/file_util.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/task/sequenced_task_runner.h"
#include "components/optimization_guide/core/optimization_guide_model_provider.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "components/optimization_guide/proto/page_topics_model_metadata.pb.h"
#include "components/optimization_guide/proto/page_topics_override_list.pb.h"
#include "third_party/zlib/google/compression_utils.h"

namespace optimization_guide {

namespace {

// The ID of the NONE category in the taxonomy. This node always exists.
// Semantically, the none category is attached to data for which we can say
// with certainty that no single label in the taxonomy is appropriate.
const int32_t kNoneCategoryId = -2;

// The |kMeaninglessPrefixV2MinVersion| needed to support meaningless prefix v2.
// This should be compared with the version provided the model metadata.
const int32_t kMeaninglessPrefixV2MinVersion = 2;

const base::FilePath::CharType kOverrideListBasePath[] =
    FILE_PATH_LITERAL("override_list.pb.gz");

// The result of an override list file load attempt. These values are logged to
// UMA histograms, do not change or reorder values. Make sure to update
// |OptimizationGuidePageTopicsOverrideListFileLoadResult| in
// //tools/metrics/histograms/enums.xml.
enum class OverrideListFileLoadResult {
  kUnknown = 0,
  kSuccess = 1,
  kCouldNotReadFile = 2,
  kCouldNotUncompressFile = 3,
  kCouldNotUnmarshalProtobuf = 4,
  kMaxValue = kCouldNotUnmarshalProtobuf,
};

void RecordOverrideListFileLoadResult(OverrideListFileLoadResult result) {
  base::UmaHistogramEnumeration(
      "OptimizationGuide.PageTopicsOverrideList.FileLoadResult", result);
}

absl::optional<std::unordered_map<std::string, std::vector<WeightedIdentifier>>>
LoadOverrideListFromFile(const base::FilePath& path) {
  if (!path.IsAbsolute() ||
      path.BaseName() != base::FilePath(kOverrideListBasePath)) {
    NOTREACHED();
    // This is enforced by calling code, so no UMA in this case.
    return absl::nullopt;
  }

  std::string file_contents;
  if (!base::ReadFileToString(path, &file_contents)) {
    RecordOverrideListFileLoadResult(
        OverrideListFileLoadResult::kCouldNotReadFile);
    return absl::nullopt;
  }

  if (!compression::GzipUncompress(file_contents, &file_contents)) {
    RecordOverrideListFileLoadResult(
        OverrideListFileLoadResult::kCouldNotUncompressFile);
    return absl::nullopt;
  }

  proto::PageTopicsOverrideList override_list_pb;
  if (!override_list_pb.ParseFromString(file_contents)) {
    RecordOverrideListFileLoadResult(
        OverrideListFileLoadResult::kCouldNotUnmarshalProtobuf);
    return absl::nullopt;
  }

  std::unordered_map<std::string, std::vector<WeightedIdentifier>>
      override_list;
  for (const proto::PageTopicsOverrideEntry& entry :
       override_list_pb.entries()) {
    std::vector<WeightedIdentifier> topics;
    topics.reserve(entry.topics().topic_ids_size());
    for (int32_t topic : entry.topics().topic_ids()) {
      // Always give overridden topics full weight.
      topics.emplace_back(WeightedIdentifier(topic, 1.0));
    }
    override_list.emplace(entry.domain(), std::move(topics));
  }

  RecordOverrideListFileLoadResult(OverrideListFileLoadResult::kSuccess);
  return override_list;
}

// Returns the length of the leading meaningless prefix of a host name as
// defined for the Topics Model.
//
// The full list of meaningless prefixes are:
//   ^(www[0-9]*|web|ftp|wap|home)$
//   ^(m|mobile|amp|w)$
int MeaninglessPrefixLength(const std::string& host) {
  size_t len = host.size();

  int dots = base::ranges::count(host, '.');
  if (dots < 2) {
    return 0;
  }

  if (len > 4 && base::StartsWith(host, "www")) {
    // Check that all characters after "www" and up to first "." are
    // digits.
    for (size_t i = 3; i < len; ++i) {
      if (host[i] == '.') {
        return i + 1;
      }
      if (!isdigit(host[i])) {
        return 0;
      }
    }
  } else {
    static const auto* kMeaninglessPrefixesLenMap = new std::set<std::string>(
        {"web", "ftp", "wap", "home", "m", "w", "amp", "mobile"});

    size_t prefix_len = host.find('.');
    std::string prefix = host.substr(0, prefix_len);
    const auto& it = kMeaninglessPrefixesLenMap->find(prefix);
    if (it != kMeaninglessPrefixesLenMap->end() && len > it->size() + 1) {
      return it->size() + 1;
    }
  }
  return 0;
}

}  // namespace

PageTopicsModelHandler::PageTopicsModelHandler(
    OptimizationGuideModelProvider* model_provider,
    scoped_refptr<base::SequencedTaskRunner> background_task_runner,
    const absl::optional<proto::Any>& model_metadata)
    : BertModelHandler(model_provider,
                       background_task_runner,
                       proto::OPTIMIZATION_TARGET_PAGE_TOPICS_V2,
                       model_metadata),
      background_task_runner_(background_task_runner) {
  SetShouldUnloadModelOnComplete(false);
}
PageTopicsModelHandler::~PageTopicsModelHandler() = default;

void PageTopicsModelHandler::ExecuteJob(
    base::OnceClosure on_job_complete_callback,
    std::unique_ptr<PageContentAnnotationJob> job) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(job->type(), AnnotationType::kPageTopics);

  // Check if there is an override list available but not loaded yet.
  if (override_list_file_path_ && !override_list_) {
    background_task_runner_->PostTaskAndReplyWithResult(
        FROM_HERE,
        base::BindOnce(&LoadOverrideListFromFile, *override_list_file_path_),
        base::BindOnce(&PageTopicsModelHandler::OnOverrideListLoadAttemptDone,
                       weak_ptr_factory_.GetWeakPtr(),
                       std::move(on_job_complete_callback), std::move(job)));
    return;
  }

  PageContentAnnotationJobExecutor::ExecuteJob(
      std::move(on_job_complete_callback), std::move(job));
}

std::string PageTopicsModelHandler::PreprocessHost(
    const std::string& host) const {
  std::string output = base::ToLowerASCII(host);

  // Meaningless prefix v2 is only supported/required for
  // |kMeaninglessPrefixV2MinVersion| and on.
  if (version_ >= kMeaninglessPrefixV2MinVersion) {
    int idx = MeaninglessPrefixLength(output);
    if (idx > 0) {
      output = output.substr(idx);
    }
  } else {
    // Strip the 'www.' if it exists.
    if (base::StartsWith(output, "www.")) {
      output = output.substr(4);
    }
  }

  static const char kCharsToReplaceWithSpace[] = {'-', '_', '.', '+'};
  for (char c : kCharsToReplaceWithSpace) {
    std::replace(output.begin(), output.end(), c, ' ');
  }

  return output;
}

void PageTopicsModelHandler::ExecuteOnSingleInput(
    AnnotationType annotation_type,
    const std::string& raw_input,
    base::OnceCallback<void(const BatchAnnotationResult&)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(annotation_type, AnnotationType::kPageTopics);

  // |processed_input| is needed by the override list and the model, but we pass
  // the |raw_input| to where the BatchAnnotationResult is created so that the
  // original input is passed back to the caller.
  std::string processed_input = PreprocessHost(raw_input);

  if (override_list_) {
    DCHECK(override_list_file_path_);
    auto iter = override_list_->find(processed_input);

    base::UmaHistogramBoolean(
        "OptimizationGuide.PageTopicsOverrideList.UsedOverride",
        iter != override_list_->end());

    if (iter != override_list_->end()) {
      std::move(callback).Run(BatchAnnotationResult::CreatePageTopicsResult(
          raw_input, iter->second));
      return;
    }
  }

  ExecuteModelWithInput(
      base::BindOnce(
          &PageTopicsModelHandler::PostprocessCategoriesToBatchAnnotationResult,
          weak_ptr_factory_.GetWeakPtr(), std::move(callback), annotation_type,
          raw_input),
      processed_input);
}

void PageTopicsModelHandler::OnOverrideListLoadAttemptDone(
    base::OnceClosure on_job_complete_callback,
    std::unique_ptr<PageContentAnnotationJob> job,
    absl::optional<
        std::unordered_map<std::string, std::vector<WeightedIdentifier>>>
        override_list) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  override_list_ = override_list;
  if (!override_list) {
    // Clear the file path so we don't try to load it again.
    override_list_file_path_ = absl::nullopt;
  }

  // Now we're ready to run the job! Call the base class to do so.
  PageContentAnnotationJobExecutor::ExecuteJob(
      std::move(on_job_complete_callback), std::move(job));
}

void PageTopicsModelHandler::PostprocessCategoriesToBatchAnnotationResult(
    base::OnceCallback<void(const BatchAnnotationResult&)> callback,
    AnnotationType annotation_type,
    const std::string& raw_input,
    const absl::optional<std::vector<tflite::task::core::Category>>& output) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(annotation_type, AnnotationType::kPageTopics);

  absl::optional<std::vector<WeightedIdentifier>> categories;
  if (output) {
    categories = ExtractCategoriesFromModelOutput(*output);
  }
  std::move(callback).Run(
      BatchAnnotationResult::CreatePageTopicsResult(raw_input, categories));
}

absl::optional<std::vector<WeightedIdentifier>>
PageTopicsModelHandler::ExtractCategoriesFromModelOutput(
    const std::vector<tflite::task::core::Category>& model_output) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  absl::optional<proto::PageTopicsModelMetadata> model_metadata =
      ParsedSupportedFeaturesForLoadedModel<proto::PageTopicsModelMetadata>();
  if (!model_metadata) {
    return absl::nullopt;
  }

  absl::optional<std::string> visibility_category_name =
      model_metadata->output_postprocessing_params().has_visibility_params() &&
              model_metadata->output_postprocessing_params()
                  .visibility_params()
                  .has_category_name()
          ? absl::make_optional(model_metadata->output_postprocessing_params()
                                    .visibility_params()
                                    .category_name())
          : absl::nullopt;

  std::vector<std::pair<int32_t, float>> category_candidates;

  for (const auto& category : model_output) {
    if (visibility_category_name &&
        category.class_name == *visibility_category_name) {
      continue;
    }
    // Assume everything else is for categories.
    int category_id;
    if (base::StringToInt(category.class_name, &category_id)) {
      category_candidates.emplace_back(
          std::make_pair(category_id, static_cast<float>(category.score)));
    }
  }

  // Postprocess categories.
  if (!model_metadata->output_postprocessing_params().has_category_params()) {
    // No parameters for postprocessing, so just return.
    return absl::nullopt;
  }
  const proto::PageTopicsCategoryPostprocessingParams category_params =
      model_metadata->output_postprocessing_params().category_params();

  // Determine the categories with the highest weights.
  std::sort(
      category_candidates.begin(), category_candidates.end(),
      [](const std::pair<int32_t, float>& a,
         const std::pair<int32_t, float>& b) { return a.second > b.second; });
  size_t max_categories = static_cast<size_t>(category_params.max_categories());
  float total_weight = 0.0;
  float sum_positive_scores = 0.0;
  absl::optional<std::pair<size_t, float>> none_idx_and_weight;
  std::vector<std::pair<int32_t, float>> categories;
  categories.reserve(max_categories);
  for (size_t i = 0; i < category_candidates.size() && i < max_categories;
       i++) {
    std::pair<int32_t, float> candidate = category_candidates[i];
    categories.push_back(candidate);
    total_weight += candidate.second;

    if (candidate.second > 0)
      sum_positive_scores += candidate.second;

    if (candidate.first == kNoneCategoryId) {
      none_idx_and_weight = std::make_pair(i, candidate.second);
    }
  }

  // Prune out categories that do not meet the minimum threshold.
  if (category_params.min_category_weight() > 0) {
    categories.erase(
        std::remove_if(categories.begin(), categories.end(),
                       [&](const std::pair<int32_t, float>& category) {
                         return category.second <
                                category_params.min_category_weight();
                       }),
        categories.end());
  }

  // Prune out none weights.
  if (total_weight == 0) {
    return absl::nullopt;
  }
  if (none_idx_and_weight) {
    if ((none_idx_and_weight->second / total_weight) >
        category_params.min_none_weight()) {
      // None weight is too strong.
      return absl::nullopt;
    }
    // None weight doesn't matter, so prune it out. Note that it may have
    // already been removed above if its weight was below the category min.
    categories.erase(
        std::remove_if(categories.begin(), categories.end(),
                       [&](const std::pair<int32_t, float>& category) {
                         return category.first == kNoneCategoryId;
                       }),
        categories.end());
  }

  // Normalize category weights.
  float normalization_factor =
      sum_positive_scores > 0 ? sum_positive_scores : 1.0;
  categories.erase(
      std::remove_if(
          categories.begin(), categories.end(),
          [&](const std::pair<int32_t, float>& category) {
            return (category.second / normalization_factor) <
                   category_params.min_normalized_weight_within_top_n();
          }),
      categories.end());

  std::vector<WeightedIdentifier> final_categories;
  final_categories.reserve(categories.size());
  for (const auto& category : categories) {
    // We expect the weight to be between 0 and 1.
    DCHECK(category.second >= 0.0 && category.second <= 1.0);
    final_categories.emplace_back(
        WeightedIdentifier(category.first, category.second));
  }
  DCHECK_LE(final_categories.size(), max_categories);

  return final_categories;
}

void PageTopicsModelHandler::UnloadModel() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  BertModelHandler::UnloadModel();
  override_list_ = absl::nullopt;
}

void PageTopicsModelHandler::OnModelUpdated(
    proto::OptimizationTarget optimization_target,
    const ModelInfo& model_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  BertModelHandler::OnModelUpdated(optimization_target, model_info);

  if (optimization_target != proto::OPTIMIZATION_TARGET_PAGE_TOPICS_V2) {
    return;
  }

  // New model, new override list.
  override_list_file_path_ = absl::nullopt;
  override_list_ = absl::nullopt;

  absl::optional<proto::PageTopicsModelMetadata> model_metadata =
      ParsedSupportedFeaturesForLoadedModel<proto::PageTopicsModelMetadata>();
  if (model_metadata) {
    version_ = model_metadata->version();
  }

  for (const base::FilePath& path : model_info.GetAdditionalFiles()) {
    DCHECK(path.IsAbsolute());
    if (path.BaseName() == base::FilePath(kOverrideListBasePath)) {
      override_list_file_path_ = path;
      break;
    }
  }

  base::UmaHistogramBoolean("OptimizationGuide.PageTopicsOverrideList.GotFile",
                            !!override_list_file_path_);
}

}  // namespace optimization_guide
