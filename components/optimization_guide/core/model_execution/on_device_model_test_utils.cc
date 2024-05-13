// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/on_device_model_test_utils.h"

#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "components/optimization_guide/core/model_execution/on_device_model_access_controller.h"
#include "components/optimization_guide/proto/model_execution.pb.h"

namespace optimization_guide {

using on_device_model::mojom::LoadModelResult;

FakeOnDeviceServiceSettings::FakeOnDeviceServiceSettings() = default;
FakeOnDeviceServiceSettings::~FakeOnDeviceServiceSettings() = default;

FakeOnDeviceSession::FakeOnDeviceSession(
    FakeOnDeviceServiceSettings* settings,
    std::optional<uint32_t> adaptation_model_id)
    : settings_(settings), adaptation_model_id_(adaptation_model_id) {}

FakeOnDeviceSession::~FakeOnDeviceSession() = default;

void FakeOnDeviceSession::AddContext(
    on_device_model::mojom::InputOptionsPtr input,
    mojo::PendingRemote<on_device_model::mojom::ContextClient> client) {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&FakeOnDeviceSession::AddContextInternal,
                                weak_factory_.GetWeakPtr(), std::move(input),
                                std::move(client)));
}

void FakeOnDeviceSession::Execute(
    on_device_model::mojom::InputOptionsPtr input,
    mojo::PendingRemote<on_device_model::mojom::StreamingResponder> response) {
  if (settings_->execute_delay.is_zero()) {
    ExecuteImpl(std::move(input), std::move(response));
    return;
  }
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&FakeOnDeviceSession::ExecuteImpl,
                     weak_factory_.GetWeakPtr(), std::move(input),
                     std::move(response)),
      settings_->execute_delay);
}

void FakeOnDeviceSession::GetSizeInTokens(const std::string& text,
                                          GetSizeInTokensCallback callback) {
  std::move(callback).Run(0);
}

void FakeOnDeviceSession::ExecuteImpl(
    on_device_model::mojom::InputOptionsPtr input,
    mojo::PendingRemote<on_device_model::mojom::StreamingResponder> response) {
  mojo::Remote<on_device_model::mojom::StreamingResponder> remote(
      std::move(response));
  for (const std::string& context : context_) {
    auto chunk = on_device_model::mojom::ResponseChunk::New();
    chunk->text = "Context: " + context + "\n";
    remote->OnResponse(std::move(chunk));
  }
  if (adaptation_model_id_) {
    auto chunk = on_device_model::mojom::ResponseChunk::New();
    chunk->text =
        "Adaptation model: " + base::NumberToString(*adaptation_model_id_) +
        "\n";
    remote->OnResponse(std::move(chunk));
  }

  if (settings_->model_execute_result.empty()) {
    auto chunk = on_device_model::mojom::ResponseChunk::New();
    chunk->text = "Input: " + input->text + "\n";
    if (input->top_k > 1) {
      chunk->text += "TopK: " + base::NumberToString(*input->top_k) +
                     ", Temp: " + base::NumberToString(*input->temperature) +
                     "\n";
    }
    remote->OnResponse(std::move(chunk));
  } else {
    for (const auto& text : settings_->model_execute_result) {
      auto chunk = on_device_model::mojom::ResponseChunk::New();
      chunk->text = text;
      remote->OnResponse(std::move(chunk));
    }
  }
  auto summary = on_device_model::mojom::ResponseSummary::New();
  remote->OnComplete(std::move(summary));
}

void FakeOnDeviceSession::AddContextInternal(
    on_device_model::mojom::InputOptionsPtr input,
    mojo::PendingRemote<on_device_model::mojom::ContextClient> client) {
  std::string suffix;
  std::string context = input->text;
  if (input->token_offset) {
    context.erase(context.begin(), context.begin() + *input->token_offset);
    suffix += " off:" + base::NumberToString(*input->token_offset);
  }
  if (input->max_tokens) {
    if (input->max_tokens < context.size()) {
      context.resize(*input->max_tokens);
    }
    suffix += " max:" + base::NumberToString(*input->max_tokens);
  }
  context_.push_back(context + suffix);
  uint32_t max_tokens = input->max_tokens.value_or(input->text.size());
  uint32_t token_offset = input->token_offset.value_or(0);
  if (client) {
    mojo::Remote<on_device_model::mojom::ContextClient> remote(
        std::move(client));
    remote->OnComplete(std::min(
        static_cast<uint32_t>(input->text.size()) - token_offset, max_tokens));
  }
}

FakeOnDeviceModel::FakeOnDeviceModel(
    FakeOnDeviceServiceSettings* settings,
    std::optional<uint32_t> adaptation_model_id)
    : settings_(settings), adaptation_model_id_(adaptation_model_id) {}

