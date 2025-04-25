// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef COMPONENTS_PASSAGE_EMBEDDINGS_PASSAGE_EMBEDDINGS_SERVICE_CONTROLLER_H_
#define COMPONENTS_PASSAGE_EMBEDDINGS_PASSAGE_EMBEDDINGS_SERVICE_CONTROLLER_H_

#include <memory>
#include <vector>

#include "base/callback_list.h"
#include "base/observer_list.h"
#include "base/timer/elapsed_timer.h"
#include "base/types/optional_ref.h"
#include "components/optimization_guide/core/model_info.h"
#include "components/optimization_guide/proto/passage_embeddings_model_metadata.pb.h"
#include "components/passage_embeddings/passage_embeddings_types.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/passage_embeddings/public/mojom/passage_embeddings.mojom.h"

namespace passage_embeddings {

class PassageEmbeddingsServiceController : public EmbedderMetadataProvider {
 public:
  PassageEmbeddingsServiceController();
  ~PassageEmbeddingsServiceController() override;

  // Updates the paths and the metadata needed for executing the passage
  // embeddings model. The original paths and metadata will be erased regardless
  // of the validity of the new model paths.
  // Returns true and notifies the observers if the given paths are valid.
  // Virtual for testing.
  virtual bool MaybeUpdateModelInfo(
      base::optional_ref<const optimization_guide::ModelInfo> model_info);

  // Returns true if the embedder is currently running.
  bool EmbedderRunning();

  // Returns the embedder used to generate embeddings.
  Embedder* GetEmbedder();

 protected:
  // EmbedderMetadataProvider:
  void AddObserver(EmbedderMetadataObserver* observer) override;
  void RemoveObserver(EmbedderMetadataObserver* observer) override;

  // Computes embeddings for each entry in `passages`. Will invoke `callback`
  // when done. If successful, it is guaranteed that `results` will have the
  // same number of passages and embeddings and in the same order as
  // `passages`. Otherwise `results` will have empty passages and embeddings.
  using GetEmbeddingsResultCallback = base::OnceCallback<void(
      std::vector<mojom::PassageEmbeddingsResultPtr> results,
      ComputeEmbeddingsStatus status)>;
  using GetEmbeddingsCallback =
      base::RepeatingCallback<void(std::vector<std::string> passages,
                                   PassagePriority priority,
                                   GetEmbeddingsResultCallback callback)>;
  void GetEmbeddings(std::vector<std::string> passages,
                     PassagePriority priority,
                     GetEmbeddingsResultCallback callback);

  // Returns true if this service controller is ready for embeddings generation.
  bool EmbedderReady();

  // Returns the metadata about the embeddings model. This is only valid when
  // EmbedderReady() returns true.
  EmbedderMetadata GetEmbedderMetadata();

  // Launches the passage embeddings service and binds `cpu_logger_` to the
  // service process. Does nothing if the service is already launched.
  virtual void MaybeLaunchService() = 0;

  // Resets `service_remote_` and `cpu_logger_`. Called when the service remote
  // is idle or disconnects.
  virtual void ResetServiceRemote() = 0;

  // Resets `embedder_remote_`. Called when the model info is updated, when
  // models fail to load, or when the embedder remote is idle or disconnects.
  void ResetEmbedderRemote();

  mojo::Remote<mojom::PassageEmbeddingsService> service_remote_;

 private:
  // uint64_t is large enough to never overflow.
  using RequestId = uint64_t;
  RequestId next_request_id_ = 0;

  // Called when the model files on disks are opened and ready to be sent to
  // the service.
  void LoadModelsToService(
      mojo::PendingReceiver<mojom::PassageEmbedder> receiver,
      base::ElapsedTimer service_launch_timer,
      mojom::PassageEmbeddingsLoadModelsParamsPtr params);

  // Called when an attempt to load models to service finishes.
  void OnLoadModelsResult(base::ElapsedTimer service_launch_timer,
                          bool success);

  // Called when an attempt to generate embeddings finishes.
  void OnGotEmbeddings(RequestId request_id,
                       GetEmbeddingsResultCallback callback,
                       base::ElapsedTimer generate_embeddings_timer,
                       PassagePriority priority,
                       std::vector<mojom::PassageEmbeddingsResultPtr> results);

  // Version of the embeddings model.
  int64_t model_version_;

  // Metadata of the embeddings model.
  std::optional<optimization_guide::proto::PassageEmbeddingsModelMetadata>
      model_metadata_;

  base::FilePath embeddings_model_path_;
  base::FilePath sp_model_path_;

  mojo::Remote<mojom::PassageEmbedder> embedder_remote_;

  // Pending requests to generate embeddings.
  std::vector<RequestId> pending_requests_;

  // Notifies embedders that model metadata updated.
  base::ObserverList<EmbedderMetadataObserver> observer_list_;

  // This holds the main scheduler that receives requests from multiple clients,
  // prioritizes all the jobs, and ultimately submits batches of work via
  // `GetEmbeddings` when the time is right.
  const std::unique_ptr<Embedder> embedder_;

  // Used to generate weak pointers to self.
  base::WeakPtrFactory<PassageEmbeddingsServiceController> weak_ptr_factory_{
      this};
};

}  // namespace passage_embeddings

#endif  // COMPONENTS_PASSAGE_EMBEDDINGS_PASSAGE_EMBEDDINGS_SERVICE_CONTROLLER_H_
