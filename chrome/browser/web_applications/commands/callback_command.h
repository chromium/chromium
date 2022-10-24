// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_CALLBACK_COMMAND_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_CALLBACK_COMMAND_H_

#include "base/callback.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"

namespace web_app {

class LockDescription;

// CallbackCommand simply runs the callback being passed. This is handy for
// small operations to web app system to avoid defining a new command class but
// still providing isolation for the work done in the callback.
class CallbackCommand : public WebAppCommand {
 public:
  CallbackCommand(std::unique_ptr<LockDescription> lock_description,
                  base::OnceClosure callback);
  ~CallbackCommand() override;

  void Start() override;

  LockDescription& lock_description() const override;

  base::Value ToDebugValue() const override;

  void OnSyncSourceRemoved() override {}
  void OnShutdown() override {}

 private:
  std::unique_ptr<LockDescription> lock_description_;
  base::OnceClosure callback_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_CALLBACK_COMMAND_H_
