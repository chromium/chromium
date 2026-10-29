// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_POPUP_WEBUI_CREATION_H_
#define CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_POPUP_WEBUI_CREATION_H_

#include "base/auto_reset.h"

namespace omnibox {

// Whether the classic and AI Mode WebUI omnibox popups create their WebUI
// content only when it is first needed (on show, or when the classic popup's
// context menu opens), instead of eagerly or after the browser window's first
// frame. Browser tests turn this on (see `InProcessBrowserTest`), since most
// never show the popups. The first show then loads the page, so the popup may
// show before the page has painted, which production only does if the popup
// is shown before its deferred creation.
// TODO(crbug.com/542347163): Make the full WebUI omnibox popup honor this too.
bool ShouldCreatePopupWebUIOnDemand();

// Sets the value while the returned object is alive.
[[nodiscard]] base::AutoReset<bool> SetCreatePopupWebUIOnDemandForTesting(
    bool on_demand);

}  // namespace omnibox

#endif  // CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_POPUP_WEBUI_CREATION_H_
