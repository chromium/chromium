// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
    bool is_demo_mode,
    const std::string& chrome_version,
    std::unique_ptr<SparkyDelegate> sparky_delegate,
    std::unique_ptr<SystemInfoDelegate> system_info_delegate)
    : BaseProvider(url_loader_factory,
                   identity_manager,
                   is_demo_mode,
                   chrome_version),
      sparky_delegate_(std::move(sparky_delegate)),
      system_info_delegate_(std::move(system_info_delegate)) {}

SparkyProvider::SparkyProvider(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    signin::IdentityManager* identity_manager,
    std::unique_ptr<SparkyDelegate> sparky_delegate,
    std::unique_ptr<SystemInfoDelegate> system_info_delegate)
    : SparkyProvider(url_loader_factory,
                     identity_manager,
                     false,
                     std::string(),
                     std::move(sparky_delegate),
                     std::move(system_info_delegate)) {}

SparkyProvider::~SparkyProvider() = default;

void SparkyProvider::QuestionAndAnswer(
    const std::string& original_content,
    const std::vector<SparkyQAPair> QAHistory,
    const std::string& question,
    proto::Task task,
    std::unique_ptr<DiagnosticsData> diagnostics_data,
    SparkyShowAnswerCallback done_callback) {
  proto::Request request;
  request.set_feature_name(proto::FeatureName::CHROMEOS_SPARKY);

  auto* input_data = request.add_input_data();
  input_data->set_tag("sparky_context");

  auto* sparky_context_data = input_data->mutable_sparky_context_data();

  auto* input_text = sparky_context_data->add_q_and_a();
  input_text->set_tag("new_question");
  input_text->set_text(question);

  for (const auto& [previous_question, previous_answer] : QAHistory) {
    input_text = sparky_context_data->add_q_and_a();
    input_text->set_tag("previous_question");
    input_text->set_text(previous_question);

    input_text = sparky_context_data->add_q_and_a();
    input_text->set_tag("previous_answer");
    input_text->set_text(previous_answer);
  }

  sparky_context_data->set_task(task);
  sparky_context_data->set_page_contents(original_content);

  if (task == proto::Task::TASK_SETTINGS) {
    auto* settings_list = sparky_delegate_->GetSettingsList();
    if (settings_list) {
      auto* settings_data = sparky_context_data->mutable_settings_data();
      AddSettingsProto(*settings_list, settings_data);
    }
  } else if (task == proto::Task::TASK_DIAGNOSTICS && diagnostics_data) {
    auto* diagnostics_proto = sparky_context_data->mutable_diagnostics_data();
    AddDiagnosticsProto(std::move(diagnostics_data), diagnostics_proto);
  }

  MantaProtoResponseCallback internal_callback = base::BindOnce(
      &OnQAServerResponseOrErrorReceived,
      base::BindOnce(&SparkyProvider::OnResponseReceived,
                     weak_ptr_factory_.GetWeakPtr(), std::move(done_callback),
                     original_content, QAHistory, question));

  // TODO(b:338501686): MISSING_TRAFFIC_ANNOTATION should be resolved before
  // launch.
  RequestInternal(GURL{GetProviderEndpoint(false)}, kOauthConsumerName,
                  MISSING_TRAFFIC_ANNOTATION, request, MantaMetricType::kSparky,
                  std::move(internal_callback));
}

void SparkyProvider::OnResponseReceived(
    SparkyShowAnswerCallback done_callback,
    const std::string& original_content,
    const std::vector<SparkyQAPair> QAHistory,
    const std::string& question,
    std::unique_ptr<proto::SparkyResponse> sparky_response,
    manta::MantaStatus status) {
  if (status.status_code != manta::MantaStatusCode::kOk) {
    std::move(done_callback).Run("", status);
    return;
  }

  if (sparky_response->has_context_request()) {
    RequestAdditionalInformation(sparky_response->context_request(),
                                 original_content, QAHistory, question,
                                 std::move(done_callback), status);
    return;
  } else if (sparky_response->has_final_response()) {
    OnActionResponse(sparky_response->final_response(),
                     std::move(done_callback), status);
    return;
  }

  // Occurs if the response cannot be parsed correctly.
  std::move(done_callback).Run("", status);
  return;
}

