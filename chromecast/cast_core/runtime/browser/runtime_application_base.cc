// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/cast_core/runtime/browser/runtime_application_base.h"

#include "base/bind.h"
#include "base/ranges/algorithm.h"
#include "base/task/bind_post_task.h"
#include "chromecast/browser/cast_web_service.h"
#include "chromecast/browser/cast_web_view_factory.h"
#include "chromecast/cast_core/grpc/grpc_status_or.h"
#include "chromecast/cast_core/runtime/browser/url_rewrite/url_request_rewrite_type_converters.h"
#include "chromecast/common/feature_constants.h"

namespace chromecast {

namespace {

// Parses renderer features.
const cast::common::Dictionary::Entry* FindEntry(
    const std::string& key,
    const cast::common::Dictionary& dict) {
  auto iter = base::ranges::find_if(
      dict.entries(), [&key](const auto& entry) { return entry.key() == key; });
  if (iter == dict.entries().end()) {
    return nullptr;
  }
  return &*iter;
}

}  // namespace

RuntimeApplicationBase::RuntimeApplicationBase(
    std::string cast_session_id,
    cast::common::ApplicationConfig app_config,
    mojom::RendererType renderer_type_used,
    CastWebService* web_service,
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : cast_session_id_(std::move(cast_session_id)),
      app_config_(std::move(app_config)),
      web_service_(web_service),
      task_runner_(std::move(task_runner)),
      renderer_type_(renderer_type_used) {
  DCHECK(web_service_);
  DCHECK(task_runner_);
}

RuntimeApplicationBase::~RuntimeApplicationBase() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!is_application_running_);
}

CastWebContents* RuntimeApplicationBase::GetCastWebContents() {
  DCHECK(cast_web_view_);
  return cast_web_view_->cast_web_contents();
}

const std::string& RuntimeApplicationBase::GetCastMediaServiceEndpoint() const {
  DCHECK(cast_media_service_grpc_endpoint_);
  return *cast_media_service_grpc_endpoint_;
}

const cast::common::ApplicationConfig& RuntimeApplicationBase::GetAppConfig()
    const {
  return app_config_;
}

const std::string& RuntimeApplicationBase::GetCastSessionId() const {
  return cast_session_id_;
}

void RuntimeApplicationBase::Load(cast::runtime::LoadApplicationRequest request,
                                  StatusCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!grpc_server_);

  if (request.runtime_application_service_info().grpc_endpoint().empty()) {
    std::move(callback).Run(
        grpc::Status(grpc::StatusCode::INVALID_ARGUMENT,
                     "Application service endpoint is missing"));
    return;
  }

  LOG(INFO) << "Loading application: " << *this;
  is_application_running_ = true;

  // Start the gRPC server.
  grpc_server_.emplace();
  grpc_server_->SetHandler<
      cast::v2::RuntimeApplicationServiceHandler::SetUrlRewriteRules>(
      base::BindPostTask(
          task_runner_,
          base::BindRepeating(&RuntimeApplicationBase::HandleSetUrlRewriteRules,
                              weak_factory_.GetWeakPtr())));
  grpc_server_->SetHandler<
      cast::v2::RuntimeMessagePortApplicationServiceHandler::PostMessage>(
      base::BindPostTask(
          task_runner_,
          base::BindRepeating(&RuntimeApplicationBase::HandlePostMessage,
                              weak_factory_.GetWeakPtr())));
  grpc_server_->Start(
      request.runtime_application_service_info().grpc_endpoint());
  LOG(INFO) << "Runtime application server started: " << *this << ", endpoint="
            << request.runtime_application_service_info().grpc_endpoint();

  // Initialize web view.
  cast_web_view_ = CreateCastWebView();

  // Set initial URL rewrite rules.
  url_rewrite::mojom::UrlRequestRewriteRulesPtr mojom_rules =
      mojo::ConvertTo<url_rewrite::mojom::UrlRequestRewriteRulesPtr>(
          request.url_rewrite_rules());
  GetCastWebContents()->SetUrlRewriteRules(std::move(mojom_rules));

  LOG(INFO) << "Successfully loaded: " << *this;
  std::move(callback).Run(grpc::Status::OK);
}

