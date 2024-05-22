// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_CATEGORIES_SELECTION_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_CATEGORIES_SELECTION_SCREEN_HANDLER_H_

#include "base/values.h"
#include "chrome/browser/ui/webui/ash/login/base_screen_handler.h"

namespace ash {

// Interface for dependency injection between CategoriesSelectionScreen and its
// WebUI representation.
class CategoriesSelectionScreenView {
 public:
  inline constexpr static StaticOobeScreenId kScreenId{
      "categories-selection", "CategoriesSelectionScreen"};

  virtual ~CategoriesSelectionScreenView() = default;

  // Shows the contents of the screen.
  virtual void Show() = 0;

  virtual void SetCategoriesData(base::Value::Dict categories) = 0;
  virtual void SetOverviewStep() = 0;

  // Gets a WeakPtr to the instance.
  virtual base::WeakPtr<CategoriesSelectionScreenView> AsWeakPtr() = 0;
};

class CategoriesSelectionScreenHandler : public BaseScreenHandler,
                                         public CategoriesSelectionScreenView {
 public:
  using TView = CategoriesSelectionScreenView;

  CategoriesSelectionScreenHandler();

  CategoriesSelectionScreenHandler(const CategoriesSelectionScreenHandler&) =
      delete;
  CategoriesSelectionScreenHandler& operator=(
      const CategoriesSelectionScreenHandler&) = delete;

  ~CategoriesSelectionScreenHandler() override;

  // BaseScreenHandler:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;

  // CategoriesSelectionScreenView:
  void Show() override;
  void SetCategoriesData(base::Value::Dict categories) override;
  void SetOverviewStep() override;
  base::WeakPtr<CategoriesSelectionScreenView> AsWeakPtr() override;

 private:
  base::WeakPtrFactory<CategoriesSelectionScreenView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_CATEGORIES_SELECTION_SCREEN_HANDLER_H_
