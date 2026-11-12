// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_COPY_LINKS_HELPER_H_
#define CHROME_BROWSER_UI_TABS_TAB_COPY_LINKS_HELPER_H_

#include <stddef.h>

#include "base/containers/span.h"
#include "url/gurl.h"

class BrowserWindowInterface;

namespace content {
class WebContents;
}  // namespace content

namespace tabs {

// Returns `contents->GetLastCommittedURL()` if it is a valid HTTP or HTTPS URL,
// or an empty `GURL` otherwise.
GURL GetURLForTabCopy(content::WebContents* contents);

// Copies HTTP/HTTPS tab links (URL and title) in `web_contents_list` to the
// clipboard via `ScopedClipboardWriter::WriteURLs()`. A single link is also
// written as plain text. Shows `ToastId::kLinkCopied` on `browser` when
// `kLinkCopiedToast` is enabled. Returns the number of copied links.
size_t CopyLinks(BrowserWindowInterface* browser,
                 base::span<content::WebContents* const> web_contents_list);

}  // namespace tabs

#endif  // CHROME_BROWSER_UI_TABS_TAB_COPY_LINKS_HELPER_H_
