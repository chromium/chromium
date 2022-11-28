// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/entity_annotator_native_library.h"

#include "base/base_paths.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/path_service.h"
#include "build/build_config.h"
#include "components/optimization_guide/core/model_util.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/optimization_guide/proto/page_entities_model_metadata.pb.h"

#if BUILDFLAG(IS_MAC)
#include "base/mac/bundle_locations.h"
#include "base/mac/foundation_util.h"
#endif

// IMPORTANT: All functions in this file that call dlsym()'ed
// functions should be annotated with DISABLE_CFI_DLSYM.

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

class ScopedEntityAnnotatorCreationStatusRecorder {
 public:
  ScopedEntityAnnotatorCreationStatusRecorder() = default;
  ~ScopedEntityAnnotatorCreationStatusRecorder() {
    DCHECK_NE(status_, EntityAnnotatorCreationStatus::kUnknown);
    base::UmaHistogramEnumeration(
        "OptimizationGuide.PageEntitiesModelHandler.CreationStatus", status_);
  }

  void set_status(EntityAnnotatorCreationStatus status) { status_ = status; }

 private:
  EntityAnnotatorCreationStatus status_ =
      EntityAnnotatorCreationStatus::kUnknown;
};

}  // namespace

EntityAnnotatorNativeLibrary::EntityAnnotatorNativeLibrary(
    base::NativeLibrary native_library,
    bool should_provide_filter_path)
    : native_library_(std::move(native_library)),
      should_provide_filter_path_(should_provide_filter_path) {
  LoadFunctions();
}
EntityAnnotatorNativeLibrary::~EntityAnnotatorNativeLibrary() = default;

// static
std::unique_ptr<EntityAnnotatorNativeLibrary>
EntityAnnotatorNativeLibrary::Create(bool should_provide_filter_path) {
  base::FilePath base_dir;
#if !BUILDFLAG(IS_ANDROID)
#if BUILDFLAG(IS_MAC)
  if (base::mac::AmIBundled()) {
    base_dir = base::mac::FrameworkBundlePath().Append("Libraries");
  } else {
#endif  // BUILDFLAG(IS_MAC)
    if (!base::PathService::Get(base::DIR_MODULE, &base_dir)) {
      LOG(ERROR) << "Error getting app dir";
      return nullptr;
    }
#if BUILDFLAG(IS_MAC)
  }
#endif  // BUILDFLAG(IS_MAC)
#endif  // !BUILDFLAG(IS_ANDROID)

  base::NativeLibraryLoadError error;
  base::NativeLibrary native_library = base::LoadNativeLibrary(
      base_dir.AppendASCII(
          base::GetNativeLibraryName("optimization_guide_internal")),
      &error);
  base::UmaHistogramBoolean(
      "OptimizationGuide.EntityAnnotatorNativeLibrary.InitiatedSuccessfully",
      (native_library != nullptr));
  if (!native_library) {
    LOG(ERROR) << "Failed to initialize optimization guide internal: "
               << error.ToString();
    return nullptr;
  }

  std::unique_ptr<EntityAnnotatorNativeLibrary>
      entity_annotator_native_library =
          base::WrapUnique<EntityAnnotatorNativeLibrary>(
              new EntityAnnotatorNativeLibrary(std::move(native_library),
                                               should_provide_filter_path));
  if (entity_annotator_native_library->IsValid()) {
    return entity_annotator_native_library;
  }
  LOG(ERROR) << "Could not find all required functions for optimization guide "
                "internal library";
  return nullptr;
}

