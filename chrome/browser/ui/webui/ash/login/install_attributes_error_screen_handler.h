// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_INSTALL_ATTRIBUTES_ERROR_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_INSTALL_ATTRIBUTES_ERROR_SCREEN_HANDLER_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/webui/ash/login/base_screen_handler.h"

namespace ash {

// Interface for dependency injection between InstallAttributesErrorScreen and
// its WebUI representation.
class InstallAttributesErrorView
    : public base::SupportsWeakPtr<InstallAttributesErrorView> {
 public:
  inline constexpr static StaticOobeScreenId kScreenId{
      "install-attributes-error-message",
      "InstallAttributesErrorMessageScreen"};

  virtual ~InstallAttributesErrorView() = default;

  // Shows the contents of the screen.
  virtual void Show() = 0;
};

class InstallAttributesErrorScreenHandler : public InstallAttributesErrorView,
                                            public BaseScreenHandler {
 public:
  using TView = InstallAttributesErrorView;

  InstallAttributesErrorScreenHandler();
  InstallAttributesErrorScreenHandler(
      const InstallAttributesErrorScreenHandler&) = delete;
  InstallAttributesErrorScreenHandler& operator=(
      const InstallAttributesErrorScreenHandler&) = delete;
  ~InstallAttributesErrorScreenHandler() override;

 private:
  void Show() override;

  // BaseScreenHandler:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_INSTALL_ATTRIBUTES_ERROR_SCREEN_HANDLER_H_
