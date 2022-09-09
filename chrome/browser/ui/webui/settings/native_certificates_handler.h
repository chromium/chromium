// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_NATIVE_CERTIFICATES_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_NATIVE_CERTIFICATES_HANDLER_H_

#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"

namespace settings {

class NativeCertificatesHandler : public SettingsPageUIHandler {
 public:
  NativeCertificatesHandler();

  NativeCertificatesHandler(const NativeCertificatesHandler&) = delete;
  NativeCertificatesHandler& operator=(const NativeCertificatesHandler&) =
      delete;

  ~NativeCertificatesHandler() override;

  // SettingsPageUIHandler implementation.
  void RegisterMessages() override;
  void OnJavascriptAllowed() override {}
  void OnJavascriptDisallowed() override {}

  // Callback for the "showManageSSLCertificates" message. This will invoke
  // an appropriate certificate management action based on the platform.
  void HandleShowManageSSLCertificates(const base::Value::List& args);
};

}  // namespace settings

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_NATIVE_CERTIFICATES_HANDLER_H_
