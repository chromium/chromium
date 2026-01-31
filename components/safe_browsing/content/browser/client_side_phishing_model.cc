// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/browser/client_side_phishing_model.h"

#include <stdint.h>

#include <memory>
#include <optional>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/memory/singleton.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/byte_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "components/optimization_guide/core/delivery/optimization_guide_model_provider.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/optimization_guide/proto/client_side_phishing_model_metadata.pb.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "components/safe_browsing/core/common/fbs/client_model_generated.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/proto/client_model.pb.h"
#include "components/safe_browsing/core/common/safebrowsing_switches.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "crypto/hash.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/zlib/google/compression_utils.h"

namespace safe_browsing {

namespace {

void ReturnModelOverrideFailure(
    base::OnceCallback<void(std::pair<std::string, base::File>)> callback) {
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback),
                                std::make_pair(std::string(), base::File())));
}

void ReadOverridenModel(
    base::FilePath path,
    base::OnceCallback<void(std::pair<std::string, base::File>)> callback) {
  if (path.empty()) {
    VLOG(2) << "Failed to override model. Path is empty. Path is " << path;
    ReturnModelOverrideFailure(std::move(callback));
    return;
  }

  std::string contents;
  if (!base::ReadFileToString(path.AppendASCII("client_model.pb"), &contents)) {
    VLOG(2) << "Failed to override model. Could not read model data.";
    ReturnModelOverrideFailure(std::move(callback));
    return;
  }

  base::File tflite_model(path.AppendASCII("visual_model.tflite"),
                          base::File::FLAG_OPEN | base::File::FLAG_READ);
  // `tflite_model` is allowed to be invalid, when testing a DOM-only model.

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback),
                     std::make_pair(contents, std::move(tflite_model))));
}

base::File LoadImageEmbeddingModelFile(const base::FilePath& model_file_path) {
  if (!base::PathExists(model_file_path)) {
    VLOG(0)
        << "Model path does not exist. Returning empty file. Given path is: "
        << model_file_path;
    return base::File();
  }

  base::File image_embedding_model_file(
      model_file_path, base::File::FLAG_OPEN | base::File::FLAG_READ);

  if (!image_embedding_model_file.IsValid()) {
    VLOG(2)
        << "Failed to receive image embedding model file. File is not valid";
    return base::File();
  }

  return image_embedding_model_file;
}

std::pair<base::File, std::optional<EmbeddingList>>
LoadImageEmbeddingModelFileAndEmbeddingList(
    const base::FilePath& model_file_path,
    base::flat_set<base::FilePath> additional_files) {
  base::File image_embedding_file =
      LoadImageEmbeddingModelFile(model_file_path);
  // No need to attempt loading |additional_files| with no image embedding file.
  if (!image_embedding_file.IsValid()) {
    return {base::File(), std::nullopt};
  }
  // The |additional_files.size()| can be zero (e.g., on Desktop). In this case,
  // we should only return the image embedding file.
  if (additional_files.size() == 0) {
    return {std::move(image_embedding_file), std::nullopt};
  }
  // There should only be one additional file.
  const base::FilePath& target_list_file_path = *additional_files.begin();

  // Read the file in.
  std::string file_content;
  if (!base::ReadFileToString(target_list_file_path, &file_content)) {
    // We failed to read in the zipped target embedding list, so proceed without
    // it.
    return {std::move(image_embedding_file), std::nullopt};
  }

  // Uncompress the file.
  if (!compression::GzipUncompress(file_content, &file_content)) {
    // We failed to unzip the target embedding list, so proceed without it.
    return {std::move(image_embedding_file), std::nullopt};
  }

  // Parse the proto.
  EmbeddingList embedding_list_pb;
  if (!embedding_list_pb.ParseFromString(file_content)) {
    // We failed to parse the EmbeddingList, so proceed without it.
    return {std::move(image_embedding_file), std::nullopt};
  }

  return {std::move(image_embedding_file),
          std::make_optional(embedding_list_pb)};
}

