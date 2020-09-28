// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/cdm_factory_daemon/cdm_factory_daemon_proxy.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chromeos/components/cdm_factory_daemon/output_protection_impl.h"
#include "chromeos/dbus/cdm_factory_daemon/cdm_factory_daemon_client.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "mojo/public/cpp/system/invitation.h"

namespace chromeos {
namespace {
constexpr char kCdmFactoryDaemonPipeName[] = "cdm-factory-daemon-pipe";
}  // namespace

CdmFactoryDaemonProxy::CdmFactoryDaemonProxy()
    : receiver_(this),
      mojo_task_runner_(base::SequencedTaskRunnerHandle::Get()) {}

CdmFactoryDaemonProxy::~CdmFactoryDaemonProxy() = default;

void CdmFactoryDaemonProxy::Create(
    mojo::PendingReceiver<CdmFactoryDaemon> receiver) {
  // We do not want to use a SelfOwnedReceiver here because if the GPU process
  // goes down, we don't want to destruct and drop our connection to the daemon.
  // It's not possible to reconnect to the daemon from the browser process w/out
  // restarting both processes (which happens if the browser goes down).
  GetInstance().BindReceiver(std::move(receiver));
}

void CdmFactoryDaemonProxy::CreateFactory(const std::string& key_system,
                                          CreateFactoryCallback callback) {
  DCHECK(mojo_task_runner_->RunsTasksInCurrentSequence());
  DVLOG(1) << "CdmFactoryDaemonProxy::CreateFactory called";
  if (daemon_remote_.is_bound()) {
    DVLOG(1) << "CdmFactoryDaemon mojo connection already exists, re-use it";
    GetFactoryInterface(key_system, std::move(callback));
    return;
  }

  EstablishDaemonConnection(
      base::BindOnce(&CdmFactoryDaemonProxy::GetFactoryInterface,
                     base::Unretained(this), key_system, std::move(callback)));
}

void CdmFactoryDaemonProxy::ConnectOemCryptoDeprecated(
    mojo::PendingReceiver<arc::mojom::OemCryptoService> oemcryptor,
    mojo::PendingRemote<arc::mojom::ProtectedBufferManager>
        protected_buffer_manager) {
  // This will never get called because this originates from Chrome as well
  // which will always be on the same version as us.
  NOTIMPLEMENTED();
}

void CdmFactoryDaemonProxy::ConnectOemCrypto(
    mojo::PendingReceiver<arc::mojom::OemCryptoService> oemcryptor,
    mojo::PendingRemote<arc::mojom::ProtectedBufferManager>
        protected_buffer_manager,
    mojo::PendingRemote<cdm::mojom::OutputProtection> output_protection) {
  // This gets invoked from ArcBridge which uses a different thread.
  if (!mojo_task_runner_->RunsTasksInCurrentSequence()) {
    mojo_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&CdmFactoryDaemonProxy::ConnectOemCrypto,
                                  base::Unretained(this), std::move(oemcryptor),
                                  std::move(protected_buffer_manager),
                                  std::move(output_protection)));
    return;
  }

  DVLOG(1) << "CdmFactoryDaemonProxy::ConnectOemCrypto called";
  if (daemon_remote_.is_bound()) {
    DVLOG(1) << "CdmFactoryDaemon mojo connection already exists, re-use it";
    CompleteOemCryptoConnection(std::move(oemcryptor),
                                std::move(protected_buffer_manager),
                                std::move(output_protection));
    return;
  }

  EstablishDaemonConnection(base::BindOnce(
      &CdmFactoryDaemonProxy::CompleteOemCryptoConnection,
      base::Unretained(this), std::move(oemcryptor),
      std::move(protected_buffer_manager), std::move(output_protection)));
}

void CdmFactoryDaemonProxy::GetOutputProtection(
    mojo::PendingReceiver<cdm::mojom::OutputProtection> output_protection) {
  OutputProtectionImpl::Create(std::move(output_protection));
}

void CdmFactoryDaemonProxy::SendDBusRequest(base::ScopedFD fd,
                                            base::OnceClosure callback) {
  chromeos::CdmFactoryDaemonClient::Get()->BootstrapMojoConnection(
      std::move(fd),
      base::BindOnce(&CdmFactoryDaemonProxy::OnBootstrapMojoConnection,
                     base::Unretained(this), std::move(callback)));
}

