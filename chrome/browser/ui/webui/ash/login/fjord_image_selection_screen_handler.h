// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_FJORD_IMAGE_SELECTION_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_FJORD_IMAGE_SELECTION_SCREEN_HANDLER_H_

#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ui/webui/ash/login/base_screen_handler.h"
#include "components/login/localized_values_builder.h"

namespace ash {

class FjordImageSelectionScreen;

// Interface for dependency injection between FjordImageSelectionScreen and its
// WebUI representation.
class FjordImageSelectionScreenView {
 public:
  inline constexpr static StaticOobeScreenId kScreenId{
      "fjord-image-selection", "FjordImageSelectionScreen"};

  virtual ~FjordImageSelectionScreenView() = default;

  // Shows the contents of the screen.
  virtual void Show(base::DictValue data) = 0;

  // Gets a WeakPtr to the instance.
  virtual base::WeakPtr<FjordImageSelectionScreenView> AsWeakPtr() = 0;
};

class FjordImageSelectionScreenHandler final
    : public BaseScreenHandler,
      public FjordImageSelectionScreenView {
 public:
  using TView = FjordImageSelectionScreenView;

  FjordImageSelectionScreenHandler();
  FjordImageSelectionScreenHandler(const FjordImageSelectionScreenHandler&) =
      delete;
  FjordImageSelectionScreenHandler& operator=(
      const FjordImageSelectionScreenHandler&) = delete;
  ~FjordImageSelectionScreenHandler() override;

  // BaseScreenHandler:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;

  // FjordImageSelectionScreenView:
  void Show(base::DictValue data) override;
  base::WeakPtr<FjordImageSelectionScreenView> AsWeakPtr() override;

 private:
  base::WeakPtrFactory<FjordImageSelectionScreenView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_FJORD_IMAGE_SELECTION_SCREEN_HANDLER_H_
