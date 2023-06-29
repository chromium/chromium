// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/supervised_user/supervised_user_error_page_controller_delegate_impl.h"

#include "chrome/renderer/supervised_user/supervised_user_error_page_controller.h"
#include "content/public/renderer/render_frame.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"

SupervisedUserErrorPageControllerDelegateImpl::
    SupervisedUserErrorPageControllerDelegateImpl(
        content::RenderFrame* render_frame)
    : content::RenderFrameObserver(render_frame),
      content::RenderFrameObserverTracker<
          SupervisedUserErrorPageControllerDelegateImpl>(render_frame) {}

SupervisedUserErrorPageControllerDelegateImpl::
    ~SupervisedUserErrorPageControllerDelegateImpl() = default;

void SupervisedUserErrorPageControllerDelegateImpl::PrepareForErrorPage() {
  pending_error_ = true;
}

void SupervisedUserErrorPageControllerDelegateImpl::GoBack() {
  if (supervised_user_interface_)
    supervised_user_interface_->GoBack();
}

void SupervisedUserErrorPageControllerDelegateImpl::RequestUrlAccessRemote(
    UrlAccessRequestInitiated callback) {
  if (supervised_user_interface_)
    supervised_user_interface_->RequestUrlAccessRemote(std::move(callback));
}

void SupervisedUserErrorPageControllerDelegateImpl::RequestUrlAccessLocal(
    UrlAccessRequestInitiated callback) {
  if (supervised_user_interface_)
    supervised_user_interface_->RequestUrlAccessLocal(std::move(callback));
}

void SupervisedUserErrorPageControllerDelegateImpl::OnDestruct() {
  delete this;
}

void SupervisedUserErrorPageControllerDelegateImpl::DidFinishLoad() {
  if (committed_error_) {
    if (!supervised_user_interface_) {
      render_frame()->GetRemoteAssociatedInterfaces()->GetInterface(
          &supervised_user_interface_);
    }

    SupervisedUserErrorPageController::Install(
        render_frame(),
        weak_supervised_user_error_controller_delegate_factory_.GetWeakPtr());
  }
}

void SupervisedUserErrorPageControllerDelegateImpl::ReadyToCommitNavigation(
    blink::WebDocumentLoader* document_loader) {
  // We are about to commit a new navigation in this render frame.
  // Invalidate the weak pointer in previous error page controller, i.e.
  // |SupervisedUserErrorPageController::delegate_|;
  weak_supervised_user_error_controller_delegate_factory_.InvalidateWeakPtrs();
  committed_error_ = pending_error_;
  pending_error_ = false;
}
