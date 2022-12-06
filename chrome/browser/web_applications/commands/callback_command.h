// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_CALLBACK_COMMAND_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_CALLBACK_COMMAND_H_

#include <memory>

#include "base/callback.h"
#include "base/values.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"

namespace web_app {

class LockDescription;

// CallbackCommand simply runs the callback being passed. This is handy for
// small operations to web app system to avoid defining a new command class but
// still providing isolation for the work done in the callback.
template <typename LockType,
          typename DescriptionType = typename LockType::LockDescription>
class CallbackCommand : public WebAppCommandTemplate<LockType> {
 public:
  CallbackCommand(const std::string& name,
                  std::unique_ptr<DescriptionType> lock_description,
                  base::OnceCallback<void(LockType& lock)> callback);
  CallbackCommand(const std::string& name,
                  std::unique_ptr<DescriptionType> lock_description,
                  base::OnceCallback<base::Value(LockType& lock)> callback);

  ~CallbackCommand() override;

  void StartWithLock(std::unique_ptr<LockType> lock) override;

  LockDescription& lock_description() const override;

  base::Value ToDebugValue() const override;

  void OnSyncSourceRemoved() override {}
  void OnShutdown() override {}

 private:
  std::unique_ptr<DescriptionType> lock_description_;
  std::unique_ptr<LockType> lock_;

  base::OnceCallback<base::Value(LockType& lock)> callback_;
  base::Value debug_value_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMMANDS_CALLBACK_COMMAND_H_