void RuntimeApplicationBase::Launch(
    cast::runtime::LaunchApplicationRequest request,
    StatusCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  LOG(INFO) << "Launching application: " << *this;

  if (request.core_application_service_info().grpc_endpoint().empty()) {
    std::move(callback).Run(
        grpc::Status(grpc::StatusCode::INVALID_ARGUMENT,
                     "Core application service endpoint is missing"));
    return;
  }
  if (request.cast_media_service_info().grpc_endpoint().empty()) {
    std::move(callback).Run(grpc::Status(grpc::StatusCode::INVALID_ARGUMENT,
                                         "Media service endpoint is missing"));
    return;
  }

  // Create stubs for Core*ApplicationServices.
  auto core_channel = grpc::CreateChannel(
      request.core_application_service_info().grpc_endpoint(),
      grpc::InsecureChannelCredentials());
  core_app_stub_.emplace(core_channel);
  core_message_port_app_stub_.emplace(core_channel);

  // Initialize MZ data.
  cast_media_service_grpc_endpoint_.emplace(
      request.cast_media_service_info().grpc_endpoint());

  // Report that Cast application launch is initiated.
  std::move(callback).Run(grpc::Status::OK);

  // Initiate application initialization flow where bindings and any extra setup
  // happens.
  InitializeApplication(
      base::BindOnce(&RuntimeApplicationBase::OnApplicationInitialized,
                     weak_factory_.GetWeakPtr()));
}

base::Value RuntimeApplicationBase::GetRendererFeatures() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const auto* entry = FindEntry(feature::kCastCoreRendererFeatures,
                                GetAppConfig().extra_features());
  if (!entry) {
    return base::Value();
  }
  DCHECK(entry->value().has_dictionary());

  base::Value::Dict renderer_features;
  for (const cast::common::Dictionary::Entry& feature :
       entry->value().dictionary().entries()) {
    base::Value::Dict dict;
    if (feature.has_value()) {
      DCHECK(feature.value().has_dictionary());
      for (const cast::common::Dictionary::Entry& feature_arg :
           feature.value().dictionary().entries()) {
        DCHECK(feature_arg.has_value());
        if (feature_arg.value().value_case() == cast::common::Value::kFlag) {
          dict.Set(feature_arg.key(), feature_arg.value().flag());
        } else if (feature_arg.value().value_case() ==
                   cast::common::Value::kText) {
          dict.Set(feature_arg.key(), feature_arg.value().text());
        } else {
          LOG(FATAL) << "No or unsupported value was set for the feature: "
                     << feature.key();
        }
      }
    }
    DVLOG(1) << "Renderer feature created: " << feature.key();
    renderer_features.Set(feature.key(), std::move(dict));
  }

  return base::Value(std::move(renderer_features));
}

bool RuntimeApplicationBase::GetIsAudioOnly() const {
  const auto* entry =
      FindEntry(feature::kCastCoreIsAudioOnly, GetAppConfig().extra_features());
  if (entry && entry->value().value_case() == cast::common::Value::kFlag) {
    return entry->value().flag();
  }
  return false;
}

bool RuntimeApplicationBase::GetEnforceFeaturePermissions() const {
  const auto* entry = FindEntry(feature::kCastCoreEnforceFeaturePermissions,
                                GetAppConfig().extra_features());
  if (entry && entry->value().value_case() == cast::common::Value::kFlag) {
    return entry->value().flag();
  }
  return false;
}

std::vector<int> RuntimeApplicationBase::GetFeaturePermissions() const {
  std::vector<int> feature_permissions;
  const auto* entry = FindEntry(feature::kCastCoreFeaturePermissions,
                                GetAppConfig().extra_features());
  if (entry) {
    DCHECK(entry->value().value_case() == cast::common::Value::kArray);
    base::ranges::for_each(
        entry->value().array().values(),
        [&feature_permissions](const cast::common::Value& value) {
          DCHECK(value.value_case() == cast::common::Value::kNumber);
          feature_permissions.push_back(value.number());
        });
  }
  return feature_permissions;
}

std::vector<std::string>
RuntimeApplicationBase::GetAdditionalFeaturePermissionOrigins() const {
  std::vector<std::string> feature_permission_origins;
  const auto* entry = FindEntry(feature::kCastCoreFeaturePermissionOrigins,
                                GetAppConfig().extra_features());
  if (entry) {
    DCHECK(entry->value().value_case() == cast::common::Value::kArray);
    base::ranges::for_each(
        entry->value().array().values(),
        [&feature_permission_origins](const cast::common::Value& value) {
          DCHECK(value.value_case() == cast::common::Value::kText);
          feature_permission_origins.push_back(value.text());
        });
  }
  return feature_permission_origins;
}

