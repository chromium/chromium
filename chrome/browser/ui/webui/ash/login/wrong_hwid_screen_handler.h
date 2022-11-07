// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_WRONG_HWID_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_WRONG_HWID_SCREEN_HANDLER_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/webui/ash/login/base_screen_handler.h"

namespace ash {

// Interface between wrong HWID screen and its representation.
class WrongHWIDScreenView : public base::SupportsWeakPtr<WrongHWIDScreenView> {
 public:
  inline constexpr static StaticOobeScreenId kScreenId{
      "wrong-hwid", "WrongHWIDMessageScreen"};

  virtual ~WrongHWIDScreenView() = default;

  virtual void Show() = 0;
};

// WebUI implementation of WrongHWIDScreenActor.
class WrongHWIDScreenHandler : public WrongHWIDScreenView,
                               public BaseScreenHandler {
 public:
  using TView = WrongHWIDScreenView;

  WrongHWIDScreenHandler();

  WrongHWIDScreenHandler(const WrongHWIDScreenHandler&) = delete;
  WrongHWIDScreenHandler& operator=(const WrongHWIDScreenHandler&) = delete;

  ~WrongHWIDScreenHandler() override;

 private:
  // WrongHWIDScreenActor implementation:
  void Show() override;

  // BaseScreenHandler implementation:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_WRONG_HWID_SCREEN_HANDLER_H_
