// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef COMPONENTS_PASSAGE_EMBEDDINGS_CORE_PASSAGE_EMBEDDINGS_SERVICE_CONTROLLER_H_
#define COMPONENTS_PASSAGE_EMBEDDINGS_CORE_PASSAGE_EMBEDDINGS_SERVICE_CONTROLLER_H_

#include <cstdint>
#include <deque>
#include <memory>
#include <string>
#include <vector>

#include "base/callback_list.h"
#include "base/memory/raw_ref.h"
#include "base/observer_list.h"
#include "base/types/optional_ref.h"
#include "components/optimization_guide/core/delivery/model_info.h"
#include "components/optimization_guide/proto/passage_embeddings_model_metadata.pb.h"
#include "components/passage_embeddings/core/passage_embeddings_service_launcher.h"
#include "components/passage_embeddings/core/passage_embeddings_types.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/passage_embeddings/public/mojom/passage_embeddings.mojom.h"

namespace base {
class ElapsedTimer;
}
namespace passage_embeddings {

class PassageEmbeddingsServiceController : public EmbedderMetadataProvider {
 public:
  friend class PassageEmbeddingsServiceControllerTest;
  explicit PassageEmbeddingsServiceController(
      PassageEmbeddingsServiceLauncher& launcher,
      bool execute_for_gemma = false);
  ~PassageEmbeddingsServiceController() override;

  // Updates the paths and the metadata needed for executing the passage
  // embeddings model. The original paths and metadata will be erased regardless
  // of the validity of the new model paths.
  // Returns true and notifies the observers if the given paths are valid.
  // Virtual for testing.
  virtual bool MaybeUpdateModelInfo(
      base::optional_ref<const optimization_guide::ModelInfo> model_info);

  // Returns true if the model is available.
  bool IsModelAvailable();

  bool EmbedderRunning();

  // Returns the embedder used to generate embeddings.
  Embedder* GetEmbedder();

  const base::FilePath& embeddings_model_path() const {
    return embeddings_model_path_;
  }
  const base::FilePath& sp_model_path() const { return sp_model_path_; }

  // EmbedderMetadataProvider:
  void AddObserver(EmbedderMetadataObserver* observer) override;
  void RemoveObserver(EmbedderMetadataObserver* observer) override;

 private:
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

  // Returns the metadata about the embeddings model. This is only valid when
  // IsModelAvailable() returns true.
  EmbedderMetadata GetEmbedderMetadata();

  // Launches the passage embeddings service. Does nothing if the service is
  // already launched.
  void MaybeLaunchService();

  // Resets `service_remote_` and notifies the launcher. Called when the service
  // remote is idle or disconnects.
  void ResetServiceRemote(bool is_idle);

  // Resets `embedder_remote_`. Called when the model info is updated, when
  // models fail to load, or when the embedder remote is idle or disconnects.
  void ResetEmbedderRemote();

  mojo::Remote<mojom::PassageEmbeddingsService> service_remote_;

  const raw_ref<PassageEmbeddingsServiceLauncher> launcher_;

  // uint64_t is large enough to never overflow.
  using RequestId = uint64_t;
  RequestId next_request_id_ = 0;

  // Called when the model files on disks are opened and ready to be sent to
  // the service.
  void LoadModelsToService(
      base::WeakPtr<PassageEmbeddingsServiceController>
          embedder_remote_weak_ptr,
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

  // Called when the embedder remote disconnects, typically due to a crash.
  void OnDisconnected(RequestId request_id,
                      GetEmbeddingsResultCallback callback);

  // Version of the embeddings model.
  int64_t model_version_ = 0;

  // Metadata of the embeddings model.
  std::optional<optimization_guide::proto::PassageEmbeddingsModelMetadata>
      model_metadata_;

  base::FilePath embeddings_model_path_;
  base::FilePath sp_model_path_;

  mojo::Remote<mojom::PassageEmbedder> embedder_remote_;

  // Pending requests to generate embeddings.
  std::deque<RequestId> pending_requests_;

  // Notifies embedders that model metadata updated.
  base::ObserverList<EmbedderMetadataObserver> observer_list_;

  // This holds the main scheduler that receives requests from multiple clients,
  // prioritizes all the jobs, and ultimately submits batches of work via
  // `GetEmbeddings` when the time is right.
  const std::unique_ptr<Embedder> embedder_;

  const bool execute_for_gemma_ = false;

  // Factory for callbacks that should only run if the embedder_remote_ hasn't
  // disconnected or been reset.
  base::WeakPtrFactory<PassageEmbeddingsServiceController>
      embedder_remote_weak_ptr_factory_{this};

  base::WeakPtrFactory<PassageEmbeddingsServiceController> weak_ptr_factory_{
      this};
};

}  // namespace passage_embeddings

#endif  // COMPONENTS_PASSAGE_EMBEDDINGS_CORE_PASSAGE_EMBEDDINGS_SERVICE_CONTROLLER_H_
