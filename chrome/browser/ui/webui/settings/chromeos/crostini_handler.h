// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_CROSTINI_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_CROSTINI_HANDLER_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"

class Profile;

namespace crostini {
enum class CrostiniResult;
}

namespace chromeos {
namespace settings {

class CrostiniHandler : public ::settings::SettingsPageUIHandler {
 public:
  explicit CrostiniHandler(Profile* profile);
  ~CrostiniHandler() override;

  // SettingsPageUIHandler
  void RegisterMessages() override;
  void OnJavascriptAllowed() override {}
  void OnJavascriptDisallowed() override {}

 private:
  void HandleRequestCrostiniInstallerView(const base::ListValue* args);
  void HandleRequestRemoveCrostini(const base::ListValue* args);
  // Callback for the "getSharedPathsDisplayText" message.  Converts actual
  // paths in chromeos to values suitable to display to users.
  // E.g. /home/chronos/u-<hash>/Downloads/foo => "Downloads > foo".
  void HandleGetCrostiniSharedPathsDisplayText(const base::ListValue* args);
  // Remove a specified path from being shared.
  void HandleRemoveCrostiniSharedPath(const base::ListValue* args);

  Profile* profile_;
  // weak_ptr_factory_ should always be last member.
  base::WeakPtrFactory<CrostiniHandler> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(CrostiniHandler);
};

}  // namespace settings
}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_CROSTINI_HANDLER_H_
