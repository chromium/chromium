// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_SPLIT_MODIFIER_KEYBOARD_INFO_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_SPLIT_MODIFIER_KEYBOARD_INFO_SCREEN_HANDLER_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ui/webui/ash/login/base_screen_handler.h"
#include "components/login/localized_values_builder.h"

namespace ash {

class SplitModifierKeyboardInfoScreen;

// Interface for dependency injection between SplitModifierKeyboardInfoScreen
// and its WebUI representation.
class SplitModifierKeyboardInfoScreenView {
 public:
  inline constexpr static StaticOobeScreenId kScreenId{
      "split-modifier-keyboard-info", "SplitModifierKeyboardInfoScreen"};

  virtual ~SplitModifierKeyboardInfoScreenView() = default;

  // Shows the contents of the screen.
  virtual void Show() = 0;

  // Gets a WeakPtr to the instance.
  virtual base::WeakPtr<SplitModifierKeyboardInfoScreenView> AsWeakPtr() = 0;
};

class SplitModifierKeyboardInfoScreenHandler final
    : public BaseScreenHandler,
      public SplitModifierKeyboardInfoScreenView {
 public:
  using TView = SplitModifierKeyboardInfoScreenView;

  SplitModifierKeyboardInfoScreenHandler();

  SplitModifierKeyboardInfoScreenHandler(
      const SplitModifierKeyboardInfoScreenHandler&) = delete;
  SplitModifierKeyboardInfoScreenHandler& operator=(
      const SplitModifierKeyboardInfoScreenHandler&) = delete;

  ~SplitModifierKeyboardInfoScreenHandler() override;

  // BaseScreenHandler:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;

  // SplitModifierKeyboardInfoScreenView:
  void Show() override;
  base::WeakPtr<SplitModifierKeyboardInfoScreenView> AsWeakPtr() override;

 private:
  base::WeakPtrFactory<SplitModifierKeyboardInfoScreenView> weak_ptr_factory_{
      this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_SPLIT_MODIFIER_KEYBOARD_INFO_SCREEN_HANDLER_H_
