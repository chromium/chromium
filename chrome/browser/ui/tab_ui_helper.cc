// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tab_ui_helper.h"

#include "base/functional/bind.h"
#include "build/build_config.h"
#include "chrome/browser/favicon/favicon_utils.h"
#include "chrome/browser/sessions/session_restore.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/resources/grit/ui_resources.h"

TabUIHelper::TabUIHelper(content::WebContents* contents)
    : WebContentsObserver(contents),
      content::WebContentsUserData<TabUIHelper>(*contents) {}

TabUIHelper::~TabUIHelper() = default;

std::u16string TabUIHelper::GetTitle() const {
  const std::u16string& contents_title = web_contents()->GetTitle();
  if (!contents_title.empty())
    return contents_title;

#if BUILDFLAG(IS_MAC)
  return l10n_util::GetStringUTF16(IDS_BROWSER_WINDOW_MAC_TAB_UNTITLED);
#else
  return std::u16string();
#endif
}

ui::ImageModel TabUIHelper::GetFavicon() const {
  return ui::ImageModel::FromImage(
      favicon::TabFaviconFromWebContents(web_contents()));
}

bool TabUIHelper::ShouldHideThrobber() const {
  // We want to hide a background tab's throbber during page load if it is
  // created by session restore. A restored tab's favicon is already fetched
  // by |SessionRestoreDelegate|.
  if (created_by_session_restore_ && !was_active_at_least_once_)
    return true;

  return false;
}

void TabUIHelper::DidStopLoading() {
  // Reset the properties after the initial navigation finishes loading, so that
  // latter navigations are not affected. Note that the prerendered page won't
  // reset the properties because DidStopLoading is not called for prerendering.
  created_by_session_restore_ = false;
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(TabUIHelper);
