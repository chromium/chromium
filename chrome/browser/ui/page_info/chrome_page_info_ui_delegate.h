// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PAGE_INFO_CHROME_PAGE_INFO_UI_DELEGATE_H_
#define CHROME_BROWSER_UI_PAGE_INFO_CHROME_PAGE_INFO_UI_DELEGATE_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "components/page_info/page_info_ui_delegate.h"
#include "url/gurl.h"

#if !BUILDFLAG(IS_ANDROID)
#include "components/page_info/core/proto/about_this_site_metadata.pb.h"
#endif

class Profile;

namespace content {
class WebContents;
}

namespace ui {
class Event;
}

class ChromePageInfoUiDelegate : public PageInfoUiDelegate {
 public:
  ChromePageInfoUiDelegate(content::WebContents* web_contents,
                           const GURL& site_url);
  ~ChromePageInfoUiDelegate() override = default;

  // Whether the combobox option for allowing a permission should be shown for
  // `type`.
  bool ShouldShowAllow(ContentSettingsType type);

  // Whether the combobox option to ask a permission should be shown for `type`.
  bool ShouldShowAsk(ContentSettingsType type);

  // If "allow" option is not available, return the reason why.
  std::u16string GetAutomaticallyBlockedReason(ContentSettingsType type);

#if !BUILDFLAG(IS_ANDROID)
  // Returns "About this site" info for the active page.
  std::optional<page_info::proto::SiteInfo> GetAboutThisSiteInfo();

  // Handles opening the "More about this page" URL in a new tab.
  void OpenMoreAboutThisPageUrl(const GURL& url, const ui::Event& event);

  // If PageInfo should show a link to the site or app's settings page, this
  // will return true and set the params to the appropriate resource IDs (IDS_*).
  // Otherwise, it will return false.
  bool ShouldShowSiteSettings(int* link_text_id, int* tooltip_text_id);

  // The returned string, if non-empty, should be added as a sublabel that gives
  // extra details to the user concerning the granted permission.
  std::u16string GetPermissionDetail(ContentSettingsType type);

  // Similar to GetPermissionDetail, if this returns true a sublabel should be
  // added to give extra details to the user concerning the granted permission,
  // with the difference being that this label includes an embedded link.
  // `text_id` should be set to the resource ID for the overall
  // label, with `link_id` being inserted into a place holder in the
  // description as a clickable link. When the link is clicked,
  // SettingsLinkClicked() is called.
  bool ShouldShowSettingsLinkForPermission(ContentSettingsType type,
                                           int* text_id,
                                           int* link_id);

  // Called when the link specified by ShouldShowSettingsLinkForPermission() is
  // clicked.
  void SettingsLinkClicked(ContentSettingsType type);

  // Opens Privacy Sandbox settings page.
  void ShowPrivacySandboxSettings();

  // PageInfoUiDelegate implementation
  bool IsBlockAutoPlayEnabled() override;
  bool IsMultipleTabsOpen() override;
  void OpenSiteSettingsFileSystem() override;
#endif  // !BUILDFLAG(IS_ANDROID)
  content::PermissionResult GetPermissionResult(
      blink::PermissionType permission) override;
  std::optional<content::PermissionResult> GetEmbargoResult(
      ContentSettingsType type) override;

  bool IsTrackingProtection3pcdEnabled() override;

 private:
  Profile* GetProfile() const;

  raw_ptr<content::WebContents, AcrossTasksDanglingUntriaged> web_contents_;
  GURL site_url_;
};

#endif  // CHROME_BROWSER_UI_PAGE_INFO_CHROME_PAGE_INFO_UI_DELEGATE_H_
