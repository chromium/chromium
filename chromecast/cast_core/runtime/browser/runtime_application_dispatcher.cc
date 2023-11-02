// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/cast_core/runtime/browser/runtime_application_dispatcher.h"

#include "base/check.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chromecast/browser/cast_content_window.h"
#include "chromecast/browser/cast_web_service.h"
#include "chromecast/cast_core/runtime/browser/streaming_runtime_application.h"
#include "chromecast/cast_core/runtime/browser/web_runtime_application.h"
#include "components/cast_receiver/browser/public/application_client.h"
#include "third_party/cast_core/public/src/proto/common/application_config.pb.h"
#include "third_party/openscreen/src/cast/common/public/cast_streaming_app_ids.h"

namespace chromecast {

RuntimeApplicationDispatcher::RuntimeApplicationDispatcher(
    PlatformFactory platform_factory,
    CastWebService* web_service,
    cast_receiver::ApplicationClient& application_client)
    : platform_(std::move(platform_factory).Run(*this, web_service)),
      web_service_(web_service),
      application_client_(application_client),
      task_runner_(base::SequencedTaskRunnerHandle::Get()) {
  DCHECK(web_service_);
  DCHECK(platform_);
}

RuntimeApplicationDispatcher::~RuntimeApplicationDispatcher() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  Stop();
}

bool RuntimeApplicationDispatcher::Start() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return platform_->Start();
}

void RuntimeApplicationDispatcher::Stop() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  loaded_apps_.clear();

  platform_->Stop();
}

void RuntimeApplicationDispatcher::LoadApplication(
    cast::runtime::LoadApplicationRequest request,
    StatusCallback callback,
    RuntimeApplicationPlatform::Factory runtime_application_factory) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::unique_ptr<RuntimeApplication> app;
  if (openscreen::cast::IsCastStreamingReceiverAppId(
          request.application_config().app_id())) {
    app = std::make_unique<StreamingRuntimeApplication>(
        request.cast_session_id(), request.application_config(), web_service_,
        task_runner_, *application_client_,
        std::move(runtime_application_factory));
  } else {
    app = std::make_unique<WebRuntimeApplication>(
        request.cast_session_id(), request.application_config(), web_service_,
        task_runner_, std::move(runtime_application_factory));
  }

  // TODO(b/232140331): Call this only when foreground app changes.
  application_client_->OnForegroundApplicationChanged(app.get());

  // Need to cache session_id as |request| object is moved.
  std::string session_id = request.cast_session_id();
  app->Load(std::move(request), std::move(callback));

  loaded_apps_.emplace(std::move(session_id), std::move(app));
}

void RuntimeApplicationDispatcher::LaunchApplication(
    cast::runtime::LaunchApplicationRequest request,
    StatusCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::string session_id = request.cast_session_id();
  auto iter = loaded_apps_.find(session_id);
  if (iter == loaded_apps_.end()) {
    std::move(callback).Run(false);
    return;
  }

  iter->second->Launch(
      std::move(request),
      base::BindOnce(&RuntimeApplicationDispatcher::OnApplicationLaunching,
                     weak_factory_.GetWeakPtr(), std::move(session_id),
                     std::move(callback)));
}

std::unique_ptr<RuntimeApplication>
RuntimeApplicationDispatcher::StopApplication(std::string session_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto iter = loaded_apps_.find(session_id);
  if (iter == loaded_apps_.end()) {
    return nullptr;
  }

  auto app = std::move(iter->second);
  loaded_apps_.erase(iter);

  // TODO(b/232140331): Call this only when foreground app changes.
  application_client_->OnForegroundApplicationChanged(nullptr);
  return app;
}

bool RuntimeApplicationDispatcher::HasApplication(
    const std::string& session_id) {
  return loaded_apps_.find(session_id) != loaded_apps_.end();
}

void RuntimeApplicationDispatcher::OnApplicationLaunching(
    std::string session_id,
    StatusCallback callback,
    cast_receiver::Status status) {
  if (!status) {
    LOG(INFO) << "Failed to launch application";
    auto iter = loaded_apps_.find(session_id);
    if (iter != loaded_apps_.end()) {
      loaded_apps_.erase(iter);

      // TODO(b/232140331): Call this only when foreground app changes.
      application_client_->OnForegroundApplicationChanged(nullptr);
    }
  }

  std::move(callback).Run(status);
}

}  // namespace chromecast
