// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_FAMILY_LINK_NOTICE_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_FAMILY_LINK_NOTICE_SCREEN_HANDLER_H_

#include <string>

#include "chrome/browser/ui/webui/chromeos/login/base_screen_handler.h"

namespace chromeos {

class FamilyLinkNoticeScreen;

// Interface for dependency injection between FamilyLinkNoticeScreen and its
// WebUI representation.
class FamilyLinkNoticeView {
 public:
  constexpr static StaticOobeScreenId kScreenId{"family-link-notice"};

  virtual ~FamilyLinkNoticeView() = default;

  // Shows the contents of the screen.
  virtual void Show() = 0;

  // Binds `screen` to the view.
  virtual void Bind(FamilyLinkNoticeScreen* screen) = 0;

  // Unbinds the screen from the view.
  virtual void Unbind() = 0;

  // Set if account is a new gaia account user just created.
  virtual void SetIsNewGaiaAccount(bool value) = 0;

  // Set email to be displayed.
  virtual void SetDisplayEmail(const std::string& value) = 0;

  // Set enterprise domain to be displayed.
  virtual void SetDomain(const std::string& value) = 0;
};

class FamilyLinkNoticeScreenHandler : public FamilyLinkNoticeView,
                                      public BaseScreenHandler {
 public:
  using TView = FamilyLinkNoticeView;

  explicit FamilyLinkNoticeScreenHandler(JSCallsContainer* js_calls_container);

  ~FamilyLinkNoticeScreenHandler() override;

  FamilyLinkNoticeScreenHandler(const FamilyLinkNoticeScreenHandler&) = delete;
  FamilyLinkNoticeScreenHandler& operator=(
      const FamilyLinkNoticeScreenHandler&) = delete;

 private:
  void Show() override;
  void Bind(FamilyLinkNoticeScreen* screen) override;
  void Unbind() override;
  void SetIsNewGaiaAccount(bool value) override;
  void SetDisplayEmail(const std::string& value) override;
  void SetDomain(const std::string& value) override;

  // BaseScreenHandler:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;
  void Initialize() override;

  FamilyLinkNoticeScreen* screen_ = nullptr;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_FAMILY_LINK_NOTICE_SCREEN_HANDLER_H_