// Load the model file at the provided file path.
std::pair<std::string, base::File> LoadModelAndVisualTfLiteFile(
    const base::FilePath& model_file_path,
    base::flat_set<base::FilePath> additional_files) {
  if (!base::PathExists(model_file_path)) {
    VLOG(0) << "Model path does not exist. Returning empty pair. Given path is "
            << model_file_path;
    return std::pair<std::string, base::File>();
  }

  // The only additional file we require and expect is the visual tf lite file
  if (additional_files.size() != 1) {
    VLOG(2) << "Did not receive just one additional file when expected. "
               "Actual size: "
            << additional_files.size();
    return std::pair<std::string, base::File>();
  }

  std::optional<base::FilePath> visual_tflite_path = std::nullopt;

  for (const base::FilePath& path : additional_files) {
    // There should only be one loop after above check
    DCHECK(path.IsAbsolute());
    visual_tflite_path = path;
  }

  base::File model(model_file_path,
                   base::File::FLAG_OPEN | base::File::FLAG_READ);
  base::File tf_lite(*visual_tflite_path,
                     base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!model.IsValid() || !tf_lite.IsValid()) {
    VLOG(2) << "Failed to override the model and/or tf_lite file.";
  }

  // Convert model to string
  std::string model_data;
  if (!base::ReadFileToString(model_file_path, &model_data)) {
    VLOG(2) << "Failed to override model. Could not read model data.";
    return std::pair<std::string, base::File>();
  }

  return std::make_pair(std::string(model_data.begin(), model_data.end()),
                        std::move(tf_lite));
}

// Close the provided model file.
void CloseModelFile(base::File model_file) {
  if (!model_file.IsValid()) {
    return;
  }
  model_file.Close();
}

void RecordImageEmbeddingModelUpdateSuccess(bool success) {
  base::UmaHistogramBoolean(
      "SBClientPhishing.ModelDynamicUpdateSuccess.ImageEmbedding", success);
}

}  // namespace

#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
TargetEmbedding::TargetEmbedding(tflite::task::vision::FeatureVector embedding,
                                 float threshold)
    : embedding(std::move(embedding)), threshold(threshold) {}
#endif

// --- ClientSidePhishingModel methods ---

ClientSidePhishingModel::ClientSidePhishingModel(
    optimization_guide::OptimizationGuideModelProvider* opt_guide)
    : opt_guide_(opt_guide),
      background_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT})),
      beginning_time_(base::TimeTicks::Now()) {
  opt_guide_->AddObserverForOptimizationTargetModel(
      optimization_guide::proto::OPTIMIZATION_TARGET_CLIENT_SIDE_PHISHING,
      /*model_metadata=*/std::nullopt, background_task_runner_, this);
}

