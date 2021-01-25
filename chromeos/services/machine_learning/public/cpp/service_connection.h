// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_MACHINE_LEARNING_PUBLIC_CPP_SERVICE_CONNECTION_H_
#define CHROMEOS_SERVICES_MACHINE_LEARNING_PUBLIC_CPP_SERVICE_CONNECTION_H_

#include "chromeos/services/machine_learning/public/mojom/machine_learning_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace chromeos {
namespace machine_learning {

// Encapsulates a connection to the Chrome OS ML Service daemon via its Mojo
// interface.
//
// Usage for BindMachineLearningService:
//   mojo::Remote<mojom::MachineLearningService> ml_service;
//   chromeos::machine_learning::ServiceConnection::GetInstance()
//       ->BindMachineLearningService(
//             ml_service.BindNewPipeAndPassReceiver());
//   // Use ml_service to LoadBuiltinModel(), LoadFlatBufferModel() etc. e.g
//   ml_service->LoadBuiltinModel(...);
//
// Usage for GetMachineLearningService:
//   chromeos::machine_learning::ServiceConnection::GetInstance()
//       ->GetMachineLearningService()
//       .LoadBuiltinModel(...);
//
// Usage for Built-in models (will be deprecated soon):
//   mojo::Remote<chromeos::machine_learning::mojom::Model> model;
//   chromeos::machine_learning::mojom::BuiltinModelSpecPtr spec =
//       chromeos::machine_learning::mojom::BuiltinModelSpec::New();
//   spec->id = ...;
//   chromeos::machine_learning::ServiceConnection::GetInstance()
//       ->LoadBuiltinModel(std::move(spec), model.BindNewPipeAndPassReceiver(),
//                          base::BindOnce(&MyCallBack));
//   // Use |model| or wait for |MyCallBack|.
// Usage for Flatbuffer models (will be deprecated soon):
//   mojo::Remote<chromeos::machine_learning::mojom::Model> model;
//   chromeos::machine_learning::mojom::FlatBufferModelSpecPtr spec =
//       chromeos::machine_learning::mojom::FlatBufferModelSpec::New();
//   spec->model_string = ...;
//   spec->inputs = ...;
//   spec->outputs = ...;
//   spec->metrics_model_name = ...;
//   chromeos::machine_learning::ServiceConnection::GetInstance()
//       ->LoadFlatBufferModel(std::move(spec),
//                             model.BindNewPipeAndPassReceiver(),
//                             base::BindOnce(&MyCallBack));
//
// Sequencing: can be called from any sequence.
class ServiceConnection {
 public:
  static ServiceConnection* GetInstance();
  // Overrides the result of GetInstance() for use in tests.
  // Does not take ownership of |fake_service_connection|.
  // Note: Caller is responsible for calling Initialize() on
  // `fake_service_connection`.
  static void UseFakeServiceConnectionForTesting(
      ServiceConnection* fake_service_connection);

  // Gets the primordial top-level machine learning service interface.
  // Must be called from the sequence that the instance is created on.
  virtual mojom::MachineLearningService& GetMachineLearningService() = 0;

  // Binds the receiver to a Clone of the primordial top-level interface.
  // May be called from any sequence.
  virtual void BindMachineLearningService(
      mojo::PendingReceiver<mojom::MachineLearningService> receiver) = 0;

  // Call this once at startup (e.g. PostBrowserStart) on the sequence that
  // should own the Mojo connection to MachineLearningService (e.g. UI thread).
  virtual void Initialize() = 0;

  // Instruct ML daemon to load the builtin model specified in |spec|, binding a
  // Model implementation to |receiver|. Bootstraps the initial Mojo connection
  // to the daemon if necessary.
  virtual void LoadBuiltinModel(
      mojom::BuiltinModelSpecPtr spec,
      mojo::PendingReceiver<mojom::Model> receiver,
      mojom::MachineLearningService::LoadBuiltinModelCallback
          result_callback) = 0;

  // Instruct ML daemon to load the flatbuffer model specified in |spec|,
  // binding a Model implementation to |receiver|. Bootstraps the initial Mojo
  // connection to the daemon if necessary.
  virtual void LoadFlatBufferModel(
      mojom::FlatBufferModelSpecPtr spec,
      mojo::PendingReceiver<mojom::Model> receiver,
      mojom::MachineLearningService::LoadFlatBufferModelCallback
          result_callback) = 0;

  // Instruct ML daemon to load the TextClassifier model, binding a
  // TextClassifier implementation to |receiver|. Bootstraps the initial Mojo
  // connection to the daemon if necessary.
  virtual void LoadTextClassifier(
      mojo::PendingReceiver<mojom::TextClassifier> receiver,
      mojom::MachineLearningService::LoadTextClassifierCallback
          result_callback) = 0;

  // Instruct ML daemon to load the Handwriting model with the given |spec|,
  // binding a Handwriting implementation to |receiver|. Bootstraps the initial
  // Mojo connection to the daemon if necessary.
  virtual void LoadHandwritingModel(
      mojom::HandwritingRecognizerSpecPtr spec,
      mojo::PendingReceiver<mojom::HandwritingRecognizer> receiver,
      mojom::MachineLearningService::LoadHandwritingModelCallback
          result_callback) = 0;

  // Same as LoadHandwritingModel, but will be deprecated and removed soon.
  virtual void LoadHandwritingModelWithSpec(
      mojom::HandwritingRecognizerSpecPtr spec,
      mojo::PendingReceiver<mojom::HandwritingRecognizer> receiver,
      mojom::MachineLearningService::LoadHandwritingModelWithSpecCallback
          result_callback) = 0;

  // Instruct ML daemon to load the Grammar model, binding a GrammarChecker
  // implementation to |receiver|. Bootstraps the initial Mojo connection to the
  // daemon if necessary.
  virtual void LoadGrammarChecker(
      mojo::PendingReceiver<mojom::GrammarChecker> receiver,
      mojom::MachineLearningService::LoadGrammarCheckerCallback
          result_callback) = 0;

  // Instruct ML daemon to load a SODA model.
  virtual void LoadSpeechRecognizer(
      mojom::SodaConfigPtr soda_config,
      mojo::PendingRemote<mojom::SodaClient> soda_client,
      mojo::PendingReceiver<mojom::SodaRecognizer> soda_recognizer,
      mojom::MachineLearningService::LoadSpeechRecognizerCallback callback) = 0;

 protected:
  ServiceConnection() = default;
  virtual ~ServiceConnection() {}
};

}  // namespace machine_learning
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_MACHINE_LEARNING_PUBLIC_CPP_SERVICE_CONNECTION_H_
