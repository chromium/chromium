// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BLOCKED_CONTENT_POPUP_BLOCKER_H_
#define CHROME_BROWSER_UI_BLOCKED_CONTENT_POPUP_BLOCKER_H_

#include "base/optional.h"
#include "third_party/blink/public/web/window_features.mojom.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

namespace content {
class WebContents;
struct OpenURLParams;
}  // namespace content

struct NavigateParams;

// Classifies what caused a popup to be blocked.
enum class PopupBlockType {
  // Popup blocked due to no user gesture.
  kNoGesture,
  // Popup blocked due to the abusive popup blocker.
  kAbusive,
};

// Whether a new window opened with |disposition| would be considered for
// popup blocking. Note that this includes more dispositions than just
// NEW_POPUP since the popup blocker targets all new windows and tabs.
bool ConsiderForPopupBlocking(WindowOpenDisposition disposition);

// Returns true if the popup request defined by |params| and the optional
// |open_url_params| should be blocked. In that case, it is also added to the
// |blocked_popups_| container.
//
// |opener_url| is an optional parameter used to compute how the popup
// permission will behave. If it is not set the current committed URL will be
// used instead.
//
// If this function returns true, then the contents of |params| is moved to
// |blocked_popups_|.
bool MaybeBlockPopup(content::WebContents* web_contents,
                     const base::Optional<GURL>& opener_url,
                     NavigateParams* params,
                     const content::OpenURLParams* open_url_params,
                     const blink::mojom::WindowFeatures& window_features);

#endif  // CHROME_BROWSER_UI_BLOCKED_CONTENT_POPUP_BLOCKER_H_
