// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/side_search/side_search_tab_contents_helper.h"

#include "chrome/browser/ui/side_search/side_search_side_contents_helper.h"
#include "components/google/core/common/google_util.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_handle.h"

SideSearchTabContentsHelper::~SideSearchTabContentsHelper() = default;

void SideSearchTabContentsHelper::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  const auto& url = navigation_handle->GetURL();

  if (!google_util::IsGoogleSearchUrl(url))
    return;

  last_search_url_ = url;
}

content::WebContents* SideSearchTabContentsHelper::GetSidePanelContents() {
  if (!side_panel_contents_)
    CreateSidePanelContents();

  DCHECK(side_panel_contents_);
  return side_panel_contents_.get();
}

SideSearchTabContentsHelper::SideSearchTabContentsHelper(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {}

SideSearchSideContentsHelper*
SideSearchTabContentsHelper::GetSideContentsHelper() {
  DCHECK(side_panel_contents_);
  return SideSearchSideContentsHelper::FromWebContents(
      side_panel_contents_.get());
}

void SideSearchTabContentsHelper::CreateSidePanelContents() {
  DCHECK(!side_panel_contents_);
  side_panel_contents_ =
      content::WebContents::Create(content::WebContents::CreateParams(
          web_contents()->GetBrowserContext(), nullptr));
  SideSearchSideContentsHelper::CreateForWebContents(
      side_panel_contents_.get());
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(SideSearchTabContentsHelper)
