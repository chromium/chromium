// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_ADD_CHILD_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_ADD_CHILD_SCREEN_HANDLER_H_

#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/ui/webui/ash/login/base_screen_handler.h"

namespace ash {

// Interface for dependency injection between AddChildScreen and its
// WebUI representation.
class AddChildScreenView {
 public:
  inline constexpr static StaticOobeScreenId kScreenId{"add-child",
                                                       "AddChildScreen"};

  virtual ~AddChildScreenView() = default;

  // Shows the contents of the screen.
  virtual void Show() = 0;

  // Gets a WeakPtr to the instance.
  virtual base::WeakPtr<AddChildScreenView> AsWeakPtr() = 0;
};

class AddChildScreenHandler final : public BaseScreenHandler,
                                    public AddChildScreenView {
 public:
  using TView = AddChildScreenView;

  AddChildScreenHandler();

  AddChildScreenHandler(const AddChildScreenHandler&) = delete;
  AddChildScreenHandler& operator=(const AddChildScreenHandler&) = delete;

  ~AddChildScreenHandler() override;

  // BaseScreenHandler:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;

  // AddChildScreenView:
  void Show() override;
  base::WeakPtr<AddChildScreenView> AsWeakPtr() override;

 private:
  base::WeakPtrFactory<AddChildScreenView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_ADD_CHILD_SCREEN_HANDLER_H_
