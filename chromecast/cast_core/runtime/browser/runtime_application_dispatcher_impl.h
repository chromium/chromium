// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_CAST_CORE_RUNTIME_BROWSER_RUNTIME_APPLICATION_DISPATCHER_IMPL_H_
#define CHROMECAST_CAST_CORE_RUNTIME_BROWSER_RUNTIME_APPLICATION_DISPATCHER_IMPL_H_

#include <memory>

#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/sequence_checker.h"
#include "chromecast/cast_core/runtime/browser/runtime_application_base.h"
#include "chromecast/cast_core/runtime/browser/runtime_application_dispatcher.h"
#include "chromecast/cast_core/runtime/browser/streaming_runtime_application.h"
#include "chromecast/cast_core/runtime/browser/web_runtime_application.h"
#include "components/cast_receiver/browser/public/application_client.h"
#include "components/cast_receiver/browser/public/application_config.h"
#include "third_party/openscreen/src/cast/common/public/cast_streaming_app_ids.h"

namespace chromecast {

template <typename TRuntimeApplicationPlatform>
class RuntimeApplicationDispatcherImpl
    : public RuntimeApplicationDispatcher<TRuntimeApplicationPlatform> {
 public:
  // |application_client| is expected to persist for the lifetime of this
  // instance.
  explicit RuntimeApplicationDispatcherImpl(
      cast_receiver::ApplicationClient& application_client);
  ~RuntimeApplicationDispatcherImpl() override = default;

 private:
  using RuntimeApplicationPlatformFactory = RuntimeApplicationDispatcher<
      TRuntimeApplicationPlatform>::RuntimeApplicationPlatformFactory;

  // RuntimeApplicationDispatcher implementation.
  TRuntimeApplicationPlatform* CreateApplication(
      std::string session_id,
      cast_receiver::ApplicationConfig app_config,
      RuntimeApplicationPlatformFactory factory) override;
  TRuntimeApplicationPlatform* GetApplication(
      const std::string& session_id) override;
  std::unique_ptr<TRuntimeApplicationPlatform> DestroyApplication(
      const std::string& session_id) override;

  SEQUENCE_CHECKER(sequence_checker_);
  base::raw_ref<cast_receiver::ApplicationClient> const application_client_;

  base::flat_map<std::string, std::unique_ptr<TRuntimeApplicationPlatform>>
      loaded_apps_;
};

template <typename TRuntimeApplicationPlatform>
RuntimeApplicationDispatcherImpl<TRuntimeApplicationPlatform>::
    RuntimeApplicationDispatcherImpl(
        cast_receiver::ApplicationClient& application_client)
    : application_client_(application_client) {}

template <typename TRuntimeApplicationPlatform>
TRuntimeApplicationPlatform*
RuntimeApplicationDispatcherImpl<TRuntimeApplicationPlatform>::
    CreateApplication(std::string session_id,
                      cast_receiver::ApplicationConfig app_config,
                      RuntimeApplicationPlatformFactory factory) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::unique_ptr<RuntimeApplicationBase> app;
  if (openscreen::cast::IsCastStreamingReceiverAppId(app_config.app_id)) {
    app = std::make_unique<StreamingRuntimeApplication>(
        session_id, std::move(app_config), *application_client_);
  } else {
    app = std::make_unique<WebRuntimeApplication>(
        session_id, std::move(app_config), *application_client_);
  }

  // TODO(b/232140331): Call this only when foreground app changes.
  application_client_->OnForegroundApplicationChanged(app.get());

  std::unique_ptr<TRuntimeApplicationPlatform> platform_app =
      std::move(factory).Run(std::move(app));
  auto [iter, success] =
      loaded_apps_.emplace(std::move(session_id), std::move(platform_app));
  DCHECK(success);
  return iter->second.get();
}

template <typename TRuntimeApplicationPlatform>
TRuntimeApplicationPlatform*
RuntimeApplicationDispatcherImpl<TRuntimeApplicationPlatform>::GetApplication(
    const std::string& session_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto iter = loaded_apps_.find(session_id);
  return iter == loaded_apps_.end() ? nullptr : iter->second.get();
}

template <typename TRuntimeApplicationPlatform>
std::unique_ptr<TRuntimeApplicationPlatform> RuntimeApplicationDispatcherImpl<
    TRuntimeApplicationPlatform>::DestroyApplication(const std::string&
                                                         session_id) {
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

}  // namespace chromecast

#endif  // CHROMECAST_CAST_CORE_RUNTIME_BROWSER_RUNTIME_APPLICATION_DISPATCHER_IMPL_H_
