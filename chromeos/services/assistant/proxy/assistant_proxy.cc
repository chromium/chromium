// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/assistant/proxy/assistant_proxy.h"

#include <memory>

#include "base/bind.h"
#include "base/check.h"
#include "chromeos/services/assistant/proxy/libassistant_service_host.h"
#include "chromeos/services/assistant/proxy/service_controller_proxy.h"
#include "chromeos/services/libassistant/libassistant_service.h"

namespace chromeos {
namespace assistant {

AssistantProxy::AssistantProxy() {
  background_thread_.Start();
}

AssistantProxy::~AssistantProxy() {
  StopLibassistantService();
}

void AssistantProxy::Initialize(LibassistantServiceHost* host) {
  DCHECK(host);
  libassistant_service_host_ = host;
  LaunchLibassistantService();

  service_controller_proxy_ =
      std::make_unique<ServiceControllerProxy>(host, BindServiceController());
}

void AssistantProxy::LaunchLibassistantService() {
  // A Mojom service runs on the thread where its receiver was bound.
  // So to make |libassistant_service_| run on the background thread, we must
  // create it on the background thread, as it binds its receiver in its
  // constructor.
  background_task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &AssistantProxy::LaunchLibassistantServiceOnBackgroundThread,
          // This is safe because we own the background thread,
          // so when we're deleted the background thread is stopped.
          base::Unretained(this),
          // |libassistant_service_remote_| runs on the current thread, so must
          // be bound here and not on the background thread.
          libassistant_service_remote_.BindNewPipeAndPassReceiver()));
}

void AssistantProxy::LaunchLibassistantServiceOnBackgroundThread(
    mojo::PendingReceiver<LibassistantServiceMojom> client) {
  DCHECK(background_task_runner()->BelongsToCurrentThread());
  DCHECK(libassistant_service_host_);
  libassistant_service_host_->Launch(std::move(client));
}

void AssistantProxy::StopLibassistantService() {
  // |libassistant_service_| is launched on the background thread, so we have to
  // stop it there as well.
  background_task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&AssistantProxy::StopLibassistantServiceOnBackgroundThread,
                     base::Unretained(this)));
}

void AssistantProxy::StopLibassistantServiceOnBackgroundThread() {
  DCHECK(background_task_runner()->BelongsToCurrentThread());
  libassistant_service_host_->Stop();
}

mojo::PendingRemote<AssistantProxy::ServiceControllerMojom>
AssistantProxy::BindServiceController() {
  mojo::PendingRemote<ServiceControllerMojom> pending_remote;
  libassistant_service_remote_->BindServiceController(
      pending_remote.InitWithNewPipeAndPassReceiver());
  return pending_remote;
}

scoped_refptr<base::SingleThreadTaskRunner>
AssistantProxy::background_task_runner() {
  return background_thread_.task_runner();
}

ServiceControllerProxy& AssistantProxy::service_controller() {
  DCHECK(service_controller_proxy_);
  return *service_controller_proxy_;
}

}  // namespace assistant
}  // namespace chromeos
