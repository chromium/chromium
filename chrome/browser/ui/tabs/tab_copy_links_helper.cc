// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_copy_links_helper.h"

#include <stddef.h>

#include <string>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/toasts/api/toast_id.h"
#include "chrome/browser/ui/toasts/toast_controller.h"
#include "chrome/browser/ui/toasts/toast_features.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/clipboard/clipboard_buffer.h"
#include "ui/base/clipboard/clipboard_url_info.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "url/gurl.h"

namespace tabs {

namespace {

std::vector<ui::ClipboardUrlInfo> FilterURLsForCopy(
    base::span<content::WebContents* const> web_contents_list) {
  std::vector<ui::ClipboardUrlInfo> urls;
  urls.reserve(web_contents_list.size());
  for (content::WebContents* const contents : web_contents_list) {
    GURL url = GetURLForTabCopy(contents);
    if (!url.is_empty()) {
      urls.push_back({.url = std::move(url), .title = contents->GetTitle()});
    }
  }
  return urls;
}

}  // namespace

GURL GetURLForTabCopy(content::WebContents* contents) {
  if (!contents) {
    return GURL();
  }
  GURL url = contents->GetLastCommittedURL();
  return url.is_valid() && url.SchemeIsHTTPOrHTTPS() ? url : GURL();
}

size_t CopyLinks(BrowserWindowInterface* browser,
                 base::span<content::WebContents* const> web_contents_list) {
  std::vector<ui::ClipboardUrlInfo> links =
      FilterURLsForCopy(web_contents_list);
  if (links.empty()) {
    return 0;
  }

  {
    ui::ScopedClipboardWriter scw(ui::ClipboardBuffer::kCopyPaste);
    if (browser && browser->GetProfile()->IsOffTheRecord()) {
      scw.MarkAsOffTheRecord();
    }
    scw.WriteURLs(links);
    // Multi-link plain text is provided by each platform's
    // `Clipboard::WriteURLs()` implementation.
    if (links.size() == 1) {
      scw.WriteText(base::UTF8ToUTF16(links[0].url.spec()));
    }
  }

  if (browser && toast_features::IsEnabled(toast_features::kLinkCopiedToast)) {
    if (ToastController* const toast_controller =
            ToastController::From(browser)) {
      ToastParams params(ToastId::kLinkCopied);
      params.body_string_cardinality_param =
          base::saturated_cast<int>(links.size());
      toast_controller->MaybeShowToast(std::move(params));
    }
  }

  return links.size();
}

}  // namespace tabs
