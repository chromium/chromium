// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/entity_annotator_native_library.h"

#include "base/base_paths.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/path_service.h"
#include "build/build_config.h"
#include "components/optimization_guide/core/model_util.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/optimization_guide/proto/page_entities_model_metadata.pb.h"

#if BUILDFLAG(IS_MAC)
#include "base/mac/bundle_locations.h"
#include "base/mac/foundation_util.h"
#endif

// IMPORTANT: All functions in this file that call dlsym()'ed
// functions should be annotated with DISABLE_CFI_ICALL.

namespace optimization_guide {

namespace {

const char kModelMetadataBaseName[] = "model_metadata.pb";
const char kWordEmbeddingsBaseName[] = "word_embeddings";
const char kNameTableBaseName[] = "entities_names";
const char kMetadataTableBaseName[] = "entities_metadata";
const char kNameFilterBaseName[] = "entities_names_filter";
const char kPrefixFilterBaseName[] = "entities_prefixes_filter";

// Sets |field_to_set| with the full file path of |base_name|'s entry in
// |base_to_full_file_path|. Returns whether |base_name| is in
// |base_to_full_file_path|.
absl::optional<std::string> GetFilePathFromMap(
    const std::string& base_name,
    const base::flat_map<std::string, base::FilePath>& base_to_full_file_path) {
  auto it = base_to_full_file_path.find(base_name);
  return it == base_to_full_file_path.end()
             ? absl::nullopt
             : absl::make_optional(FilePathToString(it->second));
}

// Returns the expected base name for |slice|. Will be of the form
// |slice|-|base_name|.
std::string GetSliceBaseName(const std::string& slice,
                             const std::string& base_name) {
  return slice + "-" + base_name;
}

}  // namespace

EntityAnnotatorNativeLibrary::EntityAnnotatorNativeLibrary(
    base::NativeLibrary native_library)
    : native_library_(std::move(native_library)) {
  DCHECK(native_library_.is_valid());

  LoadFunctions();
}
EntityAnnotatorNativeLibrary::~EntityAnnotatorNativeLibrary() = default;

// static
std::unique_ptr<EntityAnnotatorNativeLibrary>
EntityAnnotatorNativeLibrary::Create() {
  base::FilePath base_dir;
#if BUILDFLAG(IS_MAC)
  if (base::mac::AmIBundled()) {
    base_dir = base::mac::FrameworkBundlePath().Append("Libraries");
  } else {
#endif
    if (!base::PathService::Get(base::DIR_MODULE, &base_dir)) {
      LOG(ERROR) << "Error getting app dir";
      return nullptr;
    }
#if BUILDFLAG(IS_MAC)
  }
#endif

  base::NativeLibraryLoadError error;
  base::NativeLibrary native_library = base::LoadNativeLibrary(
      base_dir.AppendASCII(
          base::GetNativeLibraryName("optimization_guide_internal")),
      &error);
  if (!native_library) {
    LOG(ERROR) << "Failed to initialize optimization guide internal: "
               << error.ToString();
    return nullptr;
  }

  std::unique_ptr<EntityAnnotatorNativeLibrary>
      entity_annotator_native_library =
          base::WrapUnique<EntityAnnotatorNativeLibrary>(
              new EntityAnnotatorNativeLibrary(std::move(native_library)));
  if (entity_annotator_native_library->IsValid()) {
    return entity_annotator_native_library;
  }
  LOG(ERROR) << "Could not find all required functions for optimization guide "
                "internal library";
  return nullptr;
}

DISABLE_CFI_ICALL
void EntityAnnotatorNativeLibrary::LoadFunctions() {
  get_max_supported_feature_flag_func_ =
      reinterpret_cast<GetMaxSupportedFeatureFlagFunc>(
          native_library_.GetFunctionPointer(
              "OptimizationGuideEntityAnnotatorGetMaxSupportedFeatureFlag"));

  create_from_options_func_ = reinterpret_cast<CreateFromOptionsFunc>(
      native_library_.GetFunctionPointer(
          "OptimizationGuideEntityAnnotatorCreateFromOptions"));
  get_creation_error_func_ =
      reinterpret_cast<GetCreationErrorFunc>(native_library_.GetFunctionPointer(
          "OptimizationGuideEntityAnnotatorGetCreationError"));
  delete_func_ =
      reinterpret_cast<DeleteFunc>(native_library_.GetFunctionPointer(
          "OptimizationGuideEntityAnnotatorDelete"));

  annotate_job_create_func_ = reinterpret_cast<AnnotateJobCreateFunc>(
      native_library_.GetFunctionPointer(
          "OptimizationGuideEntityAnnotatorAnnotateJobCreate"));
  annotate_job_delete_func_ = reinterpret_cast<AnnotateJobDeleteFunc>(
      native_library_.GetFunctionPointer(
          "OptimizationGuideEntityAnnotatorAnnotateJobDelete"));
  run_annotate_job_func_ =
      reinterpret_cast<RunAnnotateJobFunc>(native_library_.GetFunctionPointer(
          "OptimizationGuideEntityAnnotatorRunAnnotateJob"));
  annotate_get_output_metadata_at_index_func_ = reinterpret_cast<
      AnnotateGetOutputMetadataAtIndexFunc>(native_library_.GetFunctionPointer(
      "OptimizationGuideEntityAnnotatorAnnotateGetOutputMetadataAtIndex"));
  annotate_get_output_metadata_score_at_index_func_ =
      reinterpret_cast<AnnotateGetOutputMetadataScoreAtIndexFunc>(
          native_library_.GetFunctionPointer(
              "OptimizationGuideEntityAnnotatorAnnotateGetOutputMetadataScoreAt"
              "Index"));

  entity_metadata_job_create_func_ =
      reinterpret_cast<EntityMetadataJobCreateFunc>(
          native_library_.GetFunctionPointer(
              "OptimizationGuideEntityAnnotatorEntityMetadataJobCreate"));
  entity_metadata_job_delete_func_ =
      reinterpret_cast<EntityMetadataJobDeleteFunc>(
          native_library_.GetFunctionPointer(
              "OptimizationGuideEntityAnnotatorEntityMetadataJobDelete"));
  run_entity_metadata_job_func_ = reinterpret_cast<RunEntityMetadataJobFunc>(
      native_library_.GetFunctionPointer(
          "OptimizationGuideEntityAnnotatorRunEntityMetadataJob"));

  options_create_func_ =
      reinterpret_cast<OptionsCreateFunc>(native_library_.GetFunctionPointer(
          "OptimizationGuideEntityAnnotatorOptionsCreate"));
  options_set_model_file_path_func_ =
      reinterpret_cast<OptionsSetModelFilePathFunc>(
          native_library_.GetFunctionPointer(
              "OptimizationGuideEntityAnnotatorOptionsSetModelFilePath"));
  options_set_model_metadata_file_path_func_ = reinterpret_cast<
      OptionsSetModelMetadataFilePathFunc>(native_library_.GetFunctionPointer(
      "OptimizationGuideEntityAnnotatorOptionsSetModelMetadataFilePath"));
  options_set_word_embeddings_file_path_func_ = reinterpret_cast<
      OptionsSetWordEmbeddingsFilePathFunc>(native_library_.GetFunctionPointer(
      "OptimizationGuideEntityAnnotatorOptionsSetWordEmbeddingsFilePath"));
  options_add_model_slice_func_ = reinterpret_cast<OptionsAddModelSliceFunc>(
      native_library_.GetFunctionPointer(
          "OptimizationGuideEntityAnnotatorOptionsAddModelSlice"));
  options_delete_func_ =
      reinterpret_cast<OptionsDeleteFunc>(native_library_.GetFunctionPointer(
          "OptimizationGuideEntityAnnotatorOptionsDelete"));

  entity_metadata_get_entity_id_func_ =
      reinterpret_cast<EntityMetadataGetEntityIdFunc>(
          native_library_.GetFunctionPointer(
              "OptimizationGuideEntityMetadataGetEntityID"));
  entity_metadata_get_human_readable_name_func_ =
      reinterpret_cast<EntityMetadataGetHumanReadableNameFunc>(
          native_library_.GetFunctionPointer(
              "OptimizationGuideEntityMetadataGetHumanReadableName"));
  entity_metadata_get_human_readable_categories_count_func_ = reinterpret_cast<
      EntityMetadataGetHumanReadableCategoriesCountFunc>(
      native_library_.GetFunctionPointer(
          "OptimizationGuideEntityMetadataGetHumanReadableCategoriesCount"));
  entity_metadata_get_human_readable_category_name_at_index_func_ =
      reinterpret_cast<EntityMetadataGetHumanReadableCategoryNameAtIndexFunc>(
          native_library_.GetFunctionPointer(
              "OptimizationGuideEntityMetadataGetHumanReadableCategoryNameAtInd"
              "ex"));
  entity_metadata_get_human_readable_category_score_at_index_func_ =
      reinterpret_cast<EntityMetadataGetHumanReadableCategoryScoreAtIndexFunc>(
          native_library_.GetFunctionPointer(
              "OptimizationGuideEntityMetadataGetHumanReadableCategoryScoreAtIn"
              "dex"));
}

DISABLE_CFI_ICALL
bool EntityAnnotatorNativeLibrary::IsValid() const {
  return get_max_supported_feature_flag_func_ && create_from_options_func_ &&
         get_creation_error_func_ && delete_func_ &&
         annotate_job_create_func_ && annotate_job_delete_func_ &&
         run_annotate_job_func_ &&
         annotate_get_output_metadata_at_index_func_ &&
         annotate_get_output_metadata_score_at_index_func_ &&
         entity_metadata_job_create_func_ && entity_metadata_job_delete_func_ &&
         run_entity_metadata_job_func_ && options_create_func_ &&
         options_set_model_file_path_func_ &&
         options_set_model_metadata_file_path_func_ &&
         options_set_word_embeddings_file_path_func_ &&
         options_add_model_slice_func_ && options_delete_func_ &&
         entity_metadata_get_entity_id_func_ &&
         entity_metadata_get_human_readable_name_func_ &&
         entity_metadata_get_human_readable_categories_count_func_ &&
         entity_metadata_get_human_readable_category_name_at_index_func_ &&
         entity_metadata_get_human_readable_category_score_at_index_func_;
}

DISABLE_CFI_ICALL
int32_t EntityAnnotatorNativeLibrary::GetMaxSupportedFeatureFlag() {
  DCHECK(IsValid());
  if (!IsValid()) {
    return -1;
  }

  return get_max_supported_feature_flag_func_();
}

DISABLE_CFI_ICALL
void* EntityAnnotatorNativeLibrary::CreateEntityAnnotator(
    const ModelInfo& model_info) {
  DCHECK(IsValid());
  if (!IsValid()) {
    return nullptr;
  }

  void* options = options_create_func_();
  if (!PopulateEntityAnnotatorOptionsFromModelInfo(options, model_info)) {
    options_delete_func_(options);
    return nullptr;
  }

  void* entity_annotator = create_from_options_func_(options);
  const char* creation_error = get_creation_error_func_(entity_annotator);
  if (creation_error) {
    LOG(ERROR) << "Failed to create entity annotator: " << creation_error;
    DeleteEntityAnnotator(entity_annotator);
    entity_annotator = nullptr;
  }
  options_delete_func_(options);
  return entity_annotator;
}

DISABLE_CFI_ICALL
bool EntityAnnotatorNativeLibrary::PopulateEntityAnnotatorOptionsFromModelInfo(
    void* options,
    const ModelInfo& model_info) {
  // We don't know which files are intended for use if we don't have model
  // metadata, so return early.
  if (!model_info.GetModelMetadata()) {
    return false;
  }

  // // Validate the model metadata.
  absl::optional<proto::PageEntitiesModelMetadata> entities_model_metadata =
      ParsedAnyMetadata<proto::PageEntitiesModelMetadata>(
          model_info.GetModelMetadata().value());
  if (!entities_model_metadata) {
    return false;
  }
  if (entities_model_metadata->slice_size() == 0) {
    return false;
  }

  // Build the entity annotator options.
  options_set_model_file_path_func_(
      options, FilePathToString(model_info.GetModelFilePath()).c_str());

  // Attach the additional files required by the model.
  base::flat_map<std::string, base::FilePath> base_to_full_file_path;
  for (const auto& model_file : model_info.GetAdditionalFiles()) {
    base_to_full_file_path.insert(
        {FilePathToString(model_file.BaseName()), model_file});
  }
  absl::optional<std::string> model_metadata_file_path =
      GetFilePathFromMap(kModelMetadataBaseName, base_to_full_file_path);
  if (!model_metadata_file_path) {
    return false;
  }
  options_set_model_metadata_file_path_func_(options,
                                             model_metadata_file_path->c_str());
  absl::optional<std::string> word_embeddings_file_path =
      GetFilePathFromMap(kWordEmbeddingsBaseName, base_to_full_file_path);
  if (!word_embeddings_file_path) {
    return false;
  }
  options_set_word_embeddings_file_path_func_(
      options, word_embeddings_file_path->c_str());

  base::flat_set<std::string> slices(entities_model_metadata->slice().begin(),
                                     entities_model_metadata->slice().end());
  for (const auto& slice_id : slices) {
    absl::optional<std::string> name_filter_path;
    if (features::ShouldProvideFilterPathForPageEntitiesModel()) {
      name_filter_path =
          GetFilePathFromMap(GetSliceBaseName(slice_id, kNameFilterBaseName),
                             base_to_full_file_path);
      if (!name_filter_path) {
        return false;
      }
    }
    absl::optional<std::string> name_table_path = GetFilePathFromMap(
        GetSliceBaseName(slice_id, kNameTableBaseName), base_to_full_file_path);
    if (!name_table_path) {
      return false;
    }
    absl::optional<std::string> prefix_filter_path;
    if (features::ShouldProvideFilterPathForPageEntitiesModel()) {
      prefix_filter_path =
          GetFilePathFromMap(GetSliceBaseName(slice_id, kPrefixFilterBaseName),
                             base_to_full_file_path);
      if (!prefix_filter_path) {
        return false;
      }
    }
    absl::optional<std::string> metadata_table_path =
        GetFilePathFromMap(GetSliceBaseName(slice_id, kMetadataTableBaseName),
                           base_to_full_file_path);
    if (!metadata_table_path) {
      return false;
    }
    options_add_model_slice_func_(
        options, slice_id.c_str(), name_filter_path.value_or("").c_str(),
        name_table_path->c_str(), prefix_filter_path.value_or("").c_str(),
        metadata_table_path->c_str());
  }

  return true;
}

DISABLE_CFI_ICALL
void EntityAnnotatorNativeLibrary::DeleteEntityAnnotator(
    void* entity_annotator) {
  DCHECK(IsValid());
  if (!IsValid()) {
    return;
  }

  delete_func_(reinterpret_cast<void*>(entity_annotator));
}

DISABLE_CFI_ICALL
absl::optional<std::vector<ScoredEntityMetadata>>
EntityAnnotatorNativeLibrary::AnnotateText(void* annotator,
                                           const std::string& text) {
  DCHECK(IsValid());
  if (!IsValid()) {
    return absl::nullopt;
  }

  if (!annotator) {
    return absl::nullopt;
  }

  void* job = annotate_job_create_func_(reinterpret_cast<void*>(annotator));
  int32_t output_metadata_count = run_annotate_job_func_(job, text.c_str());
  if (output_metadata_count <= 0) {
    return absl::nullopt;
  }
  std::vector<ScoredEntityMetadata> scored_md;
  scored_md.reserve(output_metadata_count);
  for (int32_t i = 0; i < output_metadata_count; i++) {
    ScoredEntityMetadata md;
    md.score = annotate_get_output_metadata_score_at_index_func_(job, i);
    md.metadata = GetEntityMetadataFromOptimizationGuideEntityMetadata(
        annotate_get_output_metadata_at_index_func_(job, i));
    scored_md.emplace_back(md);
  }
  annotate_job_delete_func_(job);
  return scored_md;
}

DISABLE_CFI_ICALL
absl::optional<EntityMetadata>
EntityAnnotatorNativeLibrary::GetEntityMetadataForEntityId(
    void* annotator,
    const std::string& entity_id) {
  DCHECK(IsValid());
  if (!IsValid()) {
    return absl::nullopt;
  }
  if (!annotator) {
    return absl::nullopt;
  }

  void* job =
      entity_metadata_job_create_func_(reinterpret_cast<void*>(annotator));
  const void* entity_metadata =
      run_entity_metadata_job_func_(job, entity_id.c_str());
  if (!entity_metadata) {
    return absl::nullopt;
  }
  EntityMetadata md =
      GetEntityMetadataFromOptimizationGuideEntityMetadata(entity_metadata);
  entity_metadata_job_delete_func_(job);
  return md;
}

DISABLE_CFI_ICALL
EntityMetadata EntityAnnotatorNativeLibrary::
    GetEntityMetadataFromOptimizationGuideEntityMetadata(
        const void* og_entity_metadata) {
  EntityMetadata entity_metadata;
  entity_metadata.entity_id =
      entity_metadata_get_entity_id_func_(og_entity_metadata);
  entity_metadata.human_readable_name =
      entity_metadata_get_human_readable_name_func_(og_entity_metadata);

  int32_t human_readable_categories_count =
      entity_metadata_get_human_readable_categories_count_func_(
          og_entity_metadata);
  for (int32_t i = 0; i < human_readable_categories_count; i++) {
    std::string category_name =
        entity_metadata_get_human_readable_category_name_at_index_func_(
            og_entity_metadata, i);
    float category_score =
        entity_metadata_get_human_readable_category_score_at_index_func_(
            og_entity_metadata, i);
    entity_metadata.human_readable_categories[category_name] = category_score;
  }
  return entity_metadata;
}

}  // namespace optimization_guide