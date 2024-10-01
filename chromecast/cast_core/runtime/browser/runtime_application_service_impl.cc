// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/cast_core/runtime/browser/runtime_application_service_impl.h"

#include <string>

#include "base/strings/stringprintf.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "build/build_config.h"
#include "build/chromecast_buildflags.h"
#include "chromecast/base/metrics/cast_metrics_helper.h"
#include "chromecast/base/version.h"
#include "chromecast/browser/cast_web_service.h"
#include "chromecast/browser/cast_web_view.h"
#include "chromecast/cast_core/grpc/grpc_status_or.h"
#include "chromecast/cast_core/runtime/browser/core_streaming_config_manager.h"
#include "chromecast/cast_core/runtime/browser/grpc_webui_controller_factory.h"
#include "chromecast/cast_core/runtime/browser/message_port_service_grpc.h"
#include "chromecast/cast_core/runtime/browser/url_rewrite/url_request_rewrite_type_converters.h"
#include "chromecast/common/feature_constants.h"
#include "components/cast_receiver/browser/public/content_window_controls.h"

namespace chromecast {
namespace {

class CastContentWindowControls : public cast_receiver::ContentWindowControls,
                                  public CastContentWindow::Observer {
 public:
  explicit CastContentWindowControls(CastContentWindow& content_window)
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

  raw_ref<CastContentWindow> content_window_;
};

cast::common::StopReason::Type ToProtoType(
    cast_receiver::EmbedderApplication::ApplicationStopReason reason) {
  switch (reason) {
    case cast_receiver::EmbedderApplication::ApplicationStopReason::kUndefined:
      return cast::common::StopReason::UNDEFINED;
    case cast_receiver::EmbedderApplication::ApplicationStopReason::
        kApplicationRequest:
      return cast::common::StopReason::APPLICATION_REQUEST;
    case cast_receiver::EmbedderApplication::ApplicationStopReason::
        kIdleTimeout:
      return cast::common::StopReason::IDLE_TIMEOUT;
    case cast_receiver::EmbedderApplication::ApplicationStopReason::
        kUserRequest:
      return cast::common::StopReason::USER_REQUEST;
    case cast_receiver::EmbedderApplication::ApplicationStopReason::kHttpError:
      return cast::common::StopReason::HTTP_ERROR;
    case cast_receiver::EmbedderApplication::ApplicationStopReason::
        kRuntimeError:
      return cast::common::StopReason::RUNTIME_ERROR;
  }
}

// Parses renderer features.
const cast::common::Dictionary::Entry* FindEntry(
    const std::string& key,
    const cast::common::Dictionary& dict) {
  auto iter = base::ranges::find(dict.entries(), key,
                                 &cast::common::Dictionary::Entry::key);
  if (iter == dict.entries().end()) {
    return nullptr;
  }
  return &*iter;
}

bool GetFlagEntry(const std::string& key,
                  const cast::common::Dictionary& dict,
                  bool default_value = false) {
  auto* entry = FindEntry(key, dict);
  if (!entry) {
    return default_value;
  }
  CHECK(entry->value().value_case() == cast::common::Value::kFlag);
  return entry->value().flag();
}

}  // namespace

RuntimeApplicationServiceImpl::RuntimeApplicationServiceImpl(
    std::unique_ptr<cast_receiver::RuntimeApplication> runtime_application,
    cast::common::ApplicationConfig config,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    CastWebService& web_service)
    : config_(std::move(config)),
      task_runner_(std::move(task_runner)),
      web_service_(web_service),
      runtime_application_(std::move(runtime_application)) {
  DCHECK(runtime_application_);
  DCHECK(task_runner_);
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

  metrics::CastMetricsHelper::GetInstance()->DidStartLoad(
      request.application_config().app_id());

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

  auto status = grpc_server_->Start(
      request.runtime_application_service_info().grpc_endpoint());
  if (!status.ok()) {
    LOG(ERROR) << "Failed to start runtime application server: status="
               << status.error_message();
    std::move(callback).Run(cast_receiver::Status(
        cast_receiver::StatusCode::kInternal, status.error_message()));
    return;
  }

  LOG(INFO) << "Runtime application server started: endpoint="
            << request.runtime_application_service_info().grpc_endpoint();

  // TODO(vigeni): Consider extacting this into RuntimeApplicationBase as a
  // mojo.
  url_rewrite::mojom::UrlRequestRewriteRulesPtr mojom_rules =
      mojo::ConvertTo<url_rewrite::mojom::UrlRequestRewriteRulesPtr>(
          request.url_rewrite_rules());
  runtime_application_->SetUrlRewriteRules(std::move(mojom_rules));

  cast_web_view_ = CreateCastWebView();
  metrics::CastMetricsHelper::GetInstance()->DidCompleteLoad(
      request.application_config().app_id(), request.cast_session_id());
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
  auto* cast_web_contents = cast_web_view_->cast_web_contents();
  DCHECK(cast_web_contents);
  CastWebContents::Observer::Observe(cast_web_contents);

  SetMediaBlocking(request.media_state());
  SetVisibility(request.visibility());
  SetTouchInput(request.touch_input());

  runtime_application_->Launch(std::move(callback));
}

void RuntimeApplicationServiceImpl::NavigateToPage(const GURL& url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto* cast_web_contents = cast_web_view_->cast_web_contents();
  DCHECK(cast_web_contents);

  cast_web_contents->AddRendererFeatures(GetRendererFeatures());
  cast_web_contents->SetAppProperties(
      runtime_application_->GetAppId(),
      runtime_application_->GetCastSessionId(), IsAudioOnly(), url,
      IsFeaturePermissionsEnforced(), std::vector<int>(),
      std::vector<std::string>());

  // Start loading the URL while JS visibility is disabled and no window is
  // created. This way users won't see the progressive UI updates as the page is
  // formed and styles are applied. The actual window will be created in
  // OnApplicationStarted when application is fully launched.
  cast_web_contents->LoadUrl(url);
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

  auto* message_port_service = GetMessagePortServiceGrpc();
  if (message_port_service) {
    auto status = message_port_service->HandleMessage(std::move(request));
    if (status) {
      cast::web::MessagePortStatus message_port_status;
      message_port_status.set_status(cast::web::MessagePortStatus::OK);
      reactor->Write(std::move(message_port_status));
      return;
    }

    LOG(INFO) << "Failed to post message port message: " << status;
  }

  reactor->Write(
      grpc::Status(grpc::StatusCode::UNKNOWN, "Failed to post message"));
}

CastWebView::Scoped RuntimeApplicationServiceImpl::CreateCastWebView() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  mojom::CastWebViewParamsPtr params = mojom::CastWebViewParams::New();
  params->use_media_blocker = true;
  params->keep_screen_on = false;
  params->gesture_priority = mojom::GesturePriority::MAIN_ACTIVITY;
  params->log_prefix =
      base::StringPrintf("Cast App (%s)", config_.app_id().c_str());
  params->is_remote_control_mode =
      GetFlagEntry(feature::kCastCoreIsRemoteControlMode,
                   config_.extra_features(), /*default_value=*/false);
  params->enabled_for_dev = IsEnabledForDev();
  params->enable_touch_input = IsTouchInputAllowed();
  params->log_js_console_messages =
      GetFlagEntry(feature::kCastCoreLogJsConsoleMessages,
                   config_.extra_features(), /*default_value=*/false);
  params->allow_media_access =
      GetFlagEntry(feature::kCastCoreAllowMediaAccess, config_.extra_features(),
                   /*default_value=*/false);
  params->force_720p_resolution =
      GetFlagEntry(feature::kCastCoreForce720p, config_.extra_features(),
                   /*default_value=*/false);
#if BUILDFLAG(ENABLE_CAST_RECEIVER) && BUILDFLAG(IS_LINUX)
  // Starboard-based (linux) cast receivers may not render their UI at 720p, so
  // we need to scale to the proper resolution. For example, a 4k TV may render
  // the window at 1920x1080, so a scaling factor of 1.5 is necessary for a 720p
  // app. Setting this to true would remove the scaling factor in
  // CastWebViewDefault::CastWebViewDefault (calling
  // OverridePrimaryDisplaySettings with a scaling factor of 1). As a result,
  // certain apps (e.g. Fandango at Home) would only cover part of the TV
  // screen.
  params->force_720p_resolution = false;
#endif  // BUILDFLAG(ENABLE_CAST_RECEIVER) && BUILDFLAG(IS_LINUX)
  params->turn_on_screen =
      GetFlagEntry(feature::kCastCoreTurnOnScreen, config_.extra_features(),
                   /*default_value=*/false);
  params->activity_id =
      params->is_remote_control_mode ? params->session_id : config_.app_id();
  return web_service_->CreateWebViewInternal(std::move(params));
}

