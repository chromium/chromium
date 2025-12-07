// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/experiences/clipboard/clipboard_history_controller_delegate_impl.h"

#include <set>

#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_iterator.h"
#include "content/public/browser/web_contents.h"
#include "ui/aura/window.h"

namespace {

// Helpers ---------------------------------------------------------------------

// Returns the set of all `content::WebContents` instances. Copied from
// `content::WebContentsImpl::GetAllWebContents()` due to dependency
// restrictions.
std::set<content::WebContents*> GetAllWebContents() {
  std::set<content::WebContents*> result;

  const auto it = content::RenderWidgetHost::GetRenderWidgetHosts();
  while (content::RenderWidgetHost* const rwh = it->GetNextHost()) {
    auto* const rvh = content::RenderViewHost::From(rwh);
    if (!rvh) {
      continue;
    }
    auto* const web_contents = content::WebContents::FromRenderViewHost(rvh);
    if (!web_contents) {
      continue;
    }
    if (web_contents->GetPrimaryMainFrame()->GetRenderViewHost() == rvh) {
      result.emplace(web_contents);
    }
  }

  return result;
}

// Returns the currently focused `content::WebContents` for the specified
// `web_contents`. May return `web_contents`, an embedded `content::WebContents`
// within `web_contents`, or `nullptr`.
content::WebContents* GetFocusedWebContents(
    content::WebContents* web_contents) {
  auto* const focused_frame = web_contents->GetFocusedFrame();
  return focused_frame
             ? content::WebContents::FromRenderFrameHost(focused_frame)
             : nullptr;
}

}  // namespace

// ClipboardHistoryControllerDelegateImpl --------------------------------------

ClipboardHistoryControllerDelegateImpl::
    ClipboardHistoryControllerDelegateImpl() = default;

ClipboardHistoryControllerDelegateImpl::
    ~ClipboardHistoryControllerDelegateImpl() = default;

bool ClipboardHistoryControllerDelegateImpl::Paste() const {
  for (auto* const web_contents : GetAllWebContents()) {
    auto* const focused_web_contents = GetFocusedWebContents(web_contents);
    if (!focused_web_contents) {
      continue;
    }
    if (const auto* const window = focused_web_contents->GetContentNativeView();
        window && window->HasFocus()) {
      focused_web_contents->Paste();
      return true;
    }
  }
  return false;
}
