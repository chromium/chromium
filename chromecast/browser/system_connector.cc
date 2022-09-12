// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/system_connector.h"

#include "base/check_op.h"
#include "base/threading/sequence_local_storage_slot.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace chromecast {

namespace {

base::SequenceLocalStorageSlot<service_manager::Connector>&
GetConnectorStorage() {
  static base::SequenceLocalStorageSlot<service_manager::Connector> storage;
  return storage;
}

void BindReceiverOnMainThread(
    mojo::PendingReceiver<service_manager::mojom::Connector> receiver) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  auto* main_thread_connector = GetSystemConnector();
  DCHECK(main_thread_connector)
      << "GetSystemConnector() called on background thread with no system "
      << "Connector set on the main thread.";
  main_thread_connector->BindConnectorReceiver(std::move(receiver));
}

}  // namespace

service_manager::Connector* GetSystemConnector() {
  auto& storage = GetConnectorStorage();
  if (!content::BrowserThread::IsThreadInitialized(
          content::BrowserThread::UI) ||
      content::BrowserThread::CurrentlyOn(content::BrowserThread::UI)) {
    return storage.GetValuePointer();
  }

  if (!storage) {
    mojo::PendingRemote<service_manager::mojom::Connector> remote;
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(&BindReceiverOnMainThread,
                                  remote.InitWithNewPipeAndPassReceiver()));
    storage.emplace(std::move(remote));
  }

  return storage.GetValuePointer();
}

void SetSystemConnector(std::unique_ptr<service_manager::Connector> connector) {
  if (!connector) {
    GetConnectorStorage().reset();
    return;
  }
  mojo::PendingRemote<service_manager::mojom::Connector> remote;
  connector->BindConnectorReceiver(remote.InitWithNewPipeAndPassReceiver());
  GetConnectorStorage().emplace(std::move(remote));
}

}  // namespace chromecast
