// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_CERTIFICATE_MANAGER_DIALOG_UI_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_CERTIFICATE_MANAGER_DIALOG_UI_H_

#include "ui/web_dialogs/web_dialog_ui.h"

namespace chromeos {

// A WebUI to host certificate manager UI.
class CertificateManagerDialogUI : public ui::WebDialogUI {
 public:
  explicit CertificateManagerDialogUI(content::WebUI* web_ui);

  CertificateManagerDialogUI(const CertificateManagerDialogUI&) = delete;
  CertificateManagerDialogUI& operator=(const CertificateManagerDialogUI&) =
      delete;

  ~CertificateManagerDialogUI() override;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_CERTIFICATE_MANAGER_DIALOG_UI_H_