void RuntimeApplicationServiceImpl::SetTouchInput(
    cast::common::TouchInput::Type state) {
  switch (state) {
    case cast::common::TouchInput::ENABLED:
      runtime_application_->SetTouchInputEnabled(true);
      break;
    case cast::common::TouchInput::DISABLED:
      runtime_application_->SetTouchInputEnabled(false);
      break;
    case cast::common::TouchInput::UNDEFINED:
      break;
    default:
      NOTREACHED();
  }
}

void RuntimeApplicationServiceImpl::SetVisibility(
    cast::common::Visibility::Type state) {
  switch (state) {
    case cast::common::Visibility::FULL_SCREEN:
      runtime_application_->SetVisibility(true);
      break;
    case cast::common::Visibility::HIDDEN:
      runtime_application_->SetVisibility(false);
      break;
    case cast::common::Visibility::UNDEFINED:
      break;
    default:
      NOTREACHED();
  }
}

void RuntimeApplicationServiceImpl::SetMediaBlocking(
    cast::common::MediaState::Type state) {
  switch (state) {
    case cast::common::MediaState::LOAD_BLOCKED:
      runtime_application_->SetMediaBlocking(true, true);
      break;
    case cast::common::MediaState::START_BLOCKED:
      runtime_application_->SetMediaBlocking(false, true);
      break;
    case cast::common::MediaState::UNBLOCKED:
      runtime_application_->SetMediaBlocking(false, false);
      break;
    case cast::common::MediaState::UNDEFINED:
      break;
    default:
      NOTREACHED();
  }
}

