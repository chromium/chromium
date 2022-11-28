// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_ENTITY_ANNOTATOR_NATIVE_LIBRARY_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_ENTITY_ANNOTATOR_NATIVE_LIBRARY_H_

#include <memory>
#include <vector>

#include "base/native_library.h"
#include "components/optimization_guide/core/entity_metadata.h"
#include "components/optimization_guide/core/model_info.h"

namespace optimization_guide {

// Enumerates the statuses possible when creating an entity annotator.
//
// Keep this in sync with
// OptimizationGuideEntityAnnotatorCreationStatus in enums.xml.
enum class EntityAnnotatorCreationStatus {
  kUnknown = 0,
  // The entity annotator was created successfully.
  kSuccess = 1,
  // The native library was loaded but invalid. Should not happen in the real
  // world.
  kLibraryInvalid = 2,
  // The entity annotator was requested to be created but no metadata for how to
  // create it was present.
  kMissingModelMetadata = 3,
  // The entity annotator was requested to be created but the metadata specific
  // to this model was not present.
  kMissingEntitiesModelMetadata = 4,
  // The entity annotator was requested to be created but no slices were
  // specified in the model metadata.
  kMissingEntitiesModelMetadataSliceSpecification = 5,
  // Expected files are missing.
  kMissingAdditionalEntitiesModelMetadataPath = 6,
  kMissingAdditionalWordEmbeddingsPath = 7,
  kMissingAdditionalNameFilterPath = 8,
  kMissingAdditionalNameTablePath = 9,
  kMissingAdditionalPrefixFilterPath = 10,
  kMissingAdditionalMetadataTablePath = 11,
  // All required files were present, but the creation failed for a different
  // reason.
  kInitializationFailure = 12,

  // New values go above here.
  kMaxValue = kInitializationFailure,
};

// Handles interactions with the native library that contains logic for the
// entity annotator.
class EntityAnnotatorNativeLibrary {
 public:
  // Creates an EntityAnnotatorNativeLibrary, which loads a native library and
  // relevant functions required. Will return nullptr if fails.
  // |should_provide_filter_path| dictates whether the filters used to optimize
  // the annotation should be provided.
  static std::unique_ptr<EntityAnnotatorNativeLibrary> Create(
      bool should_provide_filter_path);

  EntityAnnotatorNativeLibrary(const EntityAnnotatorNativeLibrary&) = delete;
  EntityAnnotatorNativeLibrary& operator=(const EntityAnnotatorNativeLibrary&) =
      delete;
  ~EntityAnnotatorNativeLibrary();

  // Returns whether this instance is valid (i.e. all necessary functions have
  // been loaded.)
  bool IsValid() const;

  // Gets the max supported feature from this native library.
  int32_t GetMaxSupportedFeatureFlag();

  // Creates an entity annotator from |model_info|.
  void* CreateEntityAnnotator(const ModelInfo& model_info);

  // Deletes |entity_annotator|.
  void DeleteEntityAnnotator(void* entity_annotator);

  // Uses |annotator| to annotate entities present in |text|.
  absl::optional<std::vector<ScoredEntityMetadata>> AnnotateText(
      void* annotator,
      const std::string& text);

  // Returns entity metadata from |annotator| for |entity_id|.
  absl::optional<EntityMetadata> GetEntityMetadataForEntityId(
      void* annotator,
      const std::string& entity_id);

 private:
  EntityAnnotatorNativeLibrary(base::NativeLibrary native_library,
                               bool should_provide_filter_path);

  // Loads the functions exposed by the native library.
  void LoadFunctions();

  // Populates |options| based on |model_info|. Returns false if |model_info|
  // cannot construct a valid options object. Populates |status| with the
  // correct failure reason if a valid options object could not be constructed.
  bool PopulateEntityAnnotatorOptionsFromModelInfo(
      void* options,
      const ModelInfo& model_info,
      EntityAnnotatorCreationStatus* status);

  // Returns an entity metadata from the C-API representation.
  EntityMetadata GetEntityMetadataFromOptimizationGuideEntityMetadata(
      const void* og_entity_metadata);

  base::NativeLibrary native_library_;
  const bool should_provide_filter_path_ = true;

  // Functions exposed by native library.
  using GetMaxSupportedFeatureFlagFunc = int32_t (*)();
  GetMaxSupportedFeatureFlagFunc get_max_supported_feature_flag_func_ = nullptr;

  using CreateFromOptionsFunc = void* (*)(const void*);
  CreateFromOptionsFunc create_from_options_func_ = nullptr;
  using GetCreationErrorFunc = const char* (*)(const void*);
  GetCreationErrorFunc get_creation_error_func_ = nullptr;
  using DeleteFunc = void (*)(void*);
  DeleteFunc delete_func_ = nullptr;

