// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_CDM_FACTORY_DAEMON_CDM_FACTORY_DAEMON_PROXY_H_
#define CHROMEOS_COMPONENTS_CDM_FACTORY_DAEMON_CDM_FACTORY_DAEMON_PROXY_H_

#include "base/component_export.h"
#include "chromeos/components/cdm_factory_daemon/mojom/cdm_factory_daemon.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace chromeos {

// The class serves two purposes.
// 1. Allow the GPU process to call into the browser process to get the Mojo CDM
//    Factory interface on the daemon.
// 2. Allow ArcBridge to get the OemCrypto Mojo interface from that same daemon.
//
// Since both of these mojo connections are to the same daemon, they need to be
// bootstrapped together on one higher level interface. This is done by using
// D-Bus to first bootstrap our connection to the daemon. Thenover that
// interface we request the CdmFactory and pass it back via Mojo to
// the GPU process, or we request the OemCrypto interface and pass that back to
// ArcBridge. We implement the same Mojo interface here as on the daemon since
// we are essentially just a proxy.
class COMPONENT_EXPORT(CDM_FACTORY_DAEMON) CdmFactoryDaemonProxy
    : public cdm::mojom::CdmFactoryDaemon {
 public:
  CdmFactoryDaemonProxy();

  CdmFactoryDaemonProxy(const CdmFactoryDaemonProxy&) = delete;
  CdmFactoryDaemonProxy& operator=(const CdmFactoryDaemonProxy&) = delete;

  ~CdmFactoryDaemonProxy() override;

  static void Create(mojo::PendingReceiver<CdmFactoryDaemon> receiver);

  static CdmFactoryDaemonProxy& GetInstance();

  // chromeos::cdm::mojom::CdmFactoryDaemon:
  void CreateFactory(const std::string& key_system,
                     CreateFactoryCallback callback) override;
  void ConnectOemCryptoDeprecated(
      mojo::PendingReceiver<arc::mojom::OemCryptoService> oemcryptor,
      mojo::PendingRemote<arc::mojom::ProtectedBufferManager>
          protected_buffer_manager) override;
  void ConnectOemCrypto(
      mojo::PendingReceiver<arc::mojom::OemCryptoService> oemcryptor,
      mojo::PendingRemote<arc::mojom::ProtectedBufferManager>
          protected_buffer_manager,
      mojo::PendingRemote<cdm::mojom::OutputProtection> output_protection)
      override;
  void GetOutputProtection(mojo::PendingReceiver<cdm::mojom::OutputProtection>
                               output_protection) override;

 private:
  void SendDBusRequest(base::ScopedFD fd, base::OnceClosure callback);
  void EstablishDaemonConnection(base::OnceClosure callback);
  void OnBootstrapMojoConnection(base::OnceClosure callback, bool result);
  void GetFactoryInterface(const std::string& key_system,
                           CreateFactoryCallback callback);
  void CompleteOemCryptoConnection(
      mojo::PendingReceiver<arc::mojom::OemCryptoService> oemcryptor,
      mojo::PendingRemote<arc::mojom::ProtectedBufferManager>
          protected_buffer_manager,
      mojo::PendingRemote<cdm::mojom::OutputProtection> output_protection);
  void OnGpuMojoConnectionError();
  void OnDaemonMojoConnectionError();
  void BindReceiver(mojo::PendingReceiver<CdmFactoryDaemon> receiver);

  mojo::Remote<CdmFactoryDaemon> daemon_remote_;
  mojo::Receiver<CdmFactoryDaemon> receiver_;

  scoped_refptr<base::SequencedTaskRunner> mojo_task_runner_;
};

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_CDM_FACTORY_DAEMON_CDM_FACTORY_DAEMON_PROXY_H_
