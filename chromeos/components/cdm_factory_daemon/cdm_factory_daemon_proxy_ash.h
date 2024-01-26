// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_CDM_FACTORY_DAEMON_CDM_FACTORY_DAEMON_PROXY_ASH_H_
#define CHROMEOS_COMPONENTS_CDM_FACTORY_DAEMON_CDM_FACTORY_DAEMON_PROXY_ASH_H_

#include "chromeos/components/cdm_factory_daemon/cdm_factory_daemon_proxy.h"

#include "base/component_export.h"
#include "chromeos/components/cdm_factory_daemon/mojom/cdm_factory_daemon.mojom.h"
#include "chromeos/components/cdm_factory_daemon/output_protection_impl.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace chromeos {

// This class serves two purposes.
// 1. Allow the GPU process to call into the browser process to get the Mojo CDM
//    Factory interface on the daemon.
// 2. Allow ArcBridge to get the OemCrypto Mojo interface from that same daemon.
//
// Since both of these mojo connections are to the same daemon, they need to be
// bootstrapped together on one higher level interface. This is done by using
// D-Bus to first bootstrap our connection to the daemon. Then over that
// interface we request the CdmFactory and pass it back via Mojo to
// the GPU process, or we request the OemCrypto interface and pass that back to
// ArcBridge.
class COMPONENT_EXPORT(CDM_FACTORY_DAEMON) CdmFactoryDaemonProxyAsh
    : public CdmFactoryDaemonProxy {
 public:
  CdmFactoryDaemonProxyAsh();

  CdmFactoryDaemonProxyAsh(const CdmFactoryDaemonProxyAsh&) = delete;
  CdmFactoryDaemonProxyAsh& operator=(const CdmFactoryDaemonProxyAsh&) = delete;

  ~CdmFactoryDaemonProxyAsh() override;

  static void Create(mojo::PendingReceiver<BrowserCdmFactory> receiver);

  static CdmFactoryDaemonProxyAsh& GetInstance();

  // Used to create a backing implementation of the
  // cdm::mojom::BrowserCdmFactory that will proxy all calls to the browser
  // process. This is used to create a mojo::PendingRemote that can be passed
  // to an OOP video decoder.
  static std::unique_ptr<cdm::mojom::BrowserCdmFactory>
  CreateBrowserCdmFactoryProxy();

  void ConnectOemCrypto(
      mojo::PendingReceiver<arc::mojom::OemCryptoService> oemcryptor,
      mojo::PendingRemote<arc::mojom::ProtectedBufferManager>
          protected_buffer_manager,
      mojo::PendingRemote<cdm::mojom::OutputProtection> output_protection);
  void GetHdcp14Key(
      cdm::mojom::CdmFactoryDaemon::GetHdcp14KeyCallback callback);

  // chromeos::cdm::mojom::BrowserCdmFactoryDaemon:
  void CreateFactory(const std::string& key_system,
                     CreateFactoryCallback callback) override;
  void GetHwConfigData(GetHwConfigDataCallback callback) override;
  void GetOutputProtection(mojo::PendingReceiver<cdm::mojom::OutputProtection>
                               output_protection) override;
  void GetScreenResolutions(GetScreenResolutionsCallback callback) override;
  void GetAndroidHwKeyData(const std::vector<uint8_t>& key_id,
                           const std::vector<uint8_t>& hw_identifier,
                           GetAndroidHwKeyDataCallback callback) override;
  void AllocateSecureBuffer(uint32_t size,
                            AllocateSecureBufferCallback callback) override;
  void ParseEncryptedSliceHeader(
      uint64_t secure_handle,
      uint32_t offset,
      const std::vector<uint8_t>& stream_data,
      ParseEncryptedSliceHeaderCallback callback) override;

 private:
  void EstablishDaemonConnection(base::OnceClosure callback);
  void GetFactoryInterface(const std::string& key_system,
                           CreateFactoryCallback callback);
  void ProxyGetHwConfigData(GetHwConfigDataCallback callback);
  void ProxyGetAndroidHwKeyData(const std::vector<uint8_t>& key_id,
                                const std::vector<uint8_t>& hw_identifier,
                                GetAndroidHwKeyDataCallback callback);
  void ProxyAllocateSecureBuffer(uint32_t size,
                                 AllocateSecureBufferCallback callback);
  void ProxyParseEncryptedSliceHeader(
      uint64_t secure_handle,
      uint32_t offset,
      const std::vector<uint8_t>& stream_data,
      ParseEncryptedSliceHeaderCallback callback);
  void SendDBusRequest(base::ScopedFD fd, base::OnceClosure callback);
  void OnBootstrapMojoConnection(base::OnceClosure callback, bool result);
  void CompleteOemCryptoConnection(
      mojo::PendingReceiver<arc::mojom::OemCryptoService> oemcryptor,
      mojo::PendingRemote<arc::mojom::ProtectedBufferManager>
          protected_buffer_manager,
      mojo::PendingRemote<cdm::mojom::OutputProtection> output_protection);
  void OnDaemonMojoConnectionError();

  mojo::Remote<cdm::mojom::CdmFactoryDaemon> daemon_remote_;

  // This is used when the chrome flag is set to always enable HDCP.
  std::unique_ptr<OutputProtectionImpl> forced_output_protection_;
};

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_CDM_FACTORY_DAEMON_CDM_FACTORY_DAEMON_PROXY_ASH_H_