void RuntimeApplicationBase::OnApplicationInitialized() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  GetCastWebContents()->AddRendererFeatures(GetRendererFeatures());

  // TODO(b/203580094): Currently we assume the app is not audio only.
  GetCastWebContents()->SetAppProperties(
      GetAppConfig().app_id(), GetCastSessionId(), GetIsAudioOnly(),
      GetApplicationUrl(), GetEnforceFeaturePermissions(),
      GetFeaturePermissions(), GetAdditionalFeaturePermissionOrigins());
  GetCastWebContents()->LoadUrl(GetApplicationUrl());

  // Show the web view.
  cast_web_view_->window()->GrantScreenAccess();
  cast_web_view_->window()->CreateWindow(
      ::chromecast::mojom::ZOrder::APP,
      chromecast::VisibilityPriority::STICKY_ACTIVITY);
}

void RuntimeApplicationBase::HandlePostMessage(
    cast::web::Message request,
    cast::v2::RuntimeMessagePortApplicationServiceHandler::PostMessage::Reactor*
        reactor) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!is_application_running_) {
    reactor->Write(grpc::Status(grpc::StatusCode::NOT_FOUND,
                                "No active cast session for PostMessage"));
    return;
  }

  auto response_or = HandlePortMessage(std::move(request));
  if (response_or.ok()) {
    reactor->Write(std::move(response_or).value());
  } else {
    reactor->Write(response_or.status());
  }
}

void RuntimeApplicationBase::HandleSetUrlRewriteRules(
    cast::v2::SetUrlRewriteRulesRequest request,
    cast::v2::RuntimeApplicationServiceHandler::SetUrlRewriteRules::Reactor*
        reactor) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!is_application_running_) {
    reactor->Write(
        grpc::Status(grpc::StatusCode::NOT_FOUND,
                     "No active cast session for SetUrlRewriteRules"));
    return;
  }
  if (request.has_rules()) {
    url_rewrite::mojom::UrlRequestRewriteRulesPtr mojom_rules =
        mojo::ConvertTo<url_rewrite::mojom::UrlRequestRewriteRulesPtr>(
            request.rules());
    GetCastWebContents()->SetUrlRewriteRules(std::move(mojom_rules));
  }
  reactor->Write(cast::v2::SetUrlRewriteRulesResponse());
}

void RuntimeApplicationBase::SetApplicationState(
    cast::v2::ApplicationStatusRequest::State state,
    StatusCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(state == cast::v2::ApplicationStatusRequest::STARTED ||
         state == cast::v2::ApplicationStatusRequest::STOPPED);

  auto call = core_app_stub_->CreateCall<
      cast::v2::CoreApplicationServiceStub::SetApplicationStatus>();
  call.request().set_cast_session_id(GetCastSessionId());
  call.request().set_state(state);
  if (state == cast::v2::ApplicationStatusRequest::STOPPED) {
    call.request().set_stop_reason(
        cast::v2::ApplicationStatusRequest::USER_REQUEST);
  }
  std::move(call).InvokeAsync(base::BindOnce(
      [](StatusCallback callback,
         cast::utils::GrpcStatusOr<cast::v2::ApplicationStatusResponse>
             response_or) { std::move(callback).Run(response_or.status()); },
      std::move(callback)));
}

CastWebView::Scoped RuntimeApplicationBase::CreateCastWebView() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  mojom::CastWebViewParamsPtr params = mojom::CastWebViewParams::New();
  params->renderer_type = renderer_type_;
  params->handle_inner_contents = true;
  params->session_id = GetCastSessionId();
#if DCHECK_IS_ON()
  params->enabled_for_dev = true;
#endif
  CastWebView::Scoped cast_web_view =
      web_service_->CreateWebViewInternal(std::move(params));
  DCHECK(cast_web_view);
  return cast_web_view;
}

void RuntimeApplicationBase::StopApplication() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(grpc_server_);

  LOG(INFO) << "Stopping application: " << *this;

  if (!is_application_running_) {
    return;
  }
  is_application_running_ = false;

  if (core_app_stub_) {
    SetApplicationState(cast::v2::ApplicationStatusRequest_State_STOPPED,
                        base::DoNothing());
  }

  if (cast_web_view_) {
    GetCastWebContents()->ClosePage();
  }

  grpc_server_->Stop();
  grpc_server_.reset();
  LOG(INFO) << "Application is stopped: " << *this;
}

}  // namespace chromecast
