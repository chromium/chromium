// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_WELCOME_SET_AS_DEFAULT_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_WELCOME_SET_AS_DEFAULT_HANDLER_H_

#include "chrome/browser/ui/webui/settings/settings_default_browser_handler.h"

namespace welcome {

class SetAsDefaultHandler : public settings::DefaultBrowserHandler {
 public:
  SetAsDefaultHandler();

  SetAsDefaultHandler(const SetAsDefaultHandler&) = delete;
  SetAsDefaultHandler& operator=(const SetAsDefaultHandler&) = delete;

  ~SetAsDefaultHandler() override;

 protected:
  void RecordSetAsDefaultUMA() override;
};

}  // namespace welcome

#endif  // CHROME_BROWSER_UI_WEBUI_WELCOME_SET_AS_DEFAULT_HANDLER_H_
