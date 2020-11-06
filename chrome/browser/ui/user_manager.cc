// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/user_manager.h"

#include "base/bind.h"
#include "components/guest_view/browser/guest_view_manager.h"

namespace {

bool AddToSet(std::set<content::WebContents*>* content_set,
              content::WebContents* web_contents) {
  content_set->insert(web_contents);
  return false;
}

}  // namespace

UserManagerProfileDialog::BaseDialogDelegate::BaseDialogDelegate()
    : guest_web_contents_(nullptr) {}

bool UserManagerProfileDialog::BaseDialogDelegate::HandleContextMenu(
    content::RenderFrameHost* render_frame_host,
    const content::ContextMenuParams& params) {
  // Ignores context menu.
  return true;
}

void UserManagerProfileDialog::BaseDialogDelegate::LoadingStateChanged(
    content::WebContents* source,
    bool to_different_document) {
  if (source->IsLoading() || guest_web_contents_)
    return;

  // Try to find the embedded WebView and manage its WebContents. The WebView
  // may not be found in the initial page load since it loads asynchronously.
  std::set<content::WebContents*> content_set;
  guest_view::GuestViewManager* manager =
      guest_view::GuestViewManager::FromBrowserContext(
          source->GetBrowserContext());
  if (manager)
    manager->ForEachGuest(source, base::BindRepeating(&AddToSet, &content_set));
  DCHECK_LE(content_set.size(), 1U);
  if (!content_set.empty()) {
    guest_web_contents_ = *content_set.begin();
    guest_web_contents_->SetDelegate(this);
  }
}
