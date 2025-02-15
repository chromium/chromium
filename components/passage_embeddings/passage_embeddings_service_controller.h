// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef COMPONENTS_PASSAGE_EMBEDDINGS_PASSAGE_EMBEDDINGS_SERVICE_CONTROLLER_H_
#define COMPONENTS_PASSAGE_EMBEDDINGS_PASSAGE_EMBEDDINGS_SERVICE_CONTROLLER_H_

#include <vector>

#include "base/observer_list.h"
#include "base/types/optional_ref.h"
#include "components/optimization_guide/core/model_info.h"
#include "components/optimization_guide/proto/passage_embeddings_model_metadata.pb.h"
#include "components/passage_embeddings/embedder.h"
#include "components/passage_embeddings/passage_embeddings_types.h"
#include "components/passage_embeddings/scheduling_embedder.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/passage_embeddings/public/mojom/passage_embeddings.mojom.h"

namespace passage_embeddings {

class PassageEmbeddingsServiceController {
 public:
  PassageEmbeddingsServiceController();
  virtual ~PassageEmbeddingsServiceController();

  // Updates the paths and the metadata needed for executing the passage
  // embeddings model. The original paths and metadata will be erased regardless
  // of the validity of the new model paths. Returns true if the given paths are
  // valid.
  bool MaybeUpdateModelInfo(
      base::optional_ref<const optimization_guide::ModelInfo> model_info);

  // Returns true if the embedder is currently running.
  bool EmbedderRunning();

  // Returns an embedder that can be used to generate passage embeddings.
  std::unique_ptr<Embedder> MakeEmbedder();

  // Subscribe for notification when embedder metadata is ready. This may
  // result in immediate notification if metadata is ready at time of call.
  void AddObserver(EmbedderMetadataObserver* observer);

  // Must be called exactly once for each corresponding call to
  // `AddEmbedderMetadataObserver` when observation is no longer needed.
  void RemoveObserver(EmbedderMetadataObserver* observer);

 protected:
  // Embedders are the way to access the `GetEmbeddings` API. Protecting it from
  // general use avoids bare access calls that would interrupt scheduled tasks.
  friend class MlEmbedder;

  // Starts the service and calls `callback` with the embeddings. It is
  // guaranteed that the result will have the same number of elements as
  // `passages` when all embeddings executions succeed. Otherwise, will return
  // an empty vector.
  using GetEmbeddingsCallback = base::OnceCallback<void(
      std::vector<mojom::PassageEmbeddingsResultPtr> results,
      ComputeEmbeddingsStatus status)>;
  void GetEmbeddings(std::vector<std::string> passages,
                     PassagePriority priority,
                     GetEmbeddingsCallback callback);

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
      mojom::PassageEmbeddingsLoadModelsParamsPtr params);

  // Called when an attempt to load models to service finishes.
  void OnLoadModelsResult(bool success);

  // Called when an attempt to generate embeddings finishes.
  void OnGotEmbeddings(RequestId request_id,
                       GetEmbeddingsCallback callback,
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

  // This holds the main scheduler that receives requests from multiple separate
  // client embedders, prioritizes all the jobs, and ultimately submits batches
  // of work via `GetEmbeddings` when the time is right.
  std::unique_ptr<SchedulingEmbedder> scheduling_embedder_;

  // Used to generate weak pointers to self.
  base::WeakPtrFactory<PassageEmbeddingsServiceController> weak_ptr_factory_{
      this};
};

}  // namespace passage_embeddings

#endif  // COMPONENTS_PASSAGE_EMBEDDINGS_PASSAGE_EMBEDDINGS_SERVICE_CONTROLLER_H_
