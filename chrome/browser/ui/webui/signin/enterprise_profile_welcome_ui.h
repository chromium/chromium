// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIGNIN_ENTERPRISE_PROFILE_WELCOME_UI_H_
#define CHROME_BROWSER_UI_WEBUI_SIGNIN_ENTERPRISE_PROFILE_WELCOME_UI_H_

#include "base/callback.h"
#include "chrome/browser/ui/webui/signin/signin_web_dialog_ui.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/skia/include/core/SkColor.h"

class Browser;
class EnterpriseProfileWelcomeHandler;
struct AccountInfo;

namespace content {
class WebUI;
}

class EnterpriseProfileWelcomeUI : public SigninWebDialogUI {
 public:
  // Type of a welcome screen for the enterprise flow.
  enum class ScreenType {
    kEntepriseAccountSyncEnabled,
    kEntepriseAccountSyncDisabled,
    kConsumerAccountSyncDisabled,
    kEnterpriseAccountCreation
  };

  explicit EnterpriseProfileWelcomeUI(content::WebUI* web_ui);
  ~EnterpriseProfileWelcomeUI() override;

  EnterpriseProfileWelcomeUI(const EnterpriseProfileWelcomeUI&) = delete;
  EnterpriseProfileWelcomeUI& operator=(const EnterpriseProfileWelcomeUI&) =
      delete;

  // Initializes the EnterpriseProfileWelcomeUI.
  void Initialize(Browser* browser,
                  ScreenType type,
                  const AccountInfo& account_info,
                  absl::optional<SkColor> profile_color,
                  base::OnceCallback<void(bool)> proceed_callback);

  // Allows tests to trigger page events.
  EnterpriseProfileWelcomeHandler* GetHandlerForTesting();

  // SigninWebDialogUI:
  void InitializeMessageHandlerWithBrowser(Browser* browser) override;

 private:
  // Stored for tests.
  EnterpriseProfileWelcomeHandler* handler_ = nullptr;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

#endif  // CHROME_BROWSER_UI_WEBUI_SIGNIN_ENTERPRISE_PROFILE_WELCOME_UI_H_
