// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CAST_RECEIVER_BROWSER_RUNTIME_APPLICATION_DISPATCHER_IMPL_H_
#define COMPONENTS_CAST_RECEIVER_BROWSER_RUNTIME_APPLICATION_DISPATCHER_IMPL_H_

#include <memory>

#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/sequence_checker.h"
#include "components/cast_receiver/browser/application_client.h"
#include "components/cast_receiver/browser/public/application_config.h"
#include "components/cast_receiver/browser/public/runtime_application_dispatcher.h"
#include "components/cast_receiver/browser/runtime_application_base.h"
#include "components/cast_receiver/browser/streaming_runtime_application.h"
#include "components/cast_receiver/browser/web_runtime_application.h"
#include "components/cast_streaming/common/public/app_ids.h"

namespace cast_receiver {

template <typename TEmbedderApplication>
class RuntimeApplicationDispatcherImpl
    : public RuntimeApplicationDispatcher<TEmbedderApplication> {
 public:
  // |application_client| is expected to persist for the lifetime of this
  // instance.
  explicit RuntimeApplicationDispatcherImpl(
      ApplicationClient& application_client);
  ~RuntimeApplicationDispatcherImpl() override = default;

  RuntimeApplicationDispatcherImpl(RuntimeApplicationDispatcherImpl& other) =
      delete;
  RuntimeApplicationDispatcherImpl& operator=(
      RuntimeApplicationDispatcherImpl& other) = delete;

 private:
  using EmbedderApplicationFactory = typename RuntimeApplicationDispatcher<
      TEmbedderApplication>::EmbedderApplicationFactory;

  // RuntimeApplicationDispatcher implementation.
  TEmbedderApplication* CreateApplication(
      std::string session_id,
      ApplicationConfig app_config,
      EmbedderApplicationFactory factory) override;
  TEmbedderApplication* GetApplication(const std::string& session_id) override;
  std::unique_ptr<TEmbedderApplication> DestroyApplication(
      const std::string& session_id) override;

  SEQUENCE_CHECKER(sequence_checker_);
  raw_ref<ApplicationClient> const application_client_;

  base::flat_map<std::string, std::unique_ptr<TEmbedderApplication>>
      loaded_apps_;
};

template <typename TEmbedderApplication>
RuntimeApplicationDispatcherImpl<TEmbedderApplication>::
    RuntimeApplicationDispatcherImpl(ApplicationClient& application_client)
    : application_client_(application_client) {}

template <typename TEmbedderApplication>
TEmbedderApplication*
RuntimeApplicationDispatcherImpl<TEmbedderApplication>::CreateApplication(
    std::string session_id,
    ApplicationConfig app_config,
    EmbedderApplicationFactory factory) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::unique_ptr<RuntimeApplicationBase> app;
  if (cast_streaming::IsStreamingReceiverAppId(app_config.app_id)) {
    app = std::make_unique<StreamingRuntimeApplication>(
        session_id, std::move(app_config), *application_client_);
  } else {
    app = std::make_unique<WebRuntimeApplication>(
        session_id, std::move(app_config), *application_client_);
  }

  // TODO(b/232140331): Call this only when foreground app changes.
  application_client_->OnForegroundApplicationChanged(app.get());

  auto* app_ptr = app.get();
  std::unique_ptr<TEmbedderApplication> embedder_app =
      std::move(factory).Run(std::move(app));
  app_ptr->SetEmbedderApplication(*embedder_app);

  auto [iter, success] =
      loaded_apps_.emplace(std::move(session_id), std::move(embedder_app));
  DCHECK(success);
  return iter->second.get();
}

template <typename TEmbedderApplication>
TEmbedderApplication*
RuntimeApplicationDispatcherImpl<TEmbedderApplication>::GetApplication(
    const std::string& session_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto iter = loaded_apps_.find(session_id);
  return iter == loaded_apps_.end() ? nullptr : iter->second.get();
}

template <typename TEmbedderApplication>
std::unique_ptr<TEmbedderApplication>
RuntimeApplicationDispatcherImpl<TEmbedderApplication>::DestroyApplication(
    const std::string& session_id) {
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

}  // namespace cast_receiver

#endif  // COMPONENTS_CAST_RECEIVER_BROWSER_RUNTIME_APPLICATION_DISPATCHER_IMPL_H_