void RuntimeApplicationServiceImpl::OnStreamingApplicationError(
    cast_receiver::Status status) {
  LOG(ERROR) << "Error while running streaming application: " << status
             << ". Exiting application...";
  runtime_application_->Stop(StatusCallback{});
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

  SetMediaBlocking(request.media_state());
  reactor->Write(cast::v2::SetMediaStateResponse());
}

void RuntimeApplicationServiceImpl::HandleSetVisibility(
    cast::v2::SetVisibilityRequest request,
    cast::v2::RuntimeApplicationServiceHandler::SetVisibility::Reactor*
        reactor) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  SetVisibility(request.visibility());
  reactor->Write(cast::v2::SetVisibilityResponse());
}

void RuntimeApplicationServiceImpl::HandleSetTouchInput(
    cast::v2::SetTouchInputRequest request,
    cast::v2::RuntimeApplicationServiceHandler::SetTouchInput::Reactor*
        reactor) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  SetTouchInput(request.touch_input());
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
    ApplicationStopReason stop_reason,
    int32_t net_error_code) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(core_app_stub_);

  LOG(INFO) << "Application is stopped: stop_reason=" << stop_reason << ", "
            << *runtime_application_;

  auto proto_stop_reason = ToProtoType(stop_reason);
  auto call = core_app_stub_->CreateCall<
      cast::v2::CoreApplicationServiceStub::ApplicationStopped>();
  call.request().set_cast_session_id(runtime_application_->GetCastSessionId());
  call.request().set_stop_reason(proto_stop_reason);
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

MessagePortServiceGrpc*
RuntimeApplicationServiceImpl::GetMessagePortServiceGrpc() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!core_message_port_app_stub_) {
    return nullptr;
  }

  if (!message_port_service_) {
    message_port_service_ = std::make_unique<MessagePortServiceGrpc>(
        &core_message_port_app_stub_.value());
  }
  return message_port_service_.get();
}

