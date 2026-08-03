// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIGNIN_MANAGED_USER_PROFILE_NOTICE_UI_H_
#define CHROME_BROWSER_UI_WEBUI_SIGNIN_MANAGED_USER_PROFILE_NOTICE_UI_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/profiles/profile_statistics_common.h"
#include "chrome/browser/ui/webui/signin/signin_utils.h"
#include "content/public/browser/web_contents_user_data.h"
#include "content/public/browser/web_ui_controller.h"

#if !BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_ANDROID)
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"
#endif  //  !BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_ANDROID)

class BrowserWindowInterface;
class ManagedUserProfileNoticeHandler;

namespace content {
class WebUI;
}

#if !BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_ANDROID)
class ManagedUserProfileNoticeUI;

class ManagedUserProfileNoticeUIConfig
    : public content::DefaultWebUIConfig<ManagedUserProfileNoticeUI> {
 public:
  ManagedUserProfileNoticeUIConfig()
      : DefaultWebUIConfig(content::kChromeUIScheme,
                           chrome::kChromeUIManagedUserProfileNoticeHost) {}
};
#endif  // !BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_ANDROID)

class ManagedUserProfileNoticeUI : public content::WebUIController {
 public:
  // Type of a managed user notice screen.
  // LINT.IfChange(ScreenType)
  enum class ScreenType {
    kEntepriseAccountSyncEnabled,
    kEntepriseAccountSyncDisabled,
    kConsumerAccountSyncDisabled,
    kEnterpriseAccountCreation,
    kEnterpriseOIDC,
    kProfilePicker,
    kFirstRun,
    kDeviceSignalsDisclaimer,
    kMaxValue = kDeviceSignalsDisclaimer
  };
  // LINT.ThenChange(//chrome/browser/resources/signin/managed_user_profile_notice/managed_user_profile_notice_browser_proxy.ts:ScreenType)

  explicit ManagedUserProfileNoticeUI(content::WebUI* web_ui);
  ~ManagedUserProfileNoticeUI() override;

  ManagedUserProfileNoticeUI(const ManagedUserProfileNoticeUI&) = delete;
  ManagedUserProfileNoticeUI& operator=(const ManagedUserProfileNoticeUI&) =
      delete;

  // Allows tests to trigger page events.
  ManagedUserProfileNoticeHandler* GetHandlerForTesting();

 private:
  void UpdateBrowsingDataStringWithCounts(std::u16string domain,
                                          profiles::ProfileCategoryStats stats);


  // Stored for tests.
  raw_ptr<ManagedUserProfileNoticeHandler> handler_ = nullptr;
  base::WeakPtrFactory<ManagedUserProfileNoticeUI> weak_ptr_factory_{this};

  WEB_UI_CONTROLLER_TYPE_DECL();
};

// Stores the params for the managed user profile notice UI.
class ManagedUserProfileNoticeParams
    : public content::WebContentsUserData<ManagedUserProfileNoticeParams> {
 public:
  ManagedUserProfileNoticeParams(
      content::WebContents* web_contents,
      BrowserWindowInterface* browser,
      ManagedUserProfileNoticeUI::ScreenType type,
      std::unique_ptr<signin::EnterpriseProfileCreationDialogParams>
          create_param);
  ~ManagedUserProfileNoticeParams() override;

  BrowserWindowInterface* browser() const { return browser_; }
  ManagedUserProfileNoticeUI::ScreenType type() const { return type_; }
  std::unique_ptr<signin::EnterpriseProfileCreationDialogParams>
  ReleaseCreateParam() {
    return std::move(create_param_);
  }

 private:
  friend class content::WebContentsUserData<ManagedUserProfileNoticeParams>;

  raw_ptr<BrowserWindowInterface> browser_ = nullptr;
  ManagedUserProfileNoticeUI::ScreenType type_;
  std::unique_ptr<signin::EnterpriseProfileCreationDialogParams> create_param_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_UI_WEBUI_SIGNIN_MANAGED_USER_PROFILE_NOTICE_UI_H_
