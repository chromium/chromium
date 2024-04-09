// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_CERTIFICATE_MANAGER_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_CERTIFICATE_MANAGER_HANDLER_H_

#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/webui/resources/cr_components/certificate_manager/certificate_manager_v2.mojom.h"

// Mojo handler for the Certificate Manager v2 page.
class CertificateManagerPageHandler
    : public certificate_manager_v2::mojom::CertificateManagerPageHandler {
 public:
  explicit CertificateManagerPageHandler(
      mojo::PendingRemote<certificate_manager_v2::mojom::CertificateManagerPage>
          pending_client,
      mojo::PendingReceiver<
          certificate_manager_v2::mojom::CertificateManagerPageHandler>
          pending_handler);

  CertificateManagerPageHandler(const CertificateManagerPageHandler&) = delete;
  CertificateManagerPageHandler& operator=(
      const CertificateManagerPageHandler&) = delete;

  ~CertificateManagerPageHandler() override;

  void GetChromeRootStoreCerts(
      GetChromeRootStoreCertsCallback callback) override;

 private:
  mojo::Remote<certificate_manager_v2::mojom::CertificateManagerPage>
      remote_client_;
  mojo::Receiver<certificate_manager_v2::mojom::CertificateManagerPageHandler>
      handler_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_CERTIFICATE_MANAGER_HANDLER_H_