void SparkyProvider::RequestAdditionalInformation(
    proto::ContextRequest context_request,
    const std::string& original_content,
    const std::vector<SparkyQAPair> QAHistory,
    const std::string& question,
    SparkyShowAnswerCallback done_callback,
    manta::MantaStatus status) {
  if (context_request.has_settings()) {
    if (!sparky_delegate_->GetSettingsList()->empty()) {
      QuestionAndAnswer(original_content, QAHistory, question,
                        proto::TASK_SETTINGS, nullptr,
                        std::move(done_callback));
      return;
    }
    std::move(done_callback).Run("Unable to find settings list", status);
    return;
  } else if (context_request.has_diagnostics()) {
    auto diagnostics_vector =
        ObtainDiagnosticsVectorFromProto(context_request.diagnostics());
    if (!diagnostics_vector.empty()) {
      system_info_delegate_->ObtainDiagnostics(
          diagnostics_vector,
          base::BindOnce(&SparkyProvider::OnDiagnosticsReceived,
                         weak_ptr_factory_.GetWeakPtr(), original_content,
                         QAHistory, question, std::move(done_callback),
                         status));
      return;
    }
    std::move(done_callback).Run("No diagnostics were requested", status);
    return;
  }

  // Occurs if no valid request can be found.
  std::move(done_callback).Run("", status);
}

void SparkyProvider::OnDiagnosticsReceived(
    const std::string& original_content,
    const std::vector<SparkyQAPair> QAHistory,
    const std::string& question,
    SparkyShowAnswerCallback done_callback,
    manta::MantaStatus status,
    std::unique_ptr<DiagnosticsData> diagnostics_data) {
  if (diagnostics_data) {
    QuestionAndAnswer(original_content, QAHistory, question,
                      proto::TASK_DIAGNOSTICS, std::move(diagnostics_data),
                      std::move(done_callback));
    return;
  }
  std::move(done_callback).Run("Unable to obtain the diagnostics data", status);
}

void SparkyProvider::OnActionResponse(proto::FinalResponse final_response,
                                      SparkyShowAnswerCallback done_callback,
                                      manta::MantaStatus status) {
  if (!final_response.answer().empty()) {
    auto answer = final_response.answer();
    if (final_response.has_action()) {
      auto action = final_response.action();
      if (action.has_settings()) {
        const bool setting_was_updated = UpdateSettings(action.settings());
        if (!setting_was_updated) {
          std::move(done_callback)
              .Run("Unable to update the setting for that value", status);
        }
      }
    }
    std::move(done_callback).Run(answer, status);
  } else {
    std::move(done_callback).Run("", status);
  }
}

bool SparkyProvider::UpdateSettings(proto::SettingsData settings) {
  int settings_length = settings.setting_size();
  // TODO (b:338483338) Add in error handling for the case where one setting is
  // set correctly, and a different one is not set correctly.
  bool has_set = false;
  for (int index = 0; index < settings_length; index++) {
    auto setting = settings.setting(index);
    std::unique_ptr<SettingsData> setting_data = nullptr;
    if (setting.type() == proto::SettingType::SETTING_TYPE_BOOL &&
        setting.value().has_bool_val()) {
      setting_data = std::make_unique<SettingsData>(
          setting.settings_id(), PrefType::kBoolean,
          std::make_optional<base::Value>(setting.value().bool_val()));
    } else if (setting.type() == proto::SettingType::SETTING_TYPE_DOUBLE &&
               setting.value().has_double_val()) {
      setting_data = std::make_unique<SettingsData>(
          setting.settings_id(), PrefType::kDouble,
          std::make_optional<base::Value>(setting.value().double_val()));
    } else if (setting.type() == proto::SettingType::SETTING_TYPE_INTEGER &&
               setting.value().has_int_val()) {
      setting_data = std::make_unique<SettingsData>(
          setting.settings_id(), PrefType::kInt,
          std::make_optional<base::Value>(setting.value().int_val()));
    } else if (setting.type() == proto::SettingType::SETTING_TYPE_STRING &&
               setting.value().has_text_val()) {
      setting_data = std::make_unique<SettingsData>(
          setting.settings_id(), PrefType::kString,
          std::make_optional<base::Value>(setting.value().text_val()));
    }

    if (setting_data != nullptr) {
      has_set = true;
      sparky_delegate_->SetSettings(std::move(setting_data));
    }
  }
  return has_set;
}

}  // namespace manta
