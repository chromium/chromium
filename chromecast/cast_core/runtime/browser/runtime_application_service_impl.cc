// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/cast_core/runtime/browser/runtime_application_service_impl.h"

#include "base/task/bind_post_task.h"
#include "chromecast/browser/cast_web_service.h"
#include "chromecast/browser/cast_web_view.h"
#include "chromecast/cast_core/grpc/grpc_status_or.h"
#include "chromecast/cast_core/runtime/browser/grpc_webui_controller_factory.h"
#include "chromecast/cast_core/runtime/browser/message_port_service_grpc.h"
#include "chromecast/cast_core/runtime/browser/runtime_application_base.h"
#include "chromecast/cast_core/runtime/browser/url_rewrite/url_request_rewrite_type_converters.h"

namespace chromecast {
namespace {

class CastContentWindowControls : public cast_receiver::ContentWindowControls,
                                  public CastContentWindow::Observer {
 public:
  CastContentWindowControls(CastContentWindow& content_window)
      : content_window_(content_window) {
    content_window_->AddObserver(this);
  }

  ~CastContentWindowControls() override {
    content_window_->RemoveObserver(this);
  }

  // cast_receiver::ContentWindowControls implementation.
  void ShowWindow() override {
    if (!was_window_created_) {
      content_window_->GrantScreenAccess();
      content_window_->CreateWindow(mojom::ZOrder::APP,
                                    VisibilityPriority::STICKY_ACTIVITY);
      was_window_created_ = true;
      return;
    }

    content_window_->RequestVisibility(VisibilityPriority::STICKY_ACTIVITY);
    content_window_->GrantScreenAccess();
  }

  void HideWindow() override {
    if (!was_window_created_) {
      content_window_->CreateWindow(mojom::ZOrder::APP,
                                    VisibilityPriority::HIDDEN);
      was_window_created_ = true;
      return;
    }

    content_window_->RequestVisibility(VisibilityPriority::HIDDEN);
    content_window_->RevokeScreenAccess();
  }

  void EnableTouchInput() override { content_window_->EnableTouchInput(true); }

  void DisableTouchInput() override {
    content_window_->EnableTouchInput(false);
  }

 private:
  // CastContentWindow::Observer implementation.
  void OnVisibilityChange(VisibilityType visibility_type) override {
    switch (visibility_type) {
      case VisibilityType::FULL_SCREEN:
      case VisibilityType::PARTIAL_OUT:
      case VisibilityType::TRANSIENTLY_HIDDEN:
        OnWindowShown();
        break;
      default:
        OnWindowHidden();
        break;
    }
  }

  bool was_window_created_ = false;

  base::raw_ref<CastContentWindow> content_window_;
};

}  // namespace

RuntimeApplicationServiceImpl::RuntimeApplicationServiceImpl(
    std::unique_ptr<RuntimeApplicationBase> runtime_application,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    CastWebService& web_service)
    : runtime_application_(std::move(runtime_application)),
      task_runner_(std::move(task_runner)),
      web_service_(web_service) {
  DCHECK(runtime_application_);
  DCHECK(task_runner_);

  runtime_application_->SetDelegate(*this);
}

RuntimeApplicationServiceImpl::~RuntimeApplicationServiceImpl() = default;

void RuntimeApplicationServiceImpl::Load(
    const cast::runtime::LoadApplicationRequest& request,
    StatusCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!grpc_server_);

  if (request.runtime_application_service_info().grpc_endpoint().empty()) {
    std::move(callback).Run(
        cast_receiver::Status(cast_receiver::StatusCode::kInvalidArgument,
                              "RuntimeApplication service info missing"));
    return;
  }

