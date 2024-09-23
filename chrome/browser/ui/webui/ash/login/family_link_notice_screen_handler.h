// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_FAMILY_LINK_NOTICE_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_FAMILY_LINK_NOTICE_SCREEN_HANDLER_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/webui/ash/login/base_screen_handler.h"

namespace ash {

// Interface for dependency injection between FamilyLinkNoticeScreen and its
// WebUI representation.
class FamilyLinkNoticeView {
 public:
  inline constexpr static StaticOobeScreenId kScreenId{
      "family-link-notice", "FamilyLinkNoticeScreen"};

  virtual ~FamilyLinkNoticeView() = default;

  // Shows the contents of the screen.
  virtual void Show() = 0;

  // Set if account is a new gaia account user just created.
  virtual void SetIsNewGaiaAccount(bool value) = 0;

  // Set email to be displayed.
  virtual void SetDisplayEmail(const std::string& value) = 0;

  // Set enterprise domain to be displayed.
  virtual void SetDomain(const std::string& value) = 0;

  // Gets a WeakPtr to the instance.
  virtual base::WeakPtr<FamilyLinkNoticeView> AsWeakPtr() = 0;
};

class FamilyLinkNoticeScreenHandler final : public FamilyLinkNoticeView,
                                            public BaseScreenHandler {
 public:
  using TView = FamilyLinkNoticeView;

  FamilyLinkNoticeScreenHandler();

  ~FamilyLinkNoticeScreenHandler() override;

  FamilyLinkNoticeScreenHandler(const FamilyLinkNoticeScreenHandler&) = delete;
  FamilyLinkNoticeScreenHandler& operator=(
      const FamilyLinkNoticeScreenHandler&) = delete;

 private:
  void Show() override;
  void SetIsNewGaiaAccount(bool value) override;
  void SetDisplayEmail(const std::string& value) override;
  void SetDomain(const std::string& value) override;
  base::WeakPtr<FamilyLinkNoticeView> AsWeakPtr() override;

  // BaseScreenHandler:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;

  base::WeakPtrFactory<FamilyLinkNoticeView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_FAMILY_LINK_NOTICE_SCREEN_HANDLER_H_
