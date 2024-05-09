// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef COMPONENTS_HISTORY_EMBEDDINGS_PASSAGE_EMBEDDINGS_SERVICE_CONTROLLER_H_
#define COMPONENTS_HISTORY_EMBEDDINGS_PASSAGE_EMBEDDINGS_SERVICE_CONTROLLER_H_

#include "base/types/optional_ref.h"
#include "components/history_embeddings/embedder.h"
#include "components/history_embeddings/proto/passage_embeddings_model_metadata.pb.h"
#include "components/optimization_guide/core/model_info.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/passage_embeddings/public/mojom/passage_embeddings.mojom.h"

namespace history_embeddings {

class PassageEmbeddingsServiceController {
 public:
  PassageEmbeddingsServiceController();
  virtual ~PassageEmbeddingsServiceController();

  // Launches the passage embeddings service.
  virtual void LaunchService() = 0;

  // Updates the paths needed for executing the passage embeddings model if the
  // paths provided are valid. The original paths will be erased regardless of
  // the validity of the new model paths. Returns true if the given model_info
  // is valid.
  bool MaybeUpdateModelPaths(
      base::optional_ref<const optimization_guide::ModelInfo> model_info);

  // Starts the service and calls `callback` with the embeddings. It is
  // guaranteed that the result will have the same number of elements as
  // `passages` when all embeddings executions succeed. Otherwise, will return
  // an empty vector.
  using GetEmbeddingsCallback = ComputePassagesEmbeddingsCallback;
  void GetEmbeddings(std::vector<std::string> passages,
                     GetEmbeddingsCallback callback);

  // Returns the embeddings model version;
  EmbedderMetadata GetEmbedderMetadata();

 protected:
  // Reset both service_remote_ and embedder_remote_.
  void ResetRemotes();

  mojo::Remote<passage_embeddings::mojom::PassageEmbeddingsService>
      service_remote_;
  mojo::Remote<passage_embeddings::mojom::PassageEmbedder> embedder_remote_;

 private:
  // Called when the model files on disks are opened and ready to be sent to
  // the service.
  void LoadModelsToService(
      mojo::PendingReceiver<passage_embeddings::mojom::PassageEmbedder> model,
      passage_embeddings::mojom::PassageEmbeddingsLoadModelsParamsPtr params);

  // Called when an attempt to load models to service finishes.
  void OnLoadModelsResult(bool success);

  // Called when the embedder_remote_ disconnects.
  void OnDisconnected();

  // Version of the embeddings model.
  int64_t model_version_;

  // Metadata of the embeddings model.
  std::optional<history_embeddings::proto::PassageEmbeddingsModelMetadata>
      model_metadata_;

  base::FilePath embeddings_model_path_;
  base::FilePath sp_model_path_;

  // Used to generate weak pointers to self.
  base::WeakPtrFactory<PassageEmbeddingsServiceController> weak_ptr_factory_{
      this};
};

}  // namespace history_embeddings

#endif  // COMPONENTS_HISTORY_EMBEDDINGS_PASSAGE_EMBEDDINGS_SERVICE_CONTROLLER_H_
