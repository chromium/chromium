// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_INSTALL_ISOLATED_APP_COMMAND_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_INSTALL_ISOLATED_APP_COMMAND_H_

#include "base/memory/weak_ptr.h"
#include "base/strings/string_piece_forward.h"
#include "base/values.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"

namespace web_app {

class InstallIsolatedAppCommand : public WebAppCommand {
 public:
  explicit InstallIsolatedAppCommand(base::StringPiece application_url);
  ~InstallIsolatedAppCommand() override;

  base::Value ToDebugValue() const override;

  void Start() override;
  void OnSyncSourceRemoved() override;
  void OnShutdown() override;

 private:
  base::WeakPtr<InstallIsolatedAppCommand> weak_this_;
  base::WeakPtrFactory<InstallIsolatedAppCommand> weak_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_INSTALL_ISOLATED_APP_COMMAND_H_
