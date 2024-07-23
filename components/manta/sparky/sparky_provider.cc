// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/354693352): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/manta/sparky/sparky_provider.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/check.h"
#include "base/containers/fixed_flat_map.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/endpoint_fetcher/endpoint_fetcher.h"
#include "components/manta/base_provider.h"
#include "components/manta/manta_service.h"
#include "components/manta/manta_service_callbacks.h"
#include "components/manta/manta_status.h"
#include "components/manta/proto/manta.pb.h"
#include "components/manta/proto/sparky.pb.h"
#include "components/manta/sparky/sparky_delegate.h"
#include "components/manta/sparky/sparky_util.h"
#include "components/manta/sparky/system_info_delegate.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace manta {

namespace {

// TODO (b/336703051) Update with new Oauth.
constexpr char kOauthConsumerName[] = "manta_sparky";

// Handles the QA response from the server.
void OnQAServerResponseOrErrorReceived(
    SparkyProvider::SparkyProtoResponseCallback callback,
    std::unique_ptr<proto::Response> manta_response,
    MantaStatus manta_status) {
  if (manta_status.status_code != MantaStatusCode::kOk) {
    CHECK(manta_response == nullptr);
    std::move(callback).Run(nullptr, std::move(manta_status));
    return;
  }

  CHECK(manta_response != nullptr);
  if (manta_response->output_data_size() < 1 ||
      !manta_response->output_data(0).has_sparky_response()) {
    std::string message = std::string();

    // Tries to find more information from filtered_data
    if (manta_response->filtered_data_size() > 0 &&
        manta_response->filtered_data(0).is_output_data()) {
      message = base::StringPrintf(
          "filtered output for: %s",
          proto::FilteredReason_Name(manta_response->filtered_data(0).reason())
              .c_str());
    }
    std::move(callback).Run(std::make_unique<proto::SparkyResponse>(),
                            {MantaStatusCode::kBlockedOutputs, message});
    return;
  }

  std::move(callback).Run(std::make_unique<proto::SparkyResponse>(
                              manta_response->output_data(0).sparky_response()),
                          std::move(manta_status));
}

}  // namespace

SparkyProvider::SparkyProvider(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    signin::IdentityManager* identity_manager,
    const ProviderParams& provider_params,
    std::unique_ptr<SparkyDelegate> sparky_delegate,
    std::unique_ptr<SystemInfoDelegate> system_info_delegate)
    : BaseProvider(url_loader_factory, identity_manager, provider_params),
      sparky_delegate_(std::move(sparky_delegate)),
      system_info_delegate_(std::move(system_info_delegate)) {}

SparkyProvider::SparkyProvider(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    signin::IdentityManager* identity_manager,
    std::unique_ptr<SparkyDelegate> sparky_delegate,
    std::unique_ptr<SystemInfoDelegate> system_info_delegate)
    : BaseProvider(url_loader_factory, identity_manager),
      sparky_delegate_(std::move(sparky_delegate)),
      system_info_delegate_(std::move(system_info_delegate)) {}

SparkyProvider::~SparkyProvider() = default;

void SparkyProvider::QuestionAndAnswer(
    std::unique_ptr<SparkyContext> sparky_context,
    SparkyShowAnswerCallback done_callback) {
  sparky_delegate_->GetScreenshot(base::BindOnce(
      &SparkyProvider::OnScreenshotObtained, weak_ptr_factory_.GetWeakPtr(),
      std::move(sparky_context), std::move(done_callback)));
}

void SparkyProvider::OnScreenshotObtained(
    std::unique_ptr<SparkyContext> sparky_context,
    SparkyShowAnswerCallback done_callback,
    scoped_refptr<base::RefCountedMemory> jpeg_screenshot) {
  proto::Request request;
  request.set_feature_name(proto::FeatureName::CHROMEOS_SPARKY);

  auto* input_data = request.add_input_data();
  input_data->set_tag("sparky_context");

  auto* sparky_context_data = input_data->mutable_sparky_context_data();

  AddDialogToSparkyContext(sparky_context->dialog, sparky_context_data);

  sparky_context_data->set_task(sparky_context->task);
  if (sparky_context->page_content.has_value()) {
    sparky_context_data->set_page_contents(
        sparky_context->page_content.value());
  }
  if (sparky_context->page_url.has_value()) {
    sparky_context_data->set_page_url(sparky_context->page_url.value());
  }

  if (jpeg_screenshot) {
    proto::Image* image_proto = sparky_context_data->mutable_screenshot();

    // This copies the raw data from jpeg_screenshot (handling constness).
    const uint8_t* data_ptr =
        reinterpret_cast<const uint8_t*>(jpeg_screenshot->front());
    size_t data_size = jpeg_screenshot->size();
    // TODO(crbug.com/354693352): Explicit size when making string is unsafe.
    image_proto->set_serialized_bytes(std::string(
        data_ptr, data_ptr + data_size));  // Construct string from data
  }
  auto* apps_data = sparky_context_data->mutable_apps_data();
  AddAppsData(sparky_delegate_->GetAppsList(), apps_data);

  if (sparky_context->collect_settings) {
    auto* settings_list = sparky_delegate_->GetSettingsList();
    if (settings_list) {
      auto* settings_data = sparky_context_data->mutable_settings_data();
      AddSettingsProto(*settings_list, settings_data);
    }
  }
  if (sparky_context->diagnostics_data) {
    auto* diagnostics_proto = sparky_context_data->mutable_diagnostics_data();
    AddDiagnosticsProto(sparky_context->diagnostics_data, diagnostics_proto);
  }

  // This parameter contains the address of one of the backends which the
  // request is passed through to once it is pushed up in a manta request.
  if (sparky_context->server_url) {
    proto::ServerConfig* server_config =
        sparky_context_data->mutable_server_config();
    server_config->set_server_url(sparky_context->server_url.value());
  }

  MantaProtoResponseCallback internal_callback = base::BindOnce(
      &OnQAServerResponseOrErrorReceived,
      base::BindOnce(&SparkyProvider::OnResponseReceived,
                     weak_ptr_factory_.GetWeakPtr(), std::move(done_callback),
                     std::move(sparky_context)));

  // TODO(b:338501686): MISSING_TRAFFIC_ANNOTATION should be resolved before
  // launch.
  RequestInternal(GURL{GetProviderEndpoint(false)}, kOauthConsumerName,
                  MISSING_TRAFFIC_ANNOTATION, request, MantaMetricType::kSparky,
                  std::move(internal_callback));
}