FakeOnDeviceModel::~FakeOnDeviceModel() = default;

void FakeOnDeviceModel::StartSession(
    mojo::PendingReceiver<on_device_model::mojom::Session> session) {
  // Mirror what the real OnDeviceModel does, which is only allow a single
  // Session.
  receivers_.Clear();
  receivers_.Add(
      std::make_unique<FakeOnDeviceSession>(settings_, adaptation_model_id_),
      std::move(session));
}

void FakeOnDeviceModel::DetectLanguage(const std::string& text,
                                       DetectLanguageCallback callback) {
  on_device_model::mojom::LanguageDetectionResultPtr language;
  if (text.find("esperanto") != std::string::npos) {
    language = on_device_model::mojom::LanguageDetectionResult::New("eo", 1.0);
  }
  std::move(callback).Run(std::move(language));
}

void FakeOnDeviceModel::ClassifyTextSafety(
    const std::string& text,
    ClassifyTextSafetyCallback callback) {
  auto safety_info = on_device_model::mojom::SafetyInfo::New();

  // Text is unsafe if it contains "unsafe".
  bool has_unsafe = text.find("unsafe") != std::string::npos;
  safety_info->class_scores.emplace_back(has_unsafe ? 0.8 : 0.2);

  bool has_reasonable = text.find("reasonable") != std::string::npos;
  safety_info->class_scores.emplace_back(has_reasonable ? 0.2 : 0.8);

  if (text.find("esperanto") != std::string::npos) {
    safety_info->language =
        on_device_model::mojom::LanguageDetectionResult::New("eo", 1.0);
  }

  std::move(callback).Run(std::move(safety_info));
}

void FakeOnDeviceModel::LoadAdaptation(
    on_device_model::mojom::LoadAdaptationParamsPtr params,
    mojo::PendingReceiver<on_device_model::mojom::OnDeviceModel> model,
    LoadAdaptationCallback callback) {
  auto test_model = std::make_unique<FakeOnDeviceModel>(
      settings_, ++settings_->adaptation_model_id_counter);
  model_adaptation_receivers_.Add(std::move(test_model), std::move(model));
  std::move(callback).Run(on_device_model::mojom::LoadModelResult::kSuccess);
}

FakeOnDeviceModelService::FakeOnDeviceModelService(
    mojo::PendingReceiver<on_device_model::mojom::OnDeviceModelService>
        receiver,
    FakeOnDeviceServiceSettings* settings)
    : settings_(settings), receiver_(this, std::move(receiver)) {}

FakeOnDeviceModelService::~FakeOnDeviceModelService() = default;

void FakeOnDeviceModelService::LoadModel(
    on_device_model::mojom::LoadModelParamsPtr params,
    mojo::PendingReceiver<on_device_model::mojom::OnDeviceModel> model,
    LoadModelCallback callback) {
  if (settings_->drop_connection_request) {
    std::move(callback).Run(settings_->load_model_result);
    return;
  }
  auto test_model = std::make_unique<FakeOnDeviceModel>(settings_);
  model_receivers_.Add(std::move(test_model), std::move(model));
  std::move(callback).Run(settings_->load_model_result);
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
void FakeOnDeviceModelService::LoadPlatformModel(
    const base::Uuid& uuid,
    mojo::PendingReceiver<on_device_model::mojom::OnDeviceModel> model,
    LoadModelCallback callback) {
  if (settings_->drop_connection_request) {
    std::move(callback).Run(settings_->load_model_result);
    return;
  }
  auto test_model = std::make_unique<FakeOnDeviceModel>(settings_);
  model_receivers_.Add(std::move(test_model), std::move(model));
  std::move(callback).Run(settings_->load_model_result);
}
#endif

void FakeOnDeviceModelService::GetEstimatedPerformanceClass(
    GetEstimatedPerformanceClassCallback callback) {
  std::move(callback).Run(on_device_model::mojom::PerformanceClass::kVeryHigh);
}

FakeOnDeviceModelServiceController::FakeOnDeviceModelServiceController(
    FakeOnDeviceServiceSettings* settings,
    std::unique_ptr<OnDeviceModelAccessController> access_controller,
    base::WeakPtr<OnDeviceModelComponentStateManager>
        on_device_component_state_manager)
    : OnDeviceModelServiceController(
          std::move(access_controller),
          std::move(on_device_component_state_manager)),
      settings_(settings) {}

FakeOnDeviceModelServiceController::~FakeOnDeviceModelServiceController() =
    default;

void FakeOnDeviceModelServiceController::LaunchService() {
  did_launch_service_ = true;
  service_remote_.reset();
  service_ = std::make_unique<FakeOnDeviceModelService>(
      service_remote_.BindNewPipeAndPassReceiver(), settings_);
}

}  // namespace optimization_guide
