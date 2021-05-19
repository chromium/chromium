// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PAGE_INFO_CHROME_PAGE_INFO_UI_DELEGATE_H_
#define CHROME_BROWSER_UI_PAGE_INFO_CHROME_PAGE_INFO_UI_DELEGATE_H_

#include <string>

#include "build/build_config.h"
#include "components/page_info/page_info_ui_delegate.h"
#include "url/gurl.h"

class Profile;

class ChromePageInfoUiDelegate : public PageInfoUiDelegate {
 public:
  ChromePageInfoUiDelegate(Profile* profile, const GURL& site_url);
  ~ChromePageInfoUiDelegate() override = default;

  // Whether the combobox option for allowing a permission should be shown for
  // `type`.
  bool ShouldShowAllow(ContentSettingsType type);

  // Whether the combobox option to ask a permission should be shown for `type`.
  bool ShouldShowAsk(ContentSettingsType type);

#if !defined(OS_ANDROID)
  // Whether to show a link that takes the user to the chrome://settings subpage
  // for `site_url_`.
  bool ShouldShowSiteSettings();

  // The returned string, if non-empty, should be added as a sublabel that gives
  // extra details to the user concerning the granted permission.
  std::u16string GetPermissionDetail(ContentSettingsType type);

  // PageInfoUiDelegate implementation
  bool IsBlockAutoPlayEnabled() override;
#endif  // !defined(OS_ANDROID)
  permissions::PermissionResult GetPermissionStatus(
      ContentSettingsType type) override;

 private:
  Profile* profile_;
  GURL site_url_;
};

#endif  // CHROME_BROWSER_UI_PAGE_INFO_CHROME_PAGE_INFO_UI_DELEGATE_H_