void SparkyProvider::OnResponseReceived(
    SparkyShowAnswerCallback done_callback,
    std::unique_ptr<SparkyContext> sparky_context,
    std::unique_ptr<proto::SparkyResponse> sparky_response,
    manta::MantaStatus status) {
  if (status.status_code != manta::MantaStatusCode::kOk) {
    std::move(done_callback).Run(status, nullptr);
    return;
  }

  if (sparky_response->has_context_request()) {
    RequestAdditionalInformation(sparky_response->context_request(),
                                 std::move(sparky_context),
                                 std::move(done_callback), status);
    return;
  }
  if (sparky_response->has_latest_reply()) {
    OnDialogResponse(std::move(sparky_context), sparky_response->latest_reply(),
                     std::move(done_callback), status);
    return;
  }

  // Occurs if the response cannot be parsed correctly.
  std::move(done_callback).Run(status, nullptr);
  return;
}

void SparkyProvider::RequestAdditionalInformation(
    proto::ContextRequest context_request,
    std::unique_ptr<SparkyContext> sparky_context,
    SparkyShowAnswerCallback done_callback,
    manta::MantaStatus status) {
  if (context_request.has_settings()) {
    if (!sparky_delegate_->GetSettingsList()->empty()) {
      sparky_context->collect_settings = true;
      sparky_context->task = proto::TASK_SETTINGS;
      QuestionAndAnswer(std::move(sparky_context), std::move(done_callback));
      return;
    }
    std::move(done_callback).Run(status, nullptr);
    return;
  }
  if (context_request.has_diagnostics()) {
    auto diagnostics_vector =
        ObtainDiagnosticsVectorFromProto(context_request.diagnostics());
    if (!diagnostics_vector.empty()) {
      system_info_delegate_->ObtainDiagnostics(
          diagnostics_vector,
          base::BindOnce(&SparkyProvider::OnDiagnosticsReceived,
                         weak_ptr_factory_.GetWeakPtr(),
                         std::move(sparky_context), std::move(done_callback),
                         status));
      return;
    }
    std::move(done_callback).Run(status, nullptr);
    return;
  }

  // Occurs if no valid request can be found.
  std::move(done_callback).Run(status, nullptr);
}

void SparkyProvider::OnDiagnosticsReceived(
    std::unique_ptr<SparkyContext> sparky_context,
    SparkyShowAnswerCallback done_callback,
    manta::MantaStatus status,
    std::unique_ptr<DiagnosticsData> diagnostics_data) {
  if (diagnostics_data) {
    sparky_context->diagnostics_data = std::make_optional(*diagnostics_data);
    sparky_context->task = proto::TASK_DIAGNOSTICS;
    QuestionAndAnswer(std::move(sparky_context), std::move(done_callback));
    return;
  }
  std::move(done_callback).Run(status, nullptr);
}

void SparkyProvider::OnDialogResponse(std::unique_ptr<SparkyContext>,
                                      proto::Turn latest_reply,
                                      SparkyShowAnswerCallback done_callback,
                                      manta::MantaStatus status) {
  // If the response does not contain any dialog then return an error.
  if (!latest_reply.has_message()) {
    std::move(done_callback).Run(status, nullptr);
    return;
  }
  if (latest_reply.action_size() > 0) {
    auto actions_repeated_field = latest_reply.action();
    for (const proto::Action& action : actions_repeated_field) {
      if (action.has_update_setting()) {
        std::unique_ptr<SettingsData> setting_data =
            ObtainSettingFromProto(action.update_setting());
        if (!setting_data) {
          // Return an error if the setting cannot be converted correctly from
          // the proto.
          DVLOG(1) << "Invalid setting action requested.";
          std::move(done_callback).Run(status, nullptr);
          return;
        }
        sparky_delegate_->SetSettings(std::move(setting_data));
      }
      if (action.has_launch_app_id()) {
        sparky_delegate_->LaunchApp(action.launch_app_id());
      }
    }
  }

  DialogTurn latest_dialog_struct = ConvertDialogToStruct(&latest_reply);
  std::move(done_callback).Run(status, &latest_dialog_struct);
}

}  // namespace manta