  using AnnotateJobCreateFunc = void* (*)(void*);
  AnnotateJobCreateFunc annotate_job_create_func_ = nullptr;
  using AnnotateJobDeleteFunc = void (*)(void*);
  AnnotateJobDeleteFunc annotate_job_delete_func_ = nullptr;
  using RunAnnotateJobFunc = int32_t (*)(void*, const char*);
  RunAnnotateJobFunc run_annotate_job_func_ = nullptr;
  using AnnotateGetOutputMetadataAtIndexFunc = const void* (*)(void*, int32_t);
  AnnotateGetOutputMetadataAtIndexFunc
      annotate_get_output_metadata_at_index_func_ = nullptr;
  using AnnotateGetOutputMetadataScoreAtIndexFunc = float (*)(void*, int32_t);
  AnnotateGetOutputMetadataScoreAtIndexFunc
      annotate_get_output_metadata_score_at_index_func_ = nullptr;

  using EntityMetadataJobCreateFunc = void* (*)(void*);
  EntityMetadataJobCreateFunc entity_metadata_job_create_func_ = nullptr;
  using EntityMetadataJobDeleteFunc = void (*)(void*);
  EntityMetadataJobDeleteFunc entity_metadata_job_delete_func_ = nullptr;
  using RunEntityMetadataJobFunc = const void* (*)(void*, const char*);
  RunEntityMetadataJobFunc run_entity_metadata_job_func_ = nullptr;

  using OptionsCreateFunc = void* (*)();
  OptionsCreateFunc options_create_func_ = nullptr;
  using OptionsSetModelFilePathFunc = void (*)(void*, const char*);
  OptionsSetModelFilePathFunc options_set_model_file_path_func_ = nullptr;
  using OptionsSetModelMetadataFilePathFunc = void (*)(void*, const char*);
  OptionsSetModelMetadataFilePathFunc
      options_set_model_metadata_file_path_func_ = nullptr;
  using OptionsSetWordEmbeddingsFilePathFunc = void (*)(void*, const char*);
  OptionsSetWordEmbeddingsFilePathFunc
      options_set_word_embeddings_file_path_func_ = nullptr;
  using OptionsAddModelSliceFunc = void (*)(void*,
                                            const char*,
                                            const char*,
                                            const char*,
                                            const char*,
                                            const char*);
  OptionsAddModelSliceFunc options_add_model_slice_func_ = nullptr;
  using OptionsDeleteFunc = void (*)(void*);
  OptionsDeleteFunc options_delete_func_ = nullptr;

  using EntityMetadataGetEntityIdFunc = const char* (*)(const void*);
  EntityMetadataGetEntityIdFunc entity_metadata_get_entity_id_func_ = nullptr;
  using EntityMetadataGetHumanReadableNameFunc = const char* (*)(const void*);
  EntityMetadataGetHumanReadableNameFunc
      entity_metadata_get_human_readable_name_func_ = nullptr;
  using EntityMetadataGetHumanReadableCategoriesCountFunc =
      int32_t (*)(const void*);
  EntityMetadataGetHumanReadableCategoriesCountFunc
      entity_metadata_get_human_readable_categories_count_func_ = nullptr;
  using EntityMetadataGetHumanReadableCategoryNameAtIndexFunc =
      const char* (*)(const void*, int32_t);
  EntityMetadataGetHumanReadableCategoryNameAtIndexFunc
      entity_metadata_get_human_readable_category_name_at_index_func_ = nullptr;
  using EntityMetadataGetHumanReadableCategoryScoreAtIndexFunc =
      float (*)(const void*, int32_t);
  EntityMetadataGetHumanReadableCategoryScoreAtIndexFunc
      entity_metadata_get_human_readable_category_score_at_index_func_ =
          nullptr;
  using EntityMetadataGetHumanReadableAliasesCountFunc =
      int32_t (*)(const void*);
  EntityMetadataGetHumanReadableAliasesCountFunc
      entity_metadata_get_human_readable_aliases_count_func_ = nullptr;
  using EntityMetadataGetHumanReadableAliasAtIndexFunc =
      const char* (*)(const void*, int32_t);
  EntityMetadataGetHumanReadableAliasAtIndexFunc
      entity_metadata_get_human_readable_alias_at_index_func_ = nullptr;
  using EntityMetadataGetCollectionsCountFunc = int32_t (*)(const void*);
  EntityMetadataGetCollectionsCountFunc
      entity_metadata_get_collections_count_func_ = nullptr;
  using EntityMetadataGetCollectionAtIndexFunc = const char* (*)(const void*,
                                                                 int32_t);
  EntityMetadataGetCollectionAtIndexFunc
      entity_metadata_get_collection_at_index_func_ = nullptr;
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_ENTITY_ANNOTATOR_NATIVE_LIBRARY_H_