DISABLE_CFI_DLSYM
void EntityAnnotatorNativeLibrary::LoadFunctions() {
  get_max_supported_feature_flag_func_ =
      reinterpret_cast<GetMaxSupportedFeatureFlagFunc>(
          base::GetFunctionPointerFromNativeLibrary(
              native_library_,
              "OptimizationGuideEntityAnnotatorGetMaxSupportedFeatureFlag"));

  create_from_options_func_ = reinterpret_cast<CreateFromOptionsFunc>(
      base::GetFunctionPointerFromNativeLibrary(
          native_library_,
          "OptimizationGuideEntityAnnotatorCreateFromOptions"));
  get_creation_error_func_ = reinterpret_cast<GetCreationErrorFunc>(
      base::GetFunctionPointerFromNativeLibrary(
          native_library_, "OptimizationGuideEntityAnnotatorGetCreationError"));
  delete_func_ =
      reinterpret_cast<DeleteFunc>(base::GetFunctionPointerFromNativeLibrary(
          native_library_, "OptimizationGuideEntityAnnotatorDelete"));

  annotate_job_create_func_ = reinterpret_cast<AnnotateJobCreateFunc>(
      base::GetFunctionPointerFromNativeLibrary(
          native_library_,
          "OptimizationGuideEntityAnnotatorAnnotateJobCreate"));
  annotate_job_delete_func_ = reinterpret_cast<AnnotateJobDeleteFunc>(
      base::GetFunctionPointerFromNativeLibrary(
          native_library_,
          "OptimizationGuideEntityAnnotatorAnnotateJobDelete"));
  run_annotate_job_func_ = reinterpret_cast<RunAnnotateJobFunc>(
      base::GetFunctionPointerFromNativeLibrary(
          native_library_, "OptimizationGuideEntityAnnotatorRunAnnotateJob"));
  annotate_get_output_metadata_at_index_func_ = reinterpret_cast<
      AnnotateGetOutputMetadataAtIndexFunc>(
      base::GetFunctionPointerFromNativeLibrary(
          native_library_,
          "OptimizationGuideEntityAnnotatorAnnotateGetOutputMetadataAtIndex"));
  annotate_get_output_metadata_score_at_index_func_ =
      reinterpret_cast<AnnotateGetOutputMetadataScoreAtIndexFunc>(
          base::GetFunctionPointerFromNativeLibrary(
              native_library_,
              "OptimizationGuideEntityAnnotatorAnnotateGetOutputMetadataScoreAt"
              "Index"));

  entity_metadata_job_create_func_ =
      reinterpret_cast<EntityMetadataJobCreateFunc>(
          base::GetFunctionPointerFromNativeLibrary(
              native_library_,
              "OptimizationGuideEntityAnnotatorEntityMetadataJobCreate"));
  entity_metadata_job_delete_func_ =
      reinterpret_cast<EntityMetadataJobDeleteFunc>(
          base::GetFunctionPointerFromNativeLibrary(
              native_library_,
              "OptimizationGuideEntityAnnotatorEntityMetadataJobDelete"));
  run_entity_metadata_job_func_ = reinterpret_cast<RunEntityMetadataJobFunc>(
      base::GetFunctionPointerFromNativeLibrary(
          native_library_,
          "OptimizationGuideEntityAnnotatorRunEntityMetadataJob"));

  options_create_func_ = reinterpret_cast<OptionsCreateFunc>(
      base::GetFunctionPointerFromNativeLibrary(
          native_library_, "OptimizationGuideEntityAnnotatorOptionsCreate"));
  options_set_model_file_path_func_ =
      reinterpret_cast<OptionsSetModelFilePathFunc>(
          base::GetFunctionPointerFromNativeLibrary(
              native_library_,
              "OptimizationGuideEntityAnnotatorOptionsSetModelFilePath"));
  options_set_model_metadata_file_path_func_ = reinterpret_cast<
      OptionsSetModelMetadataFilePathFunc>(
      base::GetFunctionPointerFromNativeLibrary(
          native_library_,
          "OptimizationGuideEntityAnnotatorOptionsSetModelMetadataFilePath"));
  options_set_word_embeddings_file_path_func_ = reinterpret_cast<
      OptionsSetWordEmbeddingsFilePathFunc>(
      base::GetFunctionPointerFromNativeLibrary(
          native_library_,
          "OptimizationGuideEntityAnnotatorOptionsSetWordEmbeddingsFilePath"));
  options_add_model_slice_func_ = reinterpret_cast<OptionsAddModelSliceFunc>(
      base::GetFunctionPointerFromNativeLibrary(
          native_library_,
          "OptimizationGuideEntityAnnotatorOptionsAddModelSlice"));
  options_delete_func_ = reinterpret_cast<OptionsDeleteFunc>(
      base::GetFunctionPointerFromNativeLibrary(
          native_library_, "OptimizationGuideEntityAnnotatorOptionsDelete"));

  entity_metadata_get_entity_id_func_ =
      reinterpret_cast<EntityMetadataGetEntityIdFunc>(
          base::GetFunctionPointerFromNativeLibrary(
              native_library_, "OptimizationGuideEntityMetadataGetEntityID"));
  entity_metadata_get_human_readable_name_func_ =
      reinterpret_cast<EntityMetadataGetHumanReadableNameFunc>(
          base::GetFunctionPointerFromNativeLibrary(
              native_library_,
              "OptimizationGuideEntityMetadataGetHumanReadableName"));
  entity_metadata_get_human_readable_categories_count_func_ = reinterpret_cast<
      EntityMetadataGetHumanReadableCategoriesCountFunc>(
      base::GetFunctionPointerFromNativeLibrary(
          native_library_,
          "OptimizationGuideEntityMetadataGetHumanReadableCategoriesCount"));
  entity_metadata_get_human_readable_category_name_at_index_func_ =
      reinterpret_cast<EntityMetadataGetHumanReadableCategoryNameAtIndexFunc>(
          base::GetFunctionPointerFromNativeLibrary(
              native_library_,
              "OptimizationGuideEntityMetadataGetHumanReadableCategoryNameAtInd"
              "ex"));
  entity_metadata_get_human_readable_category_score_at_index_func_ =
      reinterpret_cast<EntityMetadataGetHumanReadableCategoryScoreAtIndexFunc>(
          base::GetFunctionPointerFromNativeLibrary(
              native_library_,
              "OptimizationGuideEntityMetadataGetHumanReadableCategoryScoreAtIn"
              "dex"));
  entity_metadata_get_human_readable_aliases_count_func_ =
      reinterpret_cast<EntityMetadataGetHumanReadableAliasesCountFunc>(
          base::GetFunctionPointerFromNativeLibrary(
              native_library_,
              "OptimizationGuideEntityMetadataGetHumanReadableAliasesCount"));
  entity_metadata_get_human_readable_alias_at_index_func_ =
      reinterpret_cast<EntityMetadataGetHumanReadableAliasAtIndexFunc>(
          base::GetFunctionPointerFromNativeLibrary(
              native_library_,
              "OptimizationGuideEntityMetadataGetHumanReadableAliasAtIndex"));
  entity_metadata_get_collections_count_func_ =
      reinterpret_cast<EntityMetadataGetCollectionsCountFunc>(
          base::GetFunctionPointerFromNativeLibrary(
              native_library_,
              "OptimizationGuideEntityMetadataGetCollectionsCount"));
  entity_metadata_get_collection_at_index_func_ =
      reinterpret_cast<EntityMetadataGetCollectionAtIndexFunc>(
          base::GetFunctionPointerFromNativeLibrary(
              native_library_,
              "OptimizationGuideEntityMetadataGetCollectionAtIndex"));
}