cast_receiver::MessagePortService*
RuntimeApplicationServiceImpl::GetMessagePortService() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return GetMessagePortServiceGrpc();
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

#if !BUILDFLAG(IS_CAST_DESKTOP_BUILD)
cast_receiver::StreamingConfigManager*
RuntimeApplicationServiceImpl::GetStreamingConfigManager() {
  if (streaming_config_manager_) {
    return streaming_config_manager_.get();
  }

  auto* message_port_service = GetMessagePortService();
  if (!message_port_service) {
    return nullptr;
  }

  streaming_config_manager_ = std::make_unique<CoreStreamingConfigManager>(
      *message_port_service,
      base::BindOnce(
          &RuntimeApplicationServiceImpl::OnStreamingApplicationError,
          weak_factory_.GetWeakPtr()));
  return streaming_config_manager_.get();
}
#endif  // !BUILDFLAG(IS_CAST_DESKTOP_BUILD)

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

base::Value::Dict RuntimeApplicationServiceImpl::GetRendererFeatures() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const auto* entry =
      FindEntry(feature::kCastCoreRendererFeatures, config_.extra_features());

  base::Value::Dict renderer_features;
  if (!entry) {
    return renderer_features;
  }
  CHECK(entry->value().has_dictionary());

  for (const cast::common::Dictionary::Entry& feature :
       entry->value().dictionary().entries()) {
    base::Value::Dict dict;
    if (feature.has_value()) {
      CHECK(feature.value().has_dictionary());
      for (const cast::common::Dictionary::Entry& feature_arg :
           feature.value().dictionary().entries()) {
        CHECK(feature_arg.has_value());
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

  return renderer_features;
}

bool RuntimeApplicationServiceImpl::IsAudioOnly() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return GetFlagEntry(feature::kCastCoreIsAudioOnly, config_.extra_features(),
                      /*default_value=*/false);
}

bool RuntimeApplicationServiceImpl::IsEnabledForDev() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (CAST_IS_DEBUG_BUILD()) {
    return true;
  }
  const auto* entry =
      FindEntry(feature::kCastCoreRendererFeatures, config_.extra_features());
  if (!entry) {
    return false;
  }
  CHECK(entry->value().has_dictionary());

  return FindEntry(chromecast::feature::kEnableDevMode,
                   entry->value().dictionary()) != nullptr;
}

bool RuntimeApplicationServiceImpl::IsTouchInputAllowed() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const auto* entry =
      FindEntry(feature::kCastCoreRendererFeatures, config_.extra_features());
  if (!entry) {
    return false;
  }
  CHECK(entry->value().has_dictionary());
  const auto* enable_window_controls_entry =
      FindEntry(feature::kEnableWindowControls, entry->value().dictionary());
  return enable_window_controls_entry != nullptr;
}

bool RuntimeApplicationServiceImpl::IsFeaturePermissionsEnforced() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return GetFlagEntry(feature::kCastCoreEnforceFeaturePermissions,
                      config_.extra_features(),
                      /*default_value=*/false);
}

void RuntimeApplicationServiceImpl::InnerContentsCreated(
    CastWebContents* inner_contents,
    CastWebContents* outer_contents) {
  if (!config_.has_cast_web_app_config()) {
    return;
  }

  const std::string url = config_.cast_web_app_config().url();
  if (url.empty()) {
    return;
  }

#if DCHECK_IS_ON()
  base::Value::Dict features;
  base::Value::Dict dev_mode_config;
  dev_mode_config.Set(feature::kDevModeOrigin, url);
  features.Set(feature::kEnableDevMode, std::move(dev_mode_config));
  inner_contents->AddRendererFeatures(std::move(features));
#endif

  // Bind inner CastWebContents with the same session id and app id as the
  // root CastWebContents so that the same url rewrites are applied.
  inner_contents->SetAppProperties(
      runtime_application_->GetAppId(),
      runtime_application_->GetCastSessionId(), IsAudioOnly(), GURL(url),
      IsFeaturePermissionsEnforced(), std::vector<int>(),
      std::vector<std::string>());
  CastWebContents::Observer::Observe(inner_contents);
}

}  // namespace chromecast