  // Start the gRPC server.
  grpc_server_.emplace();
  grpc_server_->SetHandler<
      cast::v2::RuntimeApplicationServiceHandler::SetUrlRewriteRules>(
      base::BindPostTask(
          task_runner_,
          base::BindRepeating(
              &RuntimeApplicationServiceImpl::HandleSetUrlRewriteRules,
              weak_factory_.GetWeakPtr())));
  grpc_server_
      ->SetHandler<cast::v2::RuntimeApplicationServiceHandler::SetMediaState>(
          base::BindPostTask(
              task_runner_,
              base::BindRepeating(
                  &RuntimeApplicationServiceImpl::HandleSetMediaState,
                  weak_factory_.GetWeakPtr())));
  grpc_server_
      ->SetHandler<cast::v2::RuntimeApplicationServiceHandler::SetVisibility>(
          base::BindPostTask(
              task_runner_,
              base::BindRepeating(
                  &RuntimeApplicationServiceImpl::HandleSetVisibility,
                  weak_factory_.GetWeakPtr())));
  grpc_server_
      ->SetHandler<cast::v2::RuntimeApplicationServiceHandler::SetTouchInput>(
          base::BindPostTask(
              task_runner_,
              base::BindRepeating(
                  &RuntimeApplicationServiceImpl::HandleSetTouchInput,
                  weak_factory_.GetWeakPtr())));
  grpc_server_->SetHandler<
      cast::v2::RuntimeMessagePortApplicationServiceHandler::PostMessage>(
      base::BindPostTask(
          task_runner_,
          base::BindRepeating(&RuntimeApplicationServiceImpl::HandlePostMessage,
                              weak_factory_.GetWeakPtr())));
  grpc_server_->Start(
      request.runtime_application_service_info().grpc_endpoint());
  LOG(INFO) << "Runtime application server started: endpoint="
            << request.runtime_application_service_info().grpc_endpoint();

  // TODO(vigeni): Consider extacting this into RuntimeApplicationBase as a
  // mojo.
  url_rewrite::mojom::UrlRequestRewriteRulesPtr mojom_rules =
      mojo::ConvertTo<url_rewrite::mojom::UrlRequestRewriteRulesPtr>(
          request.url_rewrite_rules());
  runtime_application_->SetUrlRewriteRules(std::move(mojom_rules));

  cast_web_view_ = CreateCastWebView();
  runtime_application_->Load(std::move(callback));
}

void RuntimeApplicationServiceImpl::Launch(
    const cast::runtime::LaunchApplicationRequest& request,
    StatusCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (request.core_application_service_info().grpc_endpoint().empty()) {
    std::move(callback).Run(
        cast_receiver::Status(cast_receiver::StatusCode::kInvalidArgument,
                              "CoreApplication service info missing"));
    return;
  }

  if (request.cast_media_service_info().grpc_endpoint().empty()) {
    std::move(callback).Run(
        cast_receiver::Status(cast_receiver::StatusCode::kInvalidArgument,
                              "CastMedia service info missing"));
    return;
  }

  // Create stubs for Core*ApplicationServices.
  auto core_channel = grpc::CreateChannel(
      request.core_application_service_info().grpc_endpoint(),
      grpc::InsecureChannelCredentials());
  core_app_stub_.emplace(core_channel);
  core_message_port_app_stub_.emplace(core_channel);

  // TODO(b/244455581): Configure multizone.

  runtime_application_->SetMediaState(request.media_state());
  runtime_application_->SetVisibility(request.visibility());
  runtime_application_->SetTouchInput(request.touch_input());

  runtime_application_->Launch(std::move(callback));
}

void RuntimeApplicationServiceImpl::Stop(
    const cast::runtime::StopApplicationRequest& request,
    StatusCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  runtime_application_->Stop(std::move(callback));
}

void RuntimeApplicationServiceImpl::HandlePostMessage(
    cast::web::Message request,
    cast::v2::RuntimeMessagePortApplicationServiceHandler::PostMessage::Reactor*
        reactor) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!runtime_application_->IsApplicationRunning()) {
    reactor->Write(grpc::Status(grpc::StatusCode::NOT_FOUND,
                                "No active cast session for PostMessage"));
    return;
  }

  const auto success =
      runtime_application_->OnMessagePortMessage(std::move(request));
  if (success) {
    cast::web::MessagePortStatus message_port_status;
    message_port_status.set_status(cast::web::MessagePortStatus::OK);
    reactor->Write(std::move(message_port_status));
  } else {
    reactor->Write(
        grpc::Status(grpc::StatusCode::UNKNOWN, "Failed to post message"));
  }
}