DISABLE_CFI_DLSYM
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
         entity_metadata_get_human_readable_category_score_at_index_func_ &&
         entity_metadata_get_human_readable_aliases_count_func_ &&
         entity_metadata_get_human_readable_alias_at_index_func_ &&
         entity_metadata_get_collections_count_func_ &&
         entity_metadata_get_collection_at_index_func_;
}

DISABLE_CFI_DLSYM
int32_t EntityAnnotatorNativeLibrary::GetMaxSupportedFeatureFlag() {
  DCHECK(IsValid());
  if (!IsValid()) {
    return -1;
  }

  return get_max_supported_feature_flag_func_();
}

DISABLE_CFI_DLSYM
void* EntityAnnotatorNativeLibrary::CreateEntityAnnotator(
    const ModelInfo& model_info) {
  ScopedEntityAnnotatorCreationStatusRecorder recorder;
  DCHECK(IsValid());
  if (!IsValid()) {
    recorder.set_status(EntityAnnotatorCreationStatus::kLibraryInvalid);
    return nullptr;
  }

  void* options = options_create_func_();
  EntityAnnotatorCreationStatus status;
  if (!PopulateEntityAnnotatorOptionsFromModelInfo(options, model_info,
                                                   &status)) {
    options_delete_func_(options);
    recorder.set_status(status);
    return nullptr;
  }

  void* entity_annotator = create_from_options_func_(options);
  const char* creation_error = get_creation_error_func_(entity_annotator);
  if (creation_error) {
    LOG(ERROR) << "Failed to create entity annotator: " << creation_error;
    recorder.set_status(EntityAnnotatorCreationStatus::kInitializationFailure);
    DeleteEntityAnnotator(entity_annotator);
    entity_annotator = nullptr;
  } else {
    recorder.set_status(EntityAnnotatorCreationStatus::kSuccess);
  }

  options_delete_func_(options);
  return entity_annotator;
}

