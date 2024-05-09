// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_ON_DEVICE_MODEL_TEST_UTILS_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_ON_DEVICE_MODEL_TEST_UTILS_H_

#include <cstdint>

#include "build/build_config.h"
#include "components/optimization_guide/core/model_execution/on_device_model_service_controller.h"
#include "components/optimization_guide/proto/model_execution.pb.h"
#include "mojo/public/cpp/bindings/unique_receiver_set.h"
#include "services/on_device_model/public/cpp/model_assets.h"
#include "services/on_device_model/public/mojom/on_device_model.mojom.h"
#include "services/on_device_model/public/mojom/on_device_model_service.mojom.h"

namespace optimization_guide {

using on_device_model::mojom::LoadModelResult;

class FakeOnDeviceSession final : public on_device_model::mojom::Session {
 public:
  explicit FakeOnDeviceSession(std::optional<uint32_t> adaptation_model_id);
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

  std::vector<std::string> context_;
  std::optional<uint32_t> adaptation_model_id_;

  base::WeakPtrFactory<FakeOnDeviceSession> weak_factory_{this};
};

class FakeOnDeviceModel : public on_device_model::mojom::OnDeviceModel {
 public:
  explicit FakeOnDeviceModel(
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
  mojo::UniqueReceiverSet<on_device_model::mojom::Session> receivers_;
  mojo::UniqueReceiverSet<on_device_model::mojom::OnDeviceModel>
      model_adaptation_receivers_;

  std::optional<uint32_t> adaptation_model_id_;
};

class FakeOnDeviceModelService
    : public on_device_model::mojom::OnDeviceModelService {
 public:
  FakeOnDeviceModelService(
      mojo::PendingReceiver<on_device_model::mojom::OnDeviceModelService>
          receiver,
      LoadModelResult result,
      bool drop_connection_request);
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

  mojo::Receiver<on_device_model::mojom::OnDeviceModelService> receiver_;
  const LoadModelResult load_model_result_;
  const bool drop_connection_request_;
  mojo::UniqueReceiverSet<on_device_model::mojom::OnDeviceModel>
      model_receivers_;
};

class FakeOnDeviceModelServiceController
    : public OnDeviceModelServiceController {
 public:
  FakeOnDeviceModelServiceController(
      std::unique_ptr<OnDeviceModelAccessController> access_controller,
      base::WeakPtr<OnDeviceModelComponentStateManager>
          on_device_component_state_manager);

  void LaunchService() override;

  void clear_did_launch_service() { did_launch_service_ = false; }

  bool did_launch_service() const { return did_launch_service_; }

  void set_load_model_result(LoadModelResult result) {
    load_model_result_ = result;
  }

  void set_drop_connection_request(bool value) {
    drop_connection_request_ = value;
  }

  size_t on_device_model_receiver_count() const {
    return service_ ? service_->on_device_model_receiver_count() : 0;
  }

 private:
  ~FakeOnDeviceModelServiceController() override;

  LoadModelResult load_model_result_ = LoadModelResult::kSuccess;
  bool drop_connection_request_ = false;
  std::unique_ptr<FakeOnDeviceModelService> service_;
  bool did_launch_service_ = false;
};

class ScopedOnDeviceModelServiceTestSettings {
 public:
  ScopedOnDeviceModelServiceTestSettings();
  ~ScopedOnDeviceModelServiceTestSettings();

  // Sets the amount of delay that is added before the on-device model execution
  // response is sent.
  void SetExecuteDelay(base::TimeDelta delay);

  // Sets the on-device model execution response.
  void SetExecuteResult(const std::vector<std::string>& result);

 private:
  base::TimeDelta old_execute_delay_;
  std::vector<std::string> old_model_execute_result_;
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_ON_DEVICE_MODEL_TEST_UTILS_H_