void ClientSidePhishingModel::OnModelUpdated(
    optimization_guide::proto::OptimizationTarget optimization_target,
    base::optional_ref<const optimization_guide::ModelInfo> model_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (optimization_target !=
          optimization_guide::proto::OPTIMIZATION_TARGET_CLIENT_SIDE_PHISHING &&
      optimization_target !=
          optimization_guide::proto::
              OPTIMIZATION_TARGET_CLIENT_SIDE_PHISHING_IMAGE_EMBEDDER) {
    return;
  }

  if (optimization_target ==
      optimization_guide::proto::OPTIMIZATION_TARGET_CLIENT_SIDE_PHISHING) {
    // If the model_info has no value, that means the OptimizationGuide server
    // has sent an intentionally null model value to indicate that there is a
    // bad model on disk and it should be removed. Therefore, we will clear the
    // current model in the class.
    if (!model_info.has_value()) {
      trigger_model_opt_guide_metadata_image_embedding_version_.reset();
      mapped_region_ = base::MappedReadOnlyRegion();
      if (visual_tflite_model_) {
        background_task_runner_->PostTask(
            FROM_HERE,
            base::BindOnce(&CloseModelFile, std::move(*visual_tflite_model_)));
      }
      // Run callback to remove models from the renderer process. When a
      // callback is called and there are no models in this class while the
      // model type is set, it's expected that it's asked to remove the models.
      content::GetUIThreadTaskRunner({})->PostTask(
          FROM_HERE,
          base::BindOnce(&ClientSidePhishingModel::NotifyCallbacksOnUI,
                         weak_ptr_factory_.GetWeakPtr()));
      return;
    }

    background_task_runner_->PostTaskAndReplyWithResult(
        FROM_HERE,
        base::BindOnce(&LoadModelAndVisualTfLiteFile,
                       model_info->GetModelFilePath(),
                       model_info->GetAdditionalFiles()),
        base::BindOnce(
            &ClientSidePhishingModel::OnModelAndVisualTfLiteFileLoaded,
            weak_ptr_factory_.GetWeakPtr(), model_info->GetModelMetadata()));
  } else if (optimization_target ==
             optimization_guide::proto::
                 OPTIMIZATION_TARGET_CLIENT_SIDE_PHISHING_IMAGE_EMBEDDER) {
    // If the model_info has no value for this target, we only remove the image
    // embedding model, and if the trigger models are still valid, then the
    // scorer will be created with the trigger models only.
    if (!model_info.has_value()) {
      embedding_model_opt_guide_metadata_image_embedding_version_.reset();
      if (image_embedding_model_) {
        background_task_runner_->PostTask(
            FROM_HERE, base::BindOnce(&CloseModelFile,
                                      std::move(*image_embedding_model_)));
      }
      content::GetUIThreadTaskRunner({})->PostTask(
          FROM_HERE,
          base::BindOnce(&ClientSidePhishingModel::NotifyCallbacksOnUI,
                         weak_ptr_factory_.GetWeakPtr()));
      return;
    }

    background_task_runner_->PostTaskAndReplyWithResult(
        FROM_HERE,
        base::BindOnce(&LoadImageEmbeddingModelFileAndEmbeddingList,
                       model_info->GetModelFilePath(),
                       model_info->GetAdditionalFiles()),
        base::BindOnce(&ClientSidePhishingModel::
                           OnImageEmbeddingModelFileAndEmbeddingListLoaded,
                       weak_ptr_factory_.GetWeakPtr(),
                       model_info->GetModelMetadata()));
  }
}

void ClientSidePhishingModel::SubscribeToImageEmbedderOptimizationGuide() {
  if (!subscribed_to_image_embedder_ && opt_guide_) {
    subscribed_to_image_embedder_ = true;
    opt_guide_->AddObserverForOptimizationTargetModel(
        optimization_guide::proto::
            OPTIMIZATION_TARGET_CLIENT_SIDE_PHISHING_IMAGE_EMBEDDER,
        /*model_metadata=*/std::nullopt, background_task_runner_, this);
  }
}

void ClientSidePhishingModel::UnsubscribeToImageEmbedderOptimizationGuide() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (subscribed_to_image_embedder_ && opt_guide_) {
    subscribed_to_image_embedder_ = false;
    opt_guide_->RemoveObserverForOptimizationTargetModel(
        optimization_guide::proto::
            OPTIMIZATION_TARGET_CLIENT_SIDE_PHISHING_IMAGE_EMBEDDER,
        this);
    embedding_model_opt_guide_metadata_image_embedding_version_.reset();
    if (image_embedding_model_) {
      background_task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(&CloseModelFile, std::move(*image_embedding_model_)));

      // We will only notify if there was an image embedding model available, so
      // the renderer can remove it.
      content::GetUIThreadTaskRunner({})->PostTask(
          FROM_HERE,
          base::BindOnce(&ClientSidePhishingModel::NotifyCallbacksOnUI,
                         weak_ptr_factory_.GetWeakPtr()));
    }
  }
}

bool ClientSidePhishingModel::IsSubscribedToImageEmbeddingModelUpdates() {
  return subscribed_to_image_embedder_;
}

