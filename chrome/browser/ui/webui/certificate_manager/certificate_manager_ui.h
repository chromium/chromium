// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CERTIFICATE_MANAGER_CERTIFICATE_MANAGER_UI_H_
#define CHROME_BROWSER_UI_WEBUI_CERTIFICATE_MANAGER_CERTIFICATE_MANAGER_UI_H_

#include "chrome/browser/ui/webui/certificate_manager/certificate_manager_handler.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"
#include "ui/web_dialogs/web_dialog_ui.h"
#include "ui/webui/resources/cr_components/certificate_manager/certificate_manager_v2.mojom.h"

class CertificateManagerUI;

// WebUIConfig for chrome://certificate-manager
class CertificateManagerUIConfig
    : public content::DefaultWebUIConfig<CertificateManagerUI> {
 public:
  CertificateManagerUIConfig()
      : DefaultWebUIConfig(content::kChromeUIScheme,
                           chrome::kChromeUICertificateManagerHost) {}
};

// A WebUI to host certificate manager UI. On ChromeOS, this must be a dialog
// because it can be opened in a dialog window by the login screen (see
// chrome/browser/ash/login/screens/error_screen.cc).
class CertificateManagerUI
#if BUILDFLAG(IS_CHROMEOS)
    : public ui::MojoWebDialogUI
#else
    : public ui::MojoWebUIController
#endif
#if BUILDFLAG(CHROME_ROOT_STORE_CERT_MANAGEMENT_UI)
    ,
      public certificate_manager_v2::mojom::CertificateManagerPageHandlerFactory
#endif  // BUILDFLAG(CHROME_ROOT_STORE_CERT_MANAGEMENT_UI)
{
 public:
  explicit CertificateManagerUI(content::WebUI* web_ui);

  CertificateManagerUI(const CertificateManagerUI&) = delete;
  CertificateManagerUI& operator=(const CertificateManagerUI&) = delete;

  ~CertificateManagerUI() override;

#if BUILDFLAG(CHROME_ROOT_STORE_CERT_MANAGEMENT_UI)
  void BindInterface(
      mojo::PendingReceiver<
          certificate_manager_v2::mojom::CertificateManagerPageHandlerFactory>
          pending_receiver);
#endif  // BUILDFLAG(CHROME_ROOT_STORE_CERT_MANAGEMENT_UI)
 private:
#if BUILDFLAG(CHROME_ROOT_STORE_CERT_MANAGEMENT_UI)
  // certificate_manager_v2::mojom::CertificateManagerPageHandlerFactory
  void CreateCertificateManagerPageHandler(
      mojo::PendingRemote<certificate_manager_v2::mojom::CertificateManagerPage>
          client,
      mojo::PendingReceiver<
          certificate_manager_v2::mojom::CertificateManagerPageHandler> handler)
      override;

  std::unique_ptr<CertificateManagerPageHandler>
      certificate_manager_page_handler_;
  mojo::Receiver<
      certificate_manager_v2::mojom::CertificateManagerPageHandlerFactory>
      certificate_manager_handler_factory_receiver_{this};
#endif  // BUILDFLAG(CHROME_ROOT_STORE_CERT_MANAGEMENT_UI)

  WEB_UI_CONTROLLER_TYPE_DECL();
};

#endif  // CHROME_BROWSER_UI_WEBUI_CERTIFICATE_MANAGER_CERTIFICATE_MANAGER_UI_H_
