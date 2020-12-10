// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/assistant/proxy/assistant_proxy.h"

#include <memory>

#include "base/bind.h"
#include "base/check.h"
#include "chromeos/services/assistant/proxy/service_controller_proxy.h"
#include "chromeos/services/libassistant/libassistant_service.h"

namespace chromeos {
namespace assistant {

AssistantProxy::AssistantProxy() {
  background_thread_.Start();
}

AssistantProxy::~AssistantProxy() {
  DestroyLibassistantService();

  // We must wait here for the background thread to finish.
  // If we don't wait here, we run into the following timing issue:
  //  1. (main thread): Post creation of |libassistant_service_| to background
  //                    thread.
  //  2. (main thread): In destructor, post destruction of
  //                    |libassistant_service_| to background thread.
  //  3. (background thread): Create |libassistant_service_|
  //  4. (main thread): Continue the destructor, notice |libassistant_service_|
  //                    is non-null and destroy it.
  //  5. Die because we destructed |libassistant_service_| on the wrong thread.
  // By explicitly waiting for the background thread here we ensure both the
  // creation and destruction are done before we go to step #4.
  background_thread_.Stop();
}

void AssistantProxy::Initialize(assistant_client::PlatformApi* platform_api,
                                AssistantManagerServiceDelegate* delegate) {
  CreateLibassistantService(platform_api, delegate);

  service_controller_proxy_ = std::make_unique<ServiceControllerProxy>(
      background_task_runner(), BindServiceController());
}

void AssistantProxy::CreateLibassistantService(
    assistant_client::PlatformApi* platform_api,
    AssistantManagerServiceDelegate* delegate) {
  // A Mojom service runs on the thread where its receiver was bound.
  // So to make |libassistant_service_| run on the background thread, we must
  // create it on the background thread, as it binds its receiver in its
  // constructor.
  background_task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &AssistantProxy::CreateLibassistantServiceOnBackgroundThread,
          // This is safe because we own the background thread,
          // so when we're deleted the background thread is stopped.
          base::Unretained(this),
          // |libassistant_service_remote_| runs on the current thread, so must
          // be bound here and not on the background thread.
          libassistant_service_remote_.BindNewPipeAndPassReceiver(),
          platform_api, delegate));
}

void AssistantProxy::CreateLibassistantServiceOnBackgroundThread(
    mojo::PendingReceiver<LibassistantServiceMojom> client,
    assistant_client::PlatformApi* platform_api,
    AssistantManagerServiceDelegate* delegate) {
  DCHECK(background_task_runner()->BelongsToCurrentThread());
  libassistant_service_ =
      std::make_unique<chromeos::libassistant::LibassistantService>(
          std::move(client), platform_api, delegate);
}

void AssistantProxy::DestroyLibassistantService() {
  // |libassistant_service_| is created on the background thread, so we have to
  // delete it there as well. Note that it would be tempting to use
  //    background_task_runner()
  //      ->DeleteSoon(FROM_HERE, std::move(libassistant_service_));
  // but that doesn't work as it is possible |libassistant_service_| is nullptr
  // at this time, but will be populated by the background thread before it is
  // stopped.
  background_task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &AssistantProxy::DestroyLibassistantServiceOnBackgroundThread,
          base::Unretained(this)));
}

void AssistantProxy::DestroyLibassistantServiceOnBackgroundThread() {
  DCHECK(background_task_runner()->BelongsToCurrentThread());
  libassistant_service_ = nullptr;
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