void ClientSidePhishingModel::OnModelAndVisualTfLiteFileLoaded(
    std::optional<optimization_guide::proto::Any> model_metadata,
    std::pair<std::string, base::File> model_and_tflite) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (visual_tflite_model_) {
    // If the visual tf lite file is already loaded, it should be closed on a
    // background thread.
    background_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&CloseModelFile, std::move(*visual_tflite_model_)));
  }

  std::string model_str = std::move(model_and_tflite.first);
  base::File visual_tflite_model = std::move(model_and_tflite.second);

  bool model_valid = false;
  bool tflite_valid = visual_tflite_model.IsValid();
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kOverrideCsdModelFlag) &&
      !model_str.empty()) {
    model_type_ = CSDModelType::kNone;
    flatbuffers::Verifier verifier(
        reinterpret_cast<const uint8_t*>(model_str.data()), model_str.length());
    model_valid = flat::VerifyClientSideModelBuffer(verifier);
    if (model_valid) {
      mapped_region_ =
          base::ReadOnlySharedMemoryRegion::Create(model_str.length());
      if (mapped_region_.IsValid()) {
        model_type_ = CSDModelType::kFlatbuffer;
        mapped_region_.mapping.GetMemoryAsSpan<char>().copy_prefix_from(
            model_str);

        const flat::ClientSideModel* flatbuffer_model =
            flat::GetClientSideModel(mapped_region_.mapping.memory());

        if (!VerifyCSDFlatBufferIndicesAndFields(flatbuffer_model)) {
          VLOG(0) << "Failed to verify CSD Flatbuffer indices and fields";
        } else {
          if (tflite_valid) {
            thresholds_.clear();  // Clear the previous model's thresholds
                                  // before adding on the new ones
            for (const flat::TfLiteModelMetadata_::Threshold* flat_threshold :
                 *(flatbuffer_model->tflite_metadata()->thresholds())) {
              TfLiteModelMetadata::Threshold threshold;
              threshold.set_label(flat_threshold->label()->str());
              threshold.set_threshold(flat_threshold->threshold());
              threshold.set_esb_threshold(flat_threshold->esb_threshold() > 0
                                              ? flat_threshold->esb_threshold()
                                              : flat_threshold->threshold());
              thresholds_.push_back(threshold);
            }
            // TODO: (crbug.com/467955445) tflite_metadata has been verified
            // already, but image embedding metadata is not for testing
            // purposes in keeping flatbuffer model size small with an older
            // version. Once the metadata has been migrated to optimization
            // guide service, the fields will be derived through that instead
            // of the flatbuffer.
            const flat::TfLiteModelMetadata* tflite_metadata =
                flatbuffer_model->tflite_metadata();
            classification_input_width_ = tflite_metadata->input_width();
            classification_input_height_ = tflite_metadata->input_height();
            trigger_model_version_ = tflite_metadata->version();
            const flat::TfLiteModelMetadata* image_embedding_metadata =
                flatbuffer_model->img_embedding_metadata();
            if (image_embedding_metadata) {
              img_embedding_input_width_ =
                  image_embedding_metadata->input_width();
              img_embedding_input_height_ =
                  image_embedding_metadata->input_height();
              image_embedding_model_version_ =
                  image_embedding_metadata->version();
            }
          }
        }
      } else {
        model_valid = false;
      }
      base::UmaHistogramBoolean("SBClientPhishing.FlatBufferMappedRegionValid",
                                mapped_region_.IsValid());
    } else {
      VLOG(2) << "Failed to validate flatbuffer model";
    }
  }

  base::UmaHistogramBoolean("SBClientPhishing.ModelDynamicUpdateSuccess",
                            model_valid);

  if (tflite_valid) {
    visual_tflite_model_ = std::move(visual_tflite_model);
  }

  if (model_valid && tflite_valid) {
    base::UmaHistogramMediumTimes(
        "SBClientPhishing.OptimizationGuide.ModelFetchTime",
        base::TimeTicks::Now() - beginning_time_);

    std::optional<optimization_guide::proto::ClientSidePhishingModelMetadata>
        client_side_phishing_model_metadata = std::nullopt;

    if (model_metadata.has_value()) {
      client_side_phishing_model_metadata =
          optimization_guide::ParsedAnyMetadata<
              optimization_guide::proto::ClientSidePhishingModelMetadata>(
              model_metadata.value());
    }

    if (client_side_phishing_model_metadata.has_value()) {
      trigger_model_opt_guide_metadata_image_embedding_version_ =
          client_side_phishing_model_metadata->image_embedding_model_version();
    } else {
      VLOG(1) << "Client side phishing model metadata is missing an image "
                 "embedding model version value";
    }

    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(&ClientSidePhishingModel::NotifyCallbacksOnUI,
                                  weak_ptr_factory_.GetWeakPtr()));
  }
}

