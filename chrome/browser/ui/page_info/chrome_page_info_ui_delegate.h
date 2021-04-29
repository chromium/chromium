// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PAGE_INFO_CHROME_PAGE_INFO_UI_DELEGATE_H_
#define CHROME_BROWSER_UI_PAGE_INFO_CHROME_PAGE_INFO_UI_DELEGATE_H_

#include "build/build_config.h"
#include "components/page_info/page_info_ui_delegate.h"

class Profile;

class ChromePageInfoUiDelegate : public PageInfoUiDelegate {
 public:
  ChromePageInfoUiDelegate(Profile* profile, const GURL& site_url);
  ~ChromePageInfoUiDelegate() override = default;

  // PageInfoUiDelegate implementation
#if !defined(OS_ANDROID)
  bool IsBlockAutoPlayEnabled() override;
#endif
  permissions::PermissionResult GetPermissionStatus(
      ContentSettingsType type) override;
  bool ShouldShowAllow(ContentSettingsType type) override;
  bool ShouldShowAsk(ContentSettingsType type) override;

 private:
  Profile* profile_;
  GURL site_url_;
};

#endif  // CHROME_BROWSER_UI_PAGE_INFO_CHROME_PAGE_INFO_UI_DELEGATE_H_
