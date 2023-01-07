// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/web_modal/web_contents_modal_dialog_manager.h"

#include <utility>

#include "base/check.h"
#include "base/ranges/algorithm.h"
#include "components/back_forward_cache/back_forward_cache_disable.h"
#include "components/web_modal/web_contents_modal_dialog_manager_delegate.h"
#include "content/public/browser/back_forward_cache.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"

using content::WebContents;

namespace web_modal {

WebContentsModalDialogManager::~WebContentsModalDialogManager() {
  DCHECK(child_dialogs_.empty());
}

void WebContentsModalDialogManager::SetDelegate(
    WebContentsModalDialogManagerDelegate* d) {
  delegate_ = d;

  for (const auto& dialog : child_dialogs_) {
    // Delegate can be null on Views/Win32 during tab drag.
    dialog.manager->HostChanged(d ? d->GetWebContentsModalDialogHost()
                                  : nullptr);
  }
}

// TODO(gbillock): Maybe "ShowBubbleWithManager"?
void WebContentsModalDialogManager::ShowDialogWithManager(
    gfx::NativeWindow dialog,
    std::unique_ptr<SingleWebContentsDialogManager> manager) {
  if (delegate_)
    manager->HostChanged(delegate_->GetWebContentsModalDialogHost());
  child_dialogs_.emplace_back(dialog, std::move(manager));

  if (child_dialogs_.size() == 1) {
    BlockWebContentsInteraction(true);
    if (delegate_ && delegate_->IsWebContentsVisible(web_contents()))
      child_dialogs_.back().manager->Show();
  }
}

bool WebContentsModalDialogManager::IsDialogActive() const {
  return !child_dialogs_.empty();
}

void WebContentsModalDialogManager::FocusTopmostDialog() const {
  DCHECK(!child_dialogs_.empty());
  child_dialogs_.front().manager->Focus();
}

content::WebContents* WebContentsModalDialogManager::GetWebContents() const {
  return web_contents();
}

void WebContentsModalDialogManager::WillClose(gfx::NativeWindow dialog) {
  auto dlg = base::ranges::find(child_dialogs_, dialog, &DialogState::dialog);

  // The Views tab contents modal dialog calls WillClose twice.  Ignore the
  // second invocation.
  if (dlg == child_dialogs_.end())
    return;

  bool removed_topmost_dialog = dlg == child_dialogs_.begin();
  child_dialogs_.erase(dlg);
  if (!closing_all_dialogs_ &&
      (!child_dialogs_.empty() && removed_topmost_dialog) &&
      (delegate_ && delegate_->IsWebContentsVisible(web_contents()))) {
    child_dialogs_.front().manager->Show();
  }

  BlockWebContentsInteraction(!child_dialogs_.empty());
}

WebContentsModalDialogManager::WebContentsModalDialogManager(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<WebContentsModalDialogManager>(
          *web_contents),
      web_contents_is_hidden_(web_contents->GetVisibility() ==
                              content::Visibility::HIDDEN) {}

WebContentsModalDialogManager::DialogState::DialogState(
    gfx::NativeWindow dialog,
    std::unique_ptr<SingleWebContentsDialogManager> mgr)
    : dialog(dialog), manager(std::move(mgr)) {}

WebContentsModalDialogManager::DialogState::DialogState(DialogState&& state) =
    default;

WebContentsModalDialogManager::DialogState::~DialogState() = default;

// TODO(gbillock): Move this to Views impl within Show()? It would
// call WebContents* contents = native_delegate_->GetWebContents(); and
// then set the block state. Advantage: could restrict some of the
// WCMDM delegate methods, then, and pass them behind the scenes.
void WebContentsModalDialogManager::BlockWebContentsInteraction(bool blocked) {
  WebContents* contents = web_contents();
  if (!contents) {
    // The WebContents has already disconnected.
    return;
  }

  contents->SetIgnoreInputEvents(blocked);
  if (delegate_)
    delegate_->SetWebContentsBlocked(contents, blocked);
}

void WebContentsModalDialogManager::CloseAllDialogs() {
  closing_all_dialogs_ = true;

  // Clear out any dialogs since we are leaving this page entirely.
  while (!child_dialogs_.empty()) {
    child_dialogs_.front().manager->Close();
  }

  closing_all_dialogs_ = false;
}

void WebContentsModalDialogManager::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInPrimaryMainFrame() ||
      !navigation_handle->HasCommitted())
    return;

  if (!child_dialogs_.empty()) {
    // Disable BFCache for the page which had any modal dialog open.
    // This prevents the page which has print, confirm form resubmission, http
    // password dialogs, etc. to go in to BFCache. We can't simply dismiss the
    // dialogs in the case, since they are requesting meaningful input from the
    // user that affects the loading or display of the content.
    content::BackForwardCache::DisableForRenderFrameHost(
        navigation_handle->GetPreviousRenderFrameHostId(),
        back_forward_cache::DisabledReason(
            back_forward_cache::DisabledReasonId::kModalDialog));
  }

  // Close constrained windows if necessary.
  if (!net::registry_controlled_domains::SameDomainOrHost(
          navigation_handle->GetPreviousPrimaryMainFrameURL(),
          navigation_handle->GetURL(),
          net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES))
    CloseAllDialogs();
}

void WebContentsModalDialogManager::DidGetIgnoredUIEvent() {
  if (!child_dialogs_.empty()) {
    child_dialogs_.front().manager->Focus();
  }
}

void WebContentsModalDialogManager::OnVisibilityChanged(
    content::Visibility visibility) {
  const bool web_contents_was_hidden = web_contents_is_hidden_;
  web_contents_is_hidden_ = visibility == content::Visibility::HIDDEN;

  // Avoid reshowing on transitions between VISIBLE and OCCLUDED.
  if (child_dialogs_.empty() ||
      web_contents_is_hidden_ == web_contents_was_hidden) {
    return;
  }

  if (web_contents_is_hidden_)
    child_dialogs_.front().manager->Hide();
  else
    child_dialogs_.front().manager->Show();
}

void WebContentsModalDialogManager::WebContentsDestroyed() {
  // First cleanly close all child dialogs.
  // TODO(mpcomplete): handle case if MaybeCloseChildWindows() already asked
  // some of these to close.  CloseAllDialogs is async, so it might get called
  // twice before it runs.
  CloseAllDialogs();
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(WebContentsModalDialogManager);

}  // namespace web_modal
