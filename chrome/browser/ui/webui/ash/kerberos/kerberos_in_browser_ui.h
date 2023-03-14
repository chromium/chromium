// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_KERBEROS_KERBEROS_IN_BROWSER_UI_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_KERBEROS_KERBEROS_IN_BROWSER_UI_H_

#include "base/memory/weak_ptr.h"
#include "ui/web_dialogs/web_dialog_ui.h"

namespace ash {
// Kerberos UI class. This UI is invoked, when Kerberos authentication
// fails in the browser in ChromeOS.
class KerberosInBrowserUI : public ui::WebDialogUI {
 public:
  explicit KerberosInBrowserUI(content::WebUI* web_ui);
  KerberosInBrowserUI(const KerberosInBrowserUI&) = delete;
  KerberosInBrowserUI& operator=(const KerberosInBrowserUI&) = delete;
  ~KerberosInBrowserUI() override;

 private:
  void OnManageTickets(const base::Value::List&);

  base::WeakPtrFactory<KerberosInBrowserUI> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_KERBEROS_KERBEROS_IN_BROWSER_UI_H_