void CdmFactoryDaemonProxy::EstablishDaemonConnection(
    base::OnceClosure callback) {
  // This may have happened already.
  if (daemon_remote_.is_bound()) {
    std::move(callback).Run();
    return;
  }
  // Bootstrap the Mojo connection to the daemon.
  mojo::OutgoingInvitation invitation;
  mojo::PlatformChannel channel;
  mojo::ScopedMessagePipeHandle server_pipe =
      invitation.AttachMessagePipe(kCdmFactoryDaemonPipeName);
  mojo::OutgoingInvitation::Send(std::move(invitation),
                                 base::kNullProcessHandle,
                                 channel.TakeLocalEndpoint());
  base::ScopedFD fd =
      channel.TakeRemoteEndpoint().TakePlatformHandle().TakeFD();

  // Bind the Mojo pipe to the interface before we send the D-Bus message
  // to avoid any kind of race condition with detecting it's been bound.
  // It's safe to do this before the other end binds anyways.
  daemon_remote_.Bind(mojo::PendingRemote<cdm::mojom::CdmFactoryDaemon>(
      std::move(server_pipe), 0u));
  daemon_remote_.set_disconnect_handler(
      base::BindOnce(&CdmFactoryDaemonProxy::OnDaemonMojoConnectionError,
                     base::Unretained(this)));

  // We need to invoke this call on the D-Bus (UI) thread.
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&CdmFactoryDaemonProxy::SendDBusRequest,
                                base::Unretained(this), std::move(fd),
                                std::move(callback)));
}

void CdmFactoryDaemonProxy::OnBootstrapMojoConnection(
    base::OnceClosure callback,
    bool result) {
  if (!mojo_task_runner_->RunsTasksInCurrentSequence()) {
    mojo_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&CdmFactoryDaemonProxy::OnBootstrapMojoConnection,
                       base::Unretained(this), std::move(callback), result));
    return;
  }
  if (!result) {
    LOG(ERROR) << "CdmFactoryDaemon had a failure in D-Bus with the daemon";
    daemon_remote_.reset();
  } else {
    DVLOG(1) << "Succeeded with CdmFactoryDaemon bootstrapping";
  }
  std::move(callback).Run();
}

void CdmFactoryDaemonProxy::GetFactoryInterface(
    const std::string& key_system,
    CreateFactoryCallback callback) {
  if (!daemon_remote_ || !daemon_remote_.is_bound()) {
    LOG(ERROR) << "daemon_remote_ interface is not connected";
    std::move(callback).Run(mojo::PendingRemote<cdm::mojom::CdmFactory>());
    return;
  }
  daemon_remote_->CreateFactory(key_system, std::move(callback));
}

void CdmFactoryDaemonProxy::CompleteOemCryptoConnection(
    mojo::PendingReceiver<arc::mojom::OemCryptoService> oemcryptor,
    mojo::PendingRemote<arc::mojom::ProtectedBufferManager>
        protected_buffer_manager,
    mojo::PendingRemote<cdm::mojom::OutputProtection> output_protection) {
  if (!daemon_remote_ || !daemon_remote_.is_bound()) {
    LOG(ERROR) << "daemon_remote_ interface is not connected";
    // Just let the mojo objects go out of scope and be destructed to signal
    // failure.
    return;
  }
  daemon_remote_->ConnectOemCrypto(std::move(oemcryptor),
                                   std::move(protected_buffer_manager),
                                   std::move(output_protection));
}

// static
CdmFactoryDaemonProxy& CdmFactoryDaemonProxy::GetInstance() {
  static base::NoDestructor<CdmFactoryDaemonProxy> instance;
  return *instance;
}

void CdmFactoryDaemonProxy::OnGpuMojoConnectionError() {
  DVLOG(1) << "CdmFactoryDaemon GPU Mojo connection lost.";
  receiver_.reset();
}

void CdmFactoryDaemonProxy::OnDaemonMojoConnectionError() {
  DVLOG(1) << "CdmFactoryDaemon daemon Mojo connection lost.";
  daemon_remote_.reset();
}

void CdmFactoryDaemonProxy::BindReceiver(
    mojo::PendingReceiver<CdmFactoryDaemon> receiver) {
  receiver_.reset();
  receiver_.Bind(std::move(receiver));
  receiver_.set_disconnect_handler(
      base::BindOnce(&CdmFactoryDaemonProxy::OnGpuMojoConnectionError,
                     base::Unretained(this)));
}

}  // namespace chromeos