DISABLE_CFI_DLSYM
bool EntityAnnotatorNativeLibrary::PopulateEntityAnnotatorOptionsFromModelInfo(
    void* options,
    const ModelInfo& model_info,
    EntityAnnotatorCreationStatus* status) {
  // We don't know which files are intended for use if we don't have model
  // metadata, so return early.
  if (!model_info.GetModelMetadata()) {
    *status = EntityAnnotatorCreationStatus::kMissingModelMetadata;
    return false;
  }

  // // Validate the model metadata.
  absl::optional<proto::PageEntitiesModelMetadata> entities_model_metadata =
      ParsedAnyMetadata<proto::PageEntitiesModelMetadata>(
          model_info.GetModelMetadata().value());
  if (!entities_model_metadata) {
    *status = EntityAnnotatorCreationStatus::kMissingEntitiesModelMetadata;
    return false;
  }
  if (entities_model_metadata->slice_size() == 0) {
    *status = EntityAnnotatorCreationStatus::
        kMissingEntitiesModelMetadataSliceSpecification;
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
    *status = EntityAnnotatorCreationStatus::
        kMissingAdditionalEntitiesModelMetadataPath;
    return false;
  }
  options_set_model_metadata_file_path_func_(options,
                                             model_metadata_file_path->c_str());
  absl::optional<std::string> word_embeddings_file_path =
      GetFilePathFromMap(kWordEmbeddingsBaseName, base_to_full_file_path);
  if (!word_embeddings_file_path) {
    *status =
        EntityAnnotatorCreationStatus::kMissingAdditionalWordEmbeddingsPath;
    return false;
  }
  options_set_word_embeddings_file_path_func_(
      options, word_embeddings_file_path->c_str());

  base::flat_set<std::string> slices(entities_model_metadata->slice().begin(),
                                     entities_model_metadata->slice().end());
  for (const auto& slice_id : slices) {
    absl::optional<std::string> name_filter_path;
    if (should_provide_filter_path_) {
      name_filter_path =
          GetFilePathFromMap(GetSliceBaseName(slice_id, kNameFilterBaseName),
                             base_to_full_file_path);
      if (!name_filter_path) {
        *status =
            EntityAnnotatorCreationStatus::kMissingAdditionalNameFilterPath;
        return false;
      }
    }
    absl::optional<std::string> name_table_path = GetFilePathFromMap(
        GetSliceBaseName(slice_id, kNameTableBaseName), base_to_full_file_path);
    if (!name_table_path) {
      *status = EntityAnnotatorCreationStatus::kMissingAdditionalNameTablePath;
      return false;
    }
    absl::optional<std::string> prefix_filter_path;
    if (should_provide_filter_path_) {
      prefix_filter_path =
          GetFilePathFromMap(GetSliceBaseName(slice_id, kPrefixFilterBaseName),
                             base_to_full_file_path);
      if (!prefix_filter_path) {
        *status =
            EntityAnnotatorCreationStatus::kMissingAdditionalPrefixFilterPath;
        return false;
      }
    }
    absl::optional<std::string> metadata_table_path =
        GetFilePathFromMap(GetSliceBaseName(slice_id, kMetadataTableBaseName),
                           base_to_full_file_path);
    if (!metadata_table_path) {
      *status =
          EntityAnnotatorCreationStatus::kMissingAdditionalMetadataTablePath;
      return false;
    }
    options_add_model_slice_func_(
        options, slice_id.c_str(), name_filter_path.value_or("").c_str(),
        name_table_path->c_str(), prefix_filter_path.value_or("").c_str(),
        metadata_table_path->c_str());
  }

  return true;
}

DISABLE_CFI_DLSYM
void EntityAnnotatorNativeLibrary::DeleteEntityAnnotator(
    void* entity_annotator) {
  DCHECK(IsValid());
  if (!IsValid()) {
    return;
  }

  delete_func_(reinterpret_cast<void*>(entity_annotator));
}

DISABLE_CFI_DLSYM
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

DISABLE_CFI_DLSYM
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

DISABLE_CFI_DLSYM
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

  int32_t human_readable_aliases_count =
      entity_metadata_get_human_readable_aliases_count_func_(
          og_entity_metadata);
  for (int32_t i = 0; i < human_readable_aliases_count; i++) {
    entity_metadata.human_readable_aliases.push_back(
        entity_metadata_get_human_readable_alias_at_index_func_(
            og_entity_metadata, i));
  }

  int32_t collections_count =
      entity_metadata_get_collections_count_func_(og_entity_metadata);
  for (int32_t i = 0; i < collections_count; i++) {
    std::string collection =
        entity_metadata_get_collection_at_index_func_(og_entity_metadata, i);
    entity_metadata.collections.push_back(collection);
  }
  return entity_metadata;
}

}  // namespace optimization_guide
