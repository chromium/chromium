// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/assistant/proxy/assistant_proxy.h"
#include <memory>

#include "base/bind.h"
#include "base/check.h"
#include "chromeos/services/assistant/proxy/service_controller.h"
#include "chromeos/services/libassistant/libassistant_service.h"

namespace chromeos {
namespace assistant {

AssistantProxy::AssistantProxy() {
  background_thread_.Start();

  CreateMojomService();

  service_controller_ = std::make_unique<ServiceController>(
      background_task_runner(), BindServiceController());
}

AssistantProxy::~AssistantProxy() {
  DestroyMojomService();

  // We must wait here for the background thread to finish.
  // If we don't wait here, we run into the following timing issue:
  //  1. (main thread): Post creation of |mojom_service_| to background thread.
  //  2. (main thread): In destructor, post destruction of |mojom_service_| to
  //                    background thread.
  //  3. (background thread): Create |mojom_service_|
  //  4. (main thread): Continue the destructor, notice |mojom_service_| is
  //                    non-null and destroy it.
  //  5. Die because we destructed |mojom_service_| on the wrong thread.
  // By explicitly waiting for the background thread here we ensure both the
  // creation and destruction are done before we go to step #4.
  background_thread_.Stop();
}

void AssistantProxy::CreateMojomService() {
  // A Mojom service runs on the thread where its receiver was bound.
  // So to make |mojom_service_| run on the background thread, we must create
  // it on the background thread, as it binds its receiver in its constructor.
  background_task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&AssistantProxy::CreateMojomServiceOnBackgroundThread,
                     // This is safe because we own the mojom thread,
                     // so when we're deleted the mojom thread is stopped.
                     base::Unretained(this),
                     // |client_| runs on the current thread, so must be bound
                     // here and not on the background thread.
                     client_.BindNewPipeAndPassReceiver()));
}

void AssistantProxy::CreateMojomServiceOnBackgroundThread(
    mojo::PendingReceiver<LibassistantServiceMojom> client) {
  DCHECK(background_task_runner()->BelongsToCurrentThread());
  mojom_service_ =
      std::make_unique<chromeos::libassistant::LibassistantService>(
          std::move(client));
}

void AssistantProxy::DestroyMojomService() {
  // |mojom_service_| is created on the background thread, so we have to delete
  // it there as well.
  // Note that it would be tempting to use
  //    background_task_runner()
  //      ->DeleteSoon(FROM_HERE, std::move(mojom_service_));
  // but that doesn't work as it is possible |mojom_service_| is nullptr at
  // this time, but will be populated by the background thread before it is
  // stopped.
  background_task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&AssistantProxy::DestroyMojomServiceOnBackgroundThread,
                     base::Unretained(this)));
}

void AssistantProxy::DestroyMojomServiceOnBackgroundThread() {
  DCHECK(background_task_runner()->BelongsToCurrentThread());
  mojom_service_ = nullptr;
}

mojo::Remote<AssistantProxy::ServiceControllerMojom>
AssistantProxy::BindServiceController() {
  mojo::Remote<ServiceControllerMojom> remote;
  client_->BindServiceController(remote.BindNewPipeAndPassReceiver());
  return remote;
}

scoped_refptr<base::SingleThreadTaskRunner>
AssistantProxy::background_task_runner() {
  return background_thread_.task_runner();
}

ServiceController& AssistantProxy::service_controller() {
  DCHECK(service_controller_);
  return *service_controller_;
}

}  // namespace assistant
}  // namespace chromeos
