// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_ON_DEVICE_MODEL_TEST_UTILS_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_ON_DEVICE_MODEL_TEST_UTILS_H_

#include <cstdint>

#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "components/optimization_guide/core/model_execution/on_device_model_service_controller.h"
#include "components/optimization_guide/proto/model_execution.pb.h"
#include "mojo/public/cpp/bindings/unique_receiver_set.h"
#include "services/on_device_model/public/cpp/model_assets.h"
#include "services/on_device_model/public/mojom/on_device_model.mojom.h"
#include "services/on_device_model/public/mojom/on_device_model_service.mojom.h"

namespace optimization_guide {

using on_device_model::mojom::LoadModelResult;

// Hooks for tests to control the FakeOnDeviceService behavior.
struct FakeOnDeviceServiceSettings final {
  FakeOnDeviceServiceSettings();
  ~FakeOnDeviceServiceSettings();

  // If non-zero this amount of delay is added before the response is sent.
  base::TimeDelta execute_delay;

  // If non-empty, used as the output from Execute().
  std::vector<std::string> model_execute_result;

  // Counter to assign an identifier for the adaptation model.
  uint32_t adaptation_model_id_counter = 0;

  LoadModelResult load_model_result = LoadModelResult::kSuccess;

  bool drop_connection_request = false;

  void set_execute_delay(base::TimeDelta delay) { execute_delay = delay; }

  void set_execute_result(const std::vector<std::string>& result) {
    model_execute_result = result;
  }

  void set_load_model_result(LoadModelResult result) {
    load_model_result = result;
  }

  void set_drop_connection_request(bool value) {
    drop_connection_request = value;
  }
};

class FakeOnDeviceSession final : public on_device_model::mojom::Session {
 public:
  explicit FakeOnDeviceSession(FakeOnDeviceServiceSettings* settings,
                               std::optional<uint32_t> adaptation_model_id);
  ~FakeOnDeviceSession() override;

  // on_device_model::mojom::Session:
  void AddContext(on_device_model::mojom::InputOptionsPtr input,
                  mojo::PendingRemote<on_device_model::mojom::ContextClient>
                      client) override;

  void Execute(on_device_model::mojom::InputOptionsPtr input,
               mojo::PendingRemote<on_device_model::mojom::StreamingResponder>
                   response) override;

  void GetSizeInTokens(const std::string& text,
                       GetSizeInTokensCallback callback) override;

 private:
  void ExecuteImpl(
      on_device_model::mojom::InputOptionsPtr input,
      mojo::PendingRemote<on_device_model::mojom::StreamingResponder> response);

  void AddContextInternal(
      on_device_model::mojom::InputOptionsPtr input,
      mojo::PendingRemote<on_device_model::mojom::ContextClient> client);

  raw_ptr<FakeOnDeviceServiceSettings> settings_;
  std::optional<uint32_t> adaptation_model_id_;
  std::vector<std::string> context_;

  base::WeakPtrFactory<FakeOnDeviceSession> weak_factory_{this};
};

class FakeOnDeviceModel : public on_device_model::mojom::OnDeviceModel {
 public:
  explicit FakeOnDeviceModel(
      FakeOnDeviceServiceSettings* settings,
      std::optional<uint32_t> adaptation_model_id = std::nullopt);
  ~FakeOnDeviceModel() override;

  // on_device_model::mojom::OnDeviceModel:
  void StartSession(
      mojo::PendingReceiver<on_device_model::mojom::Session> session) override;

  void DetectLanguage(const std::string& text,
                      DetectLanguageCallback callback) override;

  void ClassifyTextSafety(const std::string& text,
                          ClassifyTextSafetyCallback callback) override;

  void LoadAdaptation(
      on_device_model::mojom::LoadAdaptationParamsPtr params,
      mojo::PendingReceiver<on_device_model::mojom::OnDeviceModel> model,
      LoadAdaptationCallback callback) override;

 private:
  raw_ptr<FakeOnDeviceServiceSettings> settings_;
  std::optional<uint32_t> adaptation_model_id_;

  mojo::UniqueReceiverSet<on_device_model::mojom::Session> receivers_;
  mojo::UniqueReceiverSet<on_device_model::mojom::OnDeviceModel>
      model_adaptation_receivers_;

};

class FakeOnDeviceModelService
    : public on_device_model::mojom::OnDeviceModelService {
 public:
  FakeOnDeviceModelService(
      mojo::PendingReceiver<on_device_model::mojom::OnDeviceModelService>
          receiver,
      FakeOnDeviceServiceSettings* settings);
  ~FakeOnDeviceModelService() override;

  size_t on_device_model_receiver_count() const {
    return model_receivers_.size();
  }

 private:
  // on_device_model::mojom::OnDeviceModelService:
  void LoadModel(
      on_device_model::mojom::LoadModelParamsPtr params,
      mojo::PendingReceiver<on_device_model::mojom::OnDeviceModel> model,
      LoadModelCallback callback) override;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  void LoadPlatformModel(
      const base::Uuid& uuid,
      mojo::PendingReceiver<on_device_model::mojom::OnDeviceModel> model,
      LoadModelCallback callback) override;
#endif
  void GetEstimatedPerformanceClass(
      GetEstimatedPerformanceClassCallback callback) override;

  raw_ptr<FakeOnDeviceServiceSettings> settings_;
  mojo::Receiver<on_device_model::mojom::OnDeviceModelService> receiver_;
  mojo::UniqueReceiverSet<on_device_model::mojom::OnDeviceModel>
      model_receivers_;
};

class FakeOnDeviceModelServiceController
    : public OnDeviceModelServiceController {
 public:
  FakeOnDeviceModelServiceController(
      FakeOnDeviceServiceSettings* settings,
      std::unique_ptr<OnDeviceModelAccessController> access_controller,
      base::WeakPtr<OnDeviceModelComponentStateManager>
          on_device_component_state_manager);

  void LaunchService() override;

  void clear_did_launch_service() { did_launch_service_ = false; }

  bool did_launch_service() const { return did_launch_service_; }

  size_t on_device_model_receiver_count() const {
    return service_ ? service_->on_device_model_receiver_count() : 0;
  }

 private:
  ~FakeOnDeviceModelServiceController() override;

  raw_ptr<FakeOnDeviceServiceSettings> settings_;
  std::unique_ptr<FakeOnDeviceModelService> service_;
  bool did_launch_service_ = false;
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_ON_DEVICE_MODEL_TEST_UTILS_H_