void ClientSidePhishingModel::OnImageEmbeddingModelFileAndEmbeddingListLoaded(
    std::optional<optimization_guide::proto::Any> model_metadata,
    std::pair<base::File, std::optional<EmbeddingList>> model_and_list) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::File image_embedding_model = std::move(model_and_list.first);
  bool image_embedding_model_valid = image_embedding_model.IsValid();
  RecordImageEmbeddingModelUpdateSuccess(image_embedding_model_valid);

  // Any failure to loading the image embedding model will send an empty file.
  if (!image_embedding_model_valid) {
    return;
  }

  if (image_embedding_model_) {
    // If the image embedding model file is already loaded, it should be closed
    // on a background thread.
    background_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&CloseModelFile, std::move(*image_embedding_model_)));
  }

  image_embedding_model_ = std::move(image_embedding_model);

  std::optional<optimization_guide::proto::ClientSidePhishingModelMetadata>
      image_embedding_model_metadata = std::nullopt;

  if (model_metadata.has_value()) {
    image_embedding_model_metadata = optimization_guide::ParsedAnyMetadata<
        optimization_guide::proto::ClientSidePhishingModelMetadata>(
        model_metadata.value());
  }

  int loaded_image_embedder_version = -1;  // Valid versions start at 1.
  if (image_embedding_model_metadata.has_value()) {
    embedding_model_opt_guide_metadata_image_embedding_version_ =
        image_embedding_model_metadata->image_embedding_model_version();
    loaded_image_embedder_version =
        embedding_model_opt_guide_metadata_image_embedding_version_.value();
  } else {
    VLOG(1) << "Image embedding model metadata is missing a version value";
  }

  // Log the EmbeddingList version fetched.
  if (model_and_list.second.has_value()) {
    std::string version = model_and_list.second.value().version();
    int embedding_list_version_num;
    // The expected version format is PYYMMDDRR. "P" is a letter that
    // corresponds to the platform the version is for. In the context of UMA,
    // that information is redundant. So, we skip that part of the version with
    // the substr method.
    if (base::StringToInt(version.size() > 1 ? version.substr(1) : "",
                          &embedding_list_version_num)) {
      base::UmaHistogramSparse("SBClientPhishing.ImageEmbeddingList.Version",
                               embedding_list_version_num);
    }
  }
  // Drop any existing target image embeddings in preparation for a new set.
  target_image_embeddings_.clear();
#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
  // Only load image embeddings when the version of their embedder matches the
  // version of the embedder that's been loaded.
  if (model_and_list.second.has_value() &&
      model_and_list.second.value().embedder_version() ==
          loaded_image_embedder_version) {
    // Build list of TargetEmbeddings.
    for (const EmbeddingList::Embedding& embedding :
         model_and_list.second.value().image_embeddings()) {
      tflite::task::vision::FeatureVector feature_vector;
      for (float embedding_value : embedding.value()) {
        feature_vector.add_value_float(embedding_value);
      }
      target_image_embeddings_.emplace_back(feature_vector,
                                            embedding.threshold());
    }
  }
  base::UmaHistogramCounts100000("SBClientPhishing.ImageEmbeddingList.Size",
                                 target_image_embeddings_.size());
