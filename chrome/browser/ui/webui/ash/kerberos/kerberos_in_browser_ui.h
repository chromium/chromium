// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_KERBEROS_KERBEROS_IN_BROWSER_UI_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_KERBEROS_KERBEROS_IN_BROWSER_UI_H_

#include "ash/webui/common/chrome_os_webui_config.h"
#include "base/memory/weak_ptr.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/common/url_constants.h"
#include "ui/web_dialogs/web_dialog_ui.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace ash {

class KerberosInBrowserUI;

// The WebUIConfig for the Kerberos UI class.
class KerberosInBrowserUIConfig
    : public ChromeOSWebUIConfig<KerberosInBrowserUI> {
 public:
  KerberosInBrowserUIConfig();

  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;
};

// Kerberos UI class. This UI is invoked, when Kerberos authentication
// fails in the browser in ChromeOS.
class KerberosInBrowserUI : public ui::WebDialogUI {
 public:
  explicit KerberosInBrowserUI(content::WebUI* web_ui);
  KerberosInBrowserUI(const KerberosInBrowserUI&) = delete;
  KerberosInBrowserUI& operator=(const KerberosInBrowserUI&) = delete;
  ~KerberosInBrowserUI() override;

 private:
  base::WeakPtrFactory<KerberosInBrowserUI> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_KERBEROS_KERBEROS_IN_BROWSER_UI_H_
