// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_CDM_FACTORY_DAEMON_CDM_FACTORY_DAEMON_PROXY_LACROS_H_
#define CHROMEOS_COMPONENTS_CDM_FACTORY_DAEMON_CDM_FACTORY_DAEMON_PROXY_LACROS_H_

#include "chromeos/components/cdm_factory_daemon/cdm_factory_daemon_proxy.h"

#include "base/component_export.h"
#include "base/functional/callback_forward.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace chromeos {

// This class serves as a proxy between the lacros-browser and ash-browser
// processes for HW DRM implementations. It just establishes a connection to
// ash-chrome and then proxies all requests there. Those will then end up in a
// ChromeOS daemon where the actual CDM/OEMCrypto implementations live.
class COMPONENT_EXPORT(CDM_FACTORY_DAEMON) CdmFactoryDaemonProxyLacros
    : public CdmFactoryDaemonProxy {
 public:
  CdmFactoryDaemonProxyLacros();

  CdmFactoryDaemonProxyLacros(const CdmFactoryDaemonProxyLacros&) = delete;
  CdmFactoryDaemonProxyLacros& operator=(const CdmFactoryDaemonProxyLacros&) =
      delete;

  ~CdmFactoryDaemonProxyLacros() override;

  static void Create(mojo::PendingReceiver<BrowserCdmFactory> receiver);

  static CdmFactoryDaemonProxyLacros& GetInstance();

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
  void EstablishAshConnection(base::OnceClosure callback);
  void GetFactoryInterface(const std::string& key_system,
                           CreateFactoryCallback callback);
  void ProxyGetHwConfigData(GetHwConfigDataCallback callback);

  mojo::Remote<BrowserCdmFactory> ash_remote_;
};

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_CDM_FACTORY_DAEMON_CDM_FACTORY_DAEMON_PROXY_LACROS_H_
