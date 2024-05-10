// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_CERTIFICATE_MANAGER_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_CERTIFICATE_MANAGER_HANDLER_H_

#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/webui/resources/cr_components/certificate_manager/certificate_manager_v2.mojom.h"

class Profile;
namespace content {
class WebContents;
}  // namespace content

// Mojo handler for the Certificate Manager v2 page.
class CertificateManagerPageHandler
    : public certificate_manager_v2::mojom::CertificateManagerPageHandler {
 public:
  explicit CertificateManagerPageHandler(
      mojo::PendingRemote<certificate_manager_v2::mojom::CertificateManagerPage>
          pending_client,
      mojo::PendingReceiver<
          certificate_manager_v2::mojom::CertificateManagerPageHandler>
          pending_handler,
      Profile* profile,
      content::WebContents* web_contents);

  CertificateManagerPageHandler(const CertificateManagerPageHandler&) = delete;
  CertificateManagerPageHandler& operator=(
      const CertificateManagerPageHandler&) = delete;

  ~CertificateManagerPageHandler() override;

  void GetChromeRootStoreCerts(
      GetChromeRootStoreCertsCallback callback) override;
  void ViewCertificate(const std::string& sha256_hex_hash) override;
  void GetPlatformClientCerts(GetPlatformClientCertsCallback callback) override;
  void ExportChromeRootStore() override;
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  void GetProvisionedClientCerts(
      GetProvisionedClientCertsCallback callback) override;
#endif

 private:
  mojo::Remote<certificate_manager_v2::mojom::CertificateManagerPage>
      remote_client_;
  mojo::Receiver<certificate_manager_v2::mojom::CertificateManagerPageHandler>
      handler_;
  raw_ptr<Profile> profile_;
  raw_ptr<content::WebContents> web_contents_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_CERTIFICATE_MANAGER_HANDLER_H_
