// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_PROMPT_BEHAVIOR_H_
#define CHROME_TEST_CHROMEDRIVER_PROMPT_BEHAVIOR_H_

#include <stddef.h>

#include "base/values.h"
#include "chrome/test/chromedriver/chrome/status.h"

class Status;

enum class PromptHandlerType { kAccept, kDismiss, kIgnore };
struct PromptHandlerConfiguration {
  PromptHandlerType type;
  bool notify;
};

namespace prompt_behavior {
inline constexpr char kAccept[] = "accept";
inline constexpr char kAcceptAndNotify[] = "accept and notify";
inline constexpr char kDismiss[] = "dismiss";
inline constexpr char kDismissAndNotify[] = "dismiss and notify";
inline constexpr char kIgnore[] = "ignore";
}  // namespace prompt_behavior

namespace dialog_types {
inline constexpr char kAlert[] = "alert";
inline constexpr char kBeforeUnload[] = "beforeUnload";
inline constexpr char kConfirm[] = "confirm";
inline constexpr char kPrompt[] = "prompt";
}  // namespace dialog_types

class PromptBehavior {
 public:
  PromptBehavior();
  explicit PromptBehavior(bool w3c_compliant);
  static Status Create(bool w3c_compliant, PromptBehavior& result);
  static Status Create(bool w3c_compliant,
                       const base::Value& prompt_behavior,
                       PromptBehavior& result);
  base::Value CapabilityView();
  base::Value MapperOptionsView();
  Status GetConfiguration(const std::string& dialog_type,
                          PromptHandlerConfiguration& handler_configuration);

 private:
  base::Value original_capability_value;
  PromptHandlerConfiguration alert;
  PromptHandlerConfiguration before_unload;
  PromptHandlerConfiguration confirm;
  PromptHandlerConfiguration prompt;
  // Required to return session capabilities in the right format.

  static Status Create(bool w3c_compliant,
                       const std::string& prompt_behavior,
                       PromptBehavior& result);
  static Status Create(bool w3c_compliant,
                       const base::DictValue& prompt_behavior,
                       PromptBehavior& result);
};

#endif  // CHROME_TEST_CHROMEDRIVER_PROMPT_BEHAVIOR_H_
