// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_DISCOVER_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_DISCOVER_SCREEN_HANDLER_H_

#include "base/macros.h"
#include "chrome/browser/ui/webui/chromeos/login/base_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/discover/discover_ui.h"

namespace chromeos {

class DiscoverScreen;

// Interface for dependency injection between DiscoverScreen and its
// WebUI representation.
class DiscoverScreenView {
 public:
  constexpr static StaticOobeScreenId kScreenId{"discover"};

  virtual ~DiscoverScreenView() = default;

  // Sets screen this view belongs to.
  virtual void Bind(DiscoverScreen* screen) = 0;

  // Shows the contents of the screen.
  virtual void Show() = 0;

  // Hides the contents of the screen.
  virtual void Hide() = 0;
};

// The sole implementation of the DiscoverScreenView, using WebUI.
class DiscoverScreenHandler : public BaseScreenHandler,
                              public DiscoverScreenView {
 public:
  using TView = DiscoverScreenView;

  explicit DiscoverScreenHandler(JSCallsContainer* js_calls_container);
  ~DiscoverScreenHandler() override;

  // BaseScreenHandler:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;
  void GetAdditionalParameters(base::DictionaryValue* dict) override;
  void RegisterMessages() override;

  // DiscoverScreenView:
  void Bind(DiscoverScreen* screen) override;
  void Hide() override;
  void Initialize() override;
  void Show() override;

 private:
  DiscoverScreen* screen_ = nullptr;

  DiscoverUI discover_ui_;

  DISALLOW_COPY_AND_ASSIGN(DiscoverScreenHandler);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_DISCOVER_SCREEN_HANDLER_H_