CastWebView::Scoped RuntimeApplicationServiceImpl::CreateCastWebView() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  mojom::CastWebViewParamsPtr params = mojom::CastWebViewParams::New();
  params->renderer_type = runtime_application_->GetRendererType();
  params->handle_inner_contents = true;
  params->session_id = runtime_application_->GetCastSessionId();
  params->is_remote_control_mode =
      runtime_application_->GetIsRemoteControlMode();
  params->activity_id = params->is_remote_control_mode
                            ? params->session_id
                            : runtime_application_->GetAppId();
  params->enabled_for_dev = runtime_application_->GetEnabledForDev();
  return web_service_->CreateWebViewInternal(std::move(params));
}

void RuntimeApplicationServiceImpl::HandleSetUrlRewriteRules(
    cast::v2::SetUrlRewriteRulesRequest request,
    cast::v2::RuntimeApplicationServiceHandler::SetUrlRewriteRules::Reactor*
        reactor) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!runtime_application_->IsApplicationRunning()) {
    reactor->Write(
        grpc::Status(grpc::StatusCode::NOT_FOUND,
                     "No active cast session for SetUrlRewriteRules"));
    return;
  }
  if (request.has_rules()) {
    url_rewrite::mojom::UrlRequestRewriteRulesPtr mojom_rules =
        mojo::ConvertTo<url_rewrite::mojom::UrlRequestRewriteRulesPtr>(
            request.rules());
    runtime_application_->SetUrlRewriteRules(std::move(mojom_rules));
  }
  reactor->Write(cast::v2::SetUrlRewriteRulesResponse());
}

void RuntimeApplicationServiceImpl::HandleSetMediaState(
    cast::v2::SetMediaStateRequest request,
    cast::v2::RuntimeApplicationServiceHandler::SetMediaState::Reactor*
        reactor) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  runtime_application_->SetMediaState(request.media_state());
  reactor->Write(cast::v2::SetMediaStateResponse());
}

void RuntimeApplicationServiceImpl::HandleSetVisibility(
    cast::v2::SetVisibilityRequest request,
    cast::v2::RuntimeApplicationServiceHandler::SetVisibility::Reactor*
        reactor) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  runtime_application_->SetVisibility(request.visibility());
  reactor->Write(cast::v2::SetVisibilityResponse());
}

void RuntimeApplicationServiceImpl::HandleSetTouchInput(
    cast::v2::SetTouchInputRequest request,
    cast::v2::RuntimeApplicationServiceHandler::SetTouchInput::Reactor*
        reactor) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  runtime_application_->SetTouchInput(request.touch_input());
  reactor->Write(cast::v2::SetTouchInputResponse());
}

void RuntimeApplicationServiceImpl::NotifyApplicationStarted() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(core_app_stub_);

  LOG(INFO) << "Application is started: " << *runtime_application_;

  auto call = core_app_stub_->CreateCall<
      cast::v2::CoreApplicationServiceStub::ApplicationStarted>();
  call.request().set_cast_session_id(runtime_application_->GetCastSessionId());
  std::move(call).InvokeAsync(base::BindOnce(
      [](cast::utils::GrpcStatusOr<cast::v2::ApplicationStartedResponse>
             response_or) {
        LOG_IF(ERROR, !response_or.ok())
            << "Failed to report that application started: "
            << response_or.ToString();
      }));
}