#endif
  // There is no use of the image embedding model if the visual trigger model is
  // not present, so we will only send to the renderer when that is the case.
  if (visual_tflite_model_ && image_embedding_model_) {
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(&ClientSidePhishingModel::NotifyCallbacksOnUI,
                                  weak_ptr_factory_.GetWeakPtr()));
  }
}

bool ClientSidePhishingModel::IsModelMetadataImageEmbeddingVersionMatching() {
  return trigger_model_opt_guide_metadata_image_embedding_version_
             .has_value() &&
         embedding_model_opt_guide_metadata_image_embedding_version_
             .has_value() &&
         trigger_model_opt_guide_metadata_image_embedding_version_.value() ==
             embedding_model_opt_guide_metadata_image_embedding_version_
                 .value();
}

int ClientSidePhishingModel::GetTriggerModelVersion() {
  return trigger_model_version_.has_value() ? trigger_model_version_.value()
                                            : 0;
}

int ClientSidePhishingModel::GetImageEmbeddingModelVersion() {
  return image_embedding_model_version_.has_value()
             ? image_embedding_model_version_.value()
             : 0;
}

#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
const std::vector<TargetEmbedding>&
ClientSidePhishingModel::GetTargetImageEmbeddings() const {
  return target_image_embeddings_;
}

void ClientSidePhishingModel::SetTargetImageEmbeddingsForTesting(
    std::vector<TargetEmbedding> target_embeddings) {
  target_image_embeddings_ = std::move(target_embeddings);
}
#endif
int ClientSidePhishingModel::GetClassificationInputWidth() {
  return classification_input_width_.has_value()
             ? classification_input_width_.value()
             : 0;
}

int ClientSidePhishingModel::GetClassificationInputHeight() {
  return classification_input_height_.has_value()
             ? classification_input_height_.value()
             : 0;
}

int ClientSidePhishingModel::GetImageEmbeddingInputWidth() {
  return img_embedding_input_width_.has_value()
             ? img_embedding_input_width_.value()
             : 0;
}

int ClientSidePhishingModel::GetImageEmbeddingInputHeight() {
  return img_embedding_input_height_.has_value()
             ? img_embedding_input_height_.value()
             : 0;
}

ClientSidePhishingModel::~ClientSidePhishingModel() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  opt_guide_->RemoveObserverForOptimizationTargetModel(
      optimization_guide::proto::OPTIMIZATION_TARGET_CLIENT_SIDE_PHISHING,
      this);

  if (subscribed_to_image_embedder_) {
    opt_guide_->RemoveObserverForOptimizationTargetModel(
        optimization_guide::proto::
            OPTIMIZATION_TARGET_CLIENT_SIDE_PHISHING_IMAGE_EMBEDDER,
        this);
  }

  if (visual_tflite_model_) {
    background_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&CloseModelFile, std::move(*visual_tflite_model_)));
  }

  if (image_embedding_model_) {
    background_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&CloseModelFile, std::move(*image_embedding_model_)));
  }

  opt_guide_ = nullptr;
}

base::CallbackListSubscription ClientSidePhishingModel::RegisterCallback(
    base::RepeatingCallback<void()> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return callbacks_.Add(std::move(callback));
}

bool ClientSidePhishingModel::IsEnabled() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return (model_type_ == CSDModelType::kFlatbuffer &&
          mapped_region_.IsValid() && visual_tflite_model_ &&
          visual_tflite_model_->IsValid());
}

