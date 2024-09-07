// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_INPUT_METHOD_ASSISTIVE_DELEGATE_H_
#define CHROME_BROWSER_UI_ASH_INPUT_METHOD_ASSISTIVE_DELEGATE_H_

#include <optional>
#include <string>
#include <vector>

#include "chromeos/ash/services/ime/public/cpp/assistive_suggestions.h"
#include "ui/chromeos/ui_chromeos_export.h"

namespace ui::ime {

enum class ButtonId {
  kNone,
  kUndo,
  kAddToDictionary,
  kSmartInputsSettingLink,
  kSuggestion,
  kLearnMore,
  kIgnoreSuggestion,
};

struct AssistiveWindowButton {
  ButtonId id = ButtonId::kNone;
  ash::ime::AssistiveWindowType window_type =
      ash::ime::AssistiveWindowType::kNone;
  size_t suggestion_index = -1;
  std::u16string announce_string;

  bool operator==(const AssistiveWindowButton& other) const {
    return id == other.id && window_type == other.window_type &&
           suggestion_index == other.suggestion_index &&
           announce_string == other.announce_string;
  }
};

class UI_CHROMEOS_EXPORT AssistiveDelegate {
 public:
  // Invoked when a button in an assistive window is clicked.
  virtual void AssistiveWindowButtonClicked(
      const AssistiveWindowButton& button) const = 0;

  // This method is invoked whenever there is a change to the suggestion state
  // in the assistive window. If a new suggestion is shown, accepted, dismissed,
  // updated, etc, then this method is invoked.
  virtual void AssistiveWindowChanged(
      const ash::ime::AssistiveWindow& window) const = 0;

 protected:
  virtual ~AssistiveDelegate() = default;
};

}  // namespace ui::ime

#endif  //  CHROME_BROWSER_UI_ASH_INPUT_METHOD_ASSISTIVE_DELEGATE_H_