void RuntimeApplicationServiceImpl::NotifyApplicationStopped(
    cast::common::StopReason::Type stop_reason,
    int32_t net_error_code) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(core_app_stub_);

  LOG(INFO) << "Application is stopped: stop_reason=" << stop_reason << ", "
            << *runtime_application_;

  auto call = core_app_stub_->CreateCall<
      cast::v2::CoreApplicationServiceStub::ApplicationStopped>();
  call.request().set_cast_session_id(runtime_application_->GetCastSessionId());
  call.request().set_stop_reason(stop_reason);
  call.request().set_error_code(net_error_code);
  std::move(call).InvokeAsync(base::BindOnce(
      [](cast::utils::GrpcStatusOr<cast::v2::ApplicationStoppedResponse>
             response_or) {
        LOG_IF(ERROR, !response_or.ok())
            << "Failed to report that application stopped: "
            << response_or.ToString();
      }));

  grpc_server_->Stop();
  grpc_server_.reset();
}

void RuntimeApplicationServiceImpl::NotifyMediaPlaybackChanged(bool playing) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(core_app_stub_);

  DLOG(INFO) << "Media playback changed: playing=" << playing << ", "
            << *runtime_application_;

  auto call = core_app_stub_->CreateCall<
      cast::v2::CoreApplicationServiceStub::MediaPlaybackChanged>();
  call.request().set_cast_session_id(runtime_application_->GetCastSessionId());
  call.request().set_media_playback_state(
      playing ? cast::common::MediaPlaybackState::PLAYING
              : cast::common::MediaPlaybackState::STOPPED);
  std::move(call).InvokeAsync(base::BindOnce(
      [](cast::utils::GrpcStatusOr<cast::v2::MediaPlaybackChangedResponse>
             response_or) {
        LOG_IF(ERROR, !response_or.ok())
            << "Failed to report media playback changed state: "
            << response_or.ToString();
      }));
}

void RuntimeApplicationServiceImpl::GetAllBindings(
    GetAllBindingsCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(core_message_port_app_stub_);
  auto call = core_message_port_app_stub_->CreateCall<
      cast::v2::CoreMessagePortApplicationServiceStub::GetAll>();
  std::move(call).InvokeAsync(base::BindPostTask(
      task_runner_,
      base::BindOnce(&RuntimeApplicationServiceImpl::OnAllBindingsReceived,
                     weak_factory_.GetWeakPtr(), std::move(callback))));
}

std::unique_ptr<MessagePortService>
RuntimeApplicationServiceImpl::CreateMessagePortService() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(core_message_port_app_stub_);
  return std::make_unique<MessagePortServiceGrpc>(
      &core_message_port_app_stub_.value());
}

std::unique_ptr<content::WebUIControllerFactory>
RuntimeApplicationServiceImpl::CreateWebUIControllerFactory(
    std::vector<std::string> hosts) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(core_app_stub_);
  return std::make_unique<GrpcWebUiControllerFactory>(std::move(hosts),
                                                      &core_app_stub_.value());
}

content::WebContents* RuntimeApplicationServiceImpl::GetWebContents() {
  if (!cast_web_view_) {
    return nullptr;
  }

  return cast_web_view_->web_contents();
}

cast_receiver::ContentWindowControls*
RuntimeApplicationServiceImpl::GetContentWindowControls() {
  if (!cast_web_view_ || !cast_web_view_->window()) {
    return nullptr;
  }

  if (!content_window_controls_) {
    content_window_controls_ =
        std::make_unique<CastContentWindowControls>(*cast_web_view_->window());
  }

  return content_window_controls_.get();
}

void RuntimeApplicationServiceImpl::OnAllBindingsReceived(
    GetAllBindingsCallback callback,
    cast::utils::GrpcStatusOr<cast::bindings::GetAllResponse> response_or) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!response_or.ok()) {
    std::move(callback).Run(
        cast_receiver::Status(cast_receiver::StatusCode::kCancelled,
                              "Bad GrpcStatus: " + response_or.ToString()),
        std::vector<std::string>());
    return;
  }

  const cast::bindings::GetAllResponse& response = response_or.value();
  std::vector<std::string> bindings;
  bindings.reserve(response.bindings_size());
  for (int i = 0; i < response.bindings_size(); ++i) {
    bindings.emplace_back(response.bindings(i).before_load_script());
  }

  std::move(callback).Run(cast_receiver::OkStatus(), std::move(bindings));
}

}  // namespace chromecast
