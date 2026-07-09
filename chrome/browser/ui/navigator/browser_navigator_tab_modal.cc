// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/navigator/browser_navigator_tab_modal.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/navigator/browser_navigator_params.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/geometry/rect.h"

struct NavigateParams;

namespace internal {
base::WeakPtr<content::NavigationHandle> ShowTabModalPopup(
    NavigateParams& params);
}

// static
base::WeakPtr<content::NavigationHandle> BrowserNavigatorTabModal::ShowImpl(
    const GURL& url,
    content::WebContents& source_contents,
    const gfx::Size& size) {
  NavigateParams params(
      Profile::FromBrowserContext(source_contents.GetBrowserContext()), url,
      ui::PAGE_TRANSITION_LINK);
  params.source_contents = &source_contents;
  params.window_features.bounds = gfx::Rect(size);
  params.disposition = WindowOpenDisposition::NEW_POPUP;
  params.window_action = NavigateParams::WindowAction::kShowWindow;
  return internal::ShowTabModalPopup(params);
}