// static
bool ClientSidePhishingModel::VerifyCSDFlatBufferIndicesAndFields(
    const flat::ClientSideModel* model) {
  const flat::TfLiteModelMetadata* metadata = model->tflite_metadata();
  if (!metadata) {
    return false;
  }

  const flatbuffers::Vector<flatbuffers::Offset<flat::Hash>>* hashes =
      model->hashes();
  if (!hashes) {
    return false;
  }

  const flatbuffers::Vector<flatbuffers::Offset<flat::ClientSideModel_::Rule>>*
      rules = model->rule();
  if (!rules) {
    return false;
  }
  for (const flat::ClientSideModel_::Rule* rule : *model->rule()) {
    if (!rule || !rule->feature()) {
      return false;
    }
    for (int32_t feature : *rule->feature()) {
      if (feature < 0 || feature >= static_cast<int32_t>(hashes->size())) {
        return false;
      }
    }
  }

  const flatbuffers::Vector<int32_t>* page_terms = model->page_term();
  if (!page_terms) {
    return false;
  }
  for (int32_t page_term_idx : *page_terms) {
    if (page_term_idx < 0 ||
        page_term_idx >= static_cast<int32_t>(hashes->size())) {
      return false;
    }
  }

  const flatbuffers::Vector<uint32_t>* page_words = model->page_word();
  if (!page_words) {
    return false;
  }

  const flatbuffers::Vector<
      flatbuffers::Offset<flat::TfLiteModelMetadata_::Threshold>>* thresholds =
      metadata->thresholds();
  if (!thresholds) {
    return false;
  }
  for (const flat::TfLiteModelMetadata_::Threshold* threshold : *thresholds) {
    if (!threshold || !threshold->label()) {
      return false;
    }
  }

  return true;
}

// static
std::string ClientSidePhishingModel::GetHashFromEmbedding(
    const std::vector<float>& embedding_values) {
  crypto::hash::Hasher hasher(crypto::hash::HashKind::kSha256);
  for (const float embedding_value : embedding_values) {
    hasher.Update(base::FloatToBigEndian(embedding_value));
  }
  std::array<uint8_t, crypto::hash::kSha256Size> raw_hash;
  hasher.Finish(raw_hash);
  return base::HexEncodeLower(raw_hash);
}

const std::vector<TfLiteModelMetadata::Threshold>&
ClientSidePhishingModel::GetVisualTfLiteModelThresholds() const {
  return thresholds_;
}

const base::File& ClientSidePhishingModel::GetVisualTfLiteModel() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(visual_tflite_model_ && visual_tflite_model_->IsValid());
  return *visual_tflite_model_;
}

const base::File& ClientSidePhishingModel::GetImageEmbeddingModel() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(image_embedding_model_ && image_embedding_model_->IsValid());
  return *image_embedding_model_;
}

bool ClientSidePhishingModel::HasImageEmbeddingModel() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return image_embedding_model_ && image_embedding_model_->IsValid();
}

CSDModelType ClientSidePhishingModel::GetModelType() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return model_type_;
}

base::ReadOnlySharedMemoryRegion
ClientSidePhishingModel::GetModelSharedMemoryRegion() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return mapped_region_.region.Duplicate();
}

void ClientSidePhishingModel::SetModelStringForTesting(
    const std::string& model_str,
    base::File visual_tflite_model) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  bool model_valid = false;
  bool tflite_valid = visual_tflite_model.IsValid();

  // TODO (andysjlim): Move to a helper function once protobuf feature is
  // removed
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kOverrideCsdModelFlag) &&
      !model_str.empty()) {
    model_type_ = CSDModelType::kNone;
    flatbuffers::Verifier verifier(
        reinterpret_cast<const uint8_t*>(model_str.data()), model_str.length());
    model_valid = flat::VerifyClientSideModelBuffer(verifier);
    if (model_valid) {
      mapped_region_ =
          base::ReadOnlySharedMemoryRegion::Create(model_str.length());
      if (mapped_region_.IsValid()) {
        model_type_ = CSDModelType::kFlatbuffer;
        mapped_region_.mapping.GetMemoryAsSpan<char>().copy_prefix_from(
            model_str);
      } else {
        model_valid = false;
      }
      base::UmaHistogramBoolean("SBClientPhishing.FlatBufferMappedRegionValid",
                                mapped_region_.IsValid());
    }

    base::UmaHistogramBoolean("SBClientPhishing.ModelDynamicUpdateSuccess",
                              model_valid);

    if (tflite_valid) {
      visual_tflite_model_ = std::move(visual_tflite_model);
    }
  }

  if (model_valid || tflite_valid) {
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(&ClientSidePhishingModel::NotifyCallbacksOnUI,
                                  base::Unretained(this)));
  }
}

