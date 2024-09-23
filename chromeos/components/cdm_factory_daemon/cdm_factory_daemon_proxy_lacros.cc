// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/cdm_factory_daemon/cdm_factory_daemon_proxy_lacros.h"

#include "base/logging.h"
#include "base/no_destructor.h"
#include "chromeos/lacros/lacros_service.h"
#include "mojo/public/cpp/bindings/generic_pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace chromeos {

CdmFactoryDaemonProxyLacros::CdmFactoryDaemonProxyLacros()
    : CdmFactoryDaemonProxy() {}

CdmFactoryDaemonProxyLacros::~CdmFactoryDaemonProxyLacros() = default;

void CdmFactoryDaemonProxyLacros::Create(
    mojo::PendingReceiver<BrowserCdmFactory> receiver) {
  // We do not want to use a SelfOwnedReceiver for the main implementation here
  // because if the GPU process goes down, there's no reason to drop our
  // connection to ash-chrome that we may have established already. However, the
  // connection between lacros-GPU and lacros-browser uses a ReceiverSet, which
  // is self-destructing on disconnect.
  GetInstance().BindReceiver(std::move(receiver));
}

void CdmFactoryDaemonProxyLacros::CreateFactory(
    const std::string& key_system,
    CreateFactoryCallback callback) {
  DCHECK(mojo_task_runner_->RunsTasksInCurrentSequence());
  DVLOG(1) << "CdmFactoryDaemonProxyLacros::CreateFactory called";
  if (ash_remote_) {
    DVLOG(1) << "BrowserCdmFactory mojo connection already exists, re-use it";
    GetFactoryInterface(key_system, std::move(callback));
    return;
  }

  EstablishAshConnection(
      base::BindOnce(&CdmFactoryDaemonProxyLacros::GetFactoryInterface,
                     base::Unretained(this), key_system, std::move(callback)));
}

void CdmFactoryDaemonProxyLacros::GetHwConfigData(
    GetHwConfigDataCallback callback) {
  DCHECK(mojo_task_runner_->RunsTasksInCurrentSequence());
  DVLOG(1) << "CdmFactoryDaemonProxyLacros::GetHwConfigData called";
  if (ash_remote_) {
    DVLOG(1) << "BrowserCdmFactory mojo connection already exists, re-use it";
    ProxyGetHwConfigData(std::move(callback));
    return;
  }

  EstablishAshConnection(
      base::BindOnce(&CdmFactoryDaemonProxyLacros::ProxyGetHwConfigData,
                     base::Unretained(this), std::move(callback)));
}

void CdmFactoryDaemonProxyLacros::GetOutputProtection(
    mojo::PendingReceiver<cdm::mojom::OutputProtection> output_protection) {
  if (ash_remote_) {
    // This should always be bound unless it became disconnected in the middle
    // of setting things up.
    ash_remote_->GetOutputProtection(std::move(output_protection));
  }
}

void CdmFactoryDaemonProxyLacros::GetScreenResolutions(
    GetScreenResolutionsCallback callback) {
  if (ash_remote_) {
    // This should always be bound unless it became disconnected in the middle
    // of setting things up.
    ash_remote_->GetScreenResolutions(std::move(callback));
  } else {
    std::move(callback).Run(std::vector<gfx::Size>());
  }
}

void CdmFactoryDaemonProxyLacros::GetAndroidHwKeyData(
    const std::vector<uint8_t>& key_id,
    const std::vector<uint8_t>& hw_identifier,
    GetAndroidHwKeyDataCallback callback) {
  // This should only go through ash-chrome.
  NOTREACHED_IN_MIGRATION();
}

void CdmFactoryDaemonProxyLacros::AllocateSecureBuffer(
    uint32_t size,
    AllocateSecureBufferCallback callback) {
  if (ash_remote_) {
    // This should always be bound unless it became disconnected in the middle
    // of setting things up.
    ash_remote_->AllocateSecureBuffer(size, std::move(callback));
  } else {
    std::move(callback).Run(mojo::PlatformHandle());
  }
}

void CdmFactoryDaemonProxyLacros::ParseEncryptedSliceHeader(
    uint64_t secure_handle,
    uint32_t offset,
    const std::vector<uint8_t>& stream_data,
    ParseEncryptedSliceHeaderCallback callback) {
  if (ash_remote_) {
    // This should always be bound unless it became disconnected in the middle
    // of setting things up.
    ash_remote_->ParseEncryptedSliceHeader(secure_handle, offset, stream_data,
                                           std::move(callback));
  } else {
    std::move(callback).Run(false, {});
  }
}

void CdmFactoryDaemonProxyLacros::EstablishAshConnection(
    base::OnceClosure callback) {
  // This may have happened already.
  if (ash_remote_) {
    std::move(callback).Run();
    return;
  }

  auto* service = LacrosService::Get();
  if (!service || !service->IsSupported<cdm::mojom::BrowserCdmFactory>()) {
    std::move(callback).Run();
    return;
  }
  // For Lacros, we connect to the ash-chrome browser process which will proxy
  // the connection to the daemon.
  service->BindBrowserCdmFactory(
      mojo::GenericPendingReceiver(ash_remote_.BindNewPipeAndPassReceiver()));
  std::move(callback).Run();
  return;
}

void CdmFactoryDaemonProxyLacros::GetFactoryInterface(
    const std::string& key_system,
    CreateFactoryCallback callback) {
  if (!ash_remote_) {
    LOG(ERROR) << "ash_remote_ interface is not connected";
    std::move(callback).Run(mojo::PendingRemote<cdm::mojom::CdmFactory>());
    return;
  }
  ash_remote_->CreateFactory(key_system, std::move(callback));
}

void CdmFactoryDaemonProxyLacros::ProxyGetHwConfigData(
    GetHwConfigDataCallback callback) {
  if (!ash_remote_) {
    LOG(ERROR) << "ash_remote_ interface is not connected";
    std::move(callback).Run(false, std::vector<uint8_t>());
    return;
  }
  ash_remote_->GetHwConfigData(std::move(callback));
}

// static
CdmFactoryDaemonProxyLacros& CdmFactoryDaemonProxyLacros::GetInstance() {
  static base::NoDestructor<CdmFactoryDaemonProxyLacros> instance;
  return *instance;
}

}  // namespace chromeos
