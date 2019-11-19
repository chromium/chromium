// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_SUPERVISED_USER_SUPERVISED_USER_ERROR_PAGE_CONTROLLER_DELEGATE_IMPL_H_
#define CHROME_RENDERER_SUPERVISED_USER_SUPERVISED_USER_ERROR_PAGE_CONTROLLER_DELEGATE_IMPL_H_

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/common/supervised_user_commands.mojom.h"
#include "chrome/renderer/supervised_user/supervised_user_error_page_controller_delegate.h"
#include "content/public/renderer/render_frame_observer.h"
#include "content/public/renderer/render_frame_observer_tracker.h"
#include "mojo/public/cpp/bindings/associated_remote.h"

namespace content {
class RenderFrame;
}  // namespace content

class SupervisedUserErrorPageControllerDelegateImpl
    : public content::RenderFrameObserver,
      public content::RenderFrameObserverTracker<
          SupervisedUserErrorPageControllerDelegateImpl>,
      public SupervisedUserErrorPageControllerDelegate {
 public:
  explicit SupervisedUserErrorPageControllerDelegateImpl(
      content::RenderFrame* render_frame);
  ~SupervisedUserErrorPageControllerDelegateImpl() override;

  // Notifies us that a navigation error has occurred and will be committed.
  void PrepareForErrorPage();

  // SupervisedUserErrorPageControllerDelegate:
  void GoBack() override;
  void RequestPermission(base::OnceCallback<void(bool)> callback) override;
  void Feedback() override;

  // content::RenderFrameObserver:
  void OnDestruct() override;
  void DidFinishLoad() override;
  void ReadyToCommitNavigation(
      blink::WebDocumentLoader* document_loader) override;

 private:
  mojo::AssociatedRemote<supervised_user::mojom::SupervisedUserCommands>
      supervised_user_interface_;

  // Whether there is an error page pending to be committed.
  bool pending_error_ = false;

  // Whether the committed page is an error page.
  bool committed_error_ = false;

  base::WeakPtrFactory<SupervisedUserErrorPageControllerDelegate>
      weak_supervised_user_error_controller_delegate_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(SupervisedUserErrorPageControllerDelegateImpl);
};

#endif  // CHROME_RENDERER_SUPERVISED_USER_SUPERVISED_USER_ERROR_PAGE_CONTROLLER_DELEGATE_IMPL_H_