void ClientSidePhishingModel::NotifyCallbacksOnUI() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  callbacks_.Notify();
}

void ClientSidePhishingModel::SetVisualTfLiteModelForTesting(base::File file) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  visual_tflite_model_ = std::move(file);
}

void ClientSidePhishingModel::SetModelTypeForTesting(CSDModelType model_type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  model_type_ = model_type;
}

void ClientSidePhishingModel::ClearMappedRegionForTesting() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  mapped_region_.mapping = base::WritableSharedMemoryMapping();
  mapped_region_.region = base::ReadOnlySharedMemoryRegion();
}

void* ClientSidePhishingModel::GetFlatBufferMemoryAddressForTesting() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return mapped_region_.mapping.memory();
}

// This function is used for testing in client_side_phishing_model_unittest
void ClientSidePhishingModel::MaybeOverrideModel() {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kOverrideCsdModelFlag)) {
    base::FilePath overriden_model_directory =
        base::CommandLine::ForCurrentProcess()->GetSwitchValuePath(
            switches::kOverrideCsdModelFlag);
    base::ThreadPool::PostTask(
        FROM_HERE, {base::MayBlock()},
        base::BindOnce(
            &ReadOverridenModel, overriden_model_directory,
            base::BindOnce(&ClientSidePhishingModel::OnGetOverridenModelData,
                           base::Unretained(this), CSDModelType::kFlatbuffer)));
  }
}

// This function is used for testing in client_side_phishing_model_unittest
void ClientSidePhishingModel::OnGetOverridenModelData(
    CSDModelType model_type,
    std::pair<std::string, base::File> model_and_tflite) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const std::string& model_data = model_and_tflite.first;
  base::File tflite_model = std::move(model_and_tflite.second);
  if (model_data.empty()) {
    VLOG(2) << "Overriden model data is empty";
    return;
  }

  switch (model_type) {
    case CSDModelType::kFlatbuffer: {
      flatbuffers::Verifier verifier(
          reinterpret_cast<const uint8_t*>(model_data.data()),
          model_data.length());
      if (!flat::VerifyClientSideModelBuffer(verifier)) {
        VLOG(2)
            << "Overriden model data is not a valid ClientSideModel flatbuffer";
        return;
      }
      mapped_region_ =
          base::ReadOnlySharedMemoryRegion::Create(model_data.length());
      if (!mapped_region_.IsValid()) {
        VLOG(2) << "Could not create shared memory region for flatbuffer";
        return;
      }
      mapped_region_.mapping.GetMemoryAsSpan<char>().copy_prefix_from(
          model_data);
      model_type_ = model_type;
      break;
    }
    case CSDModelType::kNone:
      NOTREACHED();
  }

  if (tflite_model.IsValid()) {
    visual_tflite_model_ = std::move(tflite_model);
  }

  VLOG(0) << "Model overridden successfully";

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&ClientSidePhishingModel::NotifyCallbacksOnUI,
                                weak_ptr_factory_.GetWeakPtr()));
}

// For browser unit testing in client_side_detection_service_browsertest
void ClientSidePhishingModel::SetModelAndVisualTfLiteForTesting(
    const base::FilePath& model_file_path,
    const base::FilePath& visual_tf_lite_model_path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::flat_set<base::FilePath> additional_files;
  additional_files.insert(visual_tf_lite_model_path);
  background_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&LoadModelAndVisualTfLiteFile, model_file_path,
                     additional_files),
      base::BindOnce(&ClientSidePhishingModel::OnModelAndVisualTfLiteFileLoaded,
                     weak_ptr_factory_.GetWeakPtr(), std::nullopt));
}

}  // namespace safe_browsing
