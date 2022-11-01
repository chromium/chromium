// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_CERTIFICATE_MANAGER_DIALOG_UI_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_CERTIFICATE_MANAGER_DIALOG_UI_H_

#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"
#include "ui/web_dialogs/web_dialog_ui.h"

namespace ash {

class CertificateManagerDialogUI;

// WebUIConfig for chrome://certificate-manager
class CertificateManagerDialogUIConfig
    : public content::DefaultWebUIConfig<CertificateManagerDialogUI> {
 public:
  CertificateManagerDialogUIConfig()
      : DefaultWebUIConfig(content::kChromeUIScheme,
                           chrome::kChromeUICertificateManagerHost) {}
};

// A WebUI to host certificate manager UI.
class CertificateManagerDialogUI : public ui::WebDialogUI {
 public:
  explicit CertificateManagerDialogUI(content::WebUI* web_ui);

  CertificateManagerDialogUI(const CertificateManagerDialogUI&) = delete;
  CertificateManagerDialogUI& operator=(const CertificateManagerDialogUI&) =
      delete;

  ~CertificateManagerDialogUI() override;
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_CERTIFICATE_MANAGER_DIALOG_UI_H_
