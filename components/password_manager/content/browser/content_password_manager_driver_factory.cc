// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/content/browser/content_password_manager_driver_factory.h"

#include <utility>

#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/content/browser/content_autofill_driver_factory.h"
#include "components/password_manager/content/browser/content_password_manager_driver.h"
#include "components/password_manager/content/browser/form_submission_tracker_util.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/common/features.h"

namespace password_manager {

ContentPasswordManagerDriverFactory::ContentPasswordManagerDriverFactory(
    content::WebContents* web_contents,
    PasswordManagerClient* password_client)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<ContentPasswordManagerDriverFactory>(
          *web_contents),
      password_client_(password_client) {}

ContentPasswordManagerDriverFactory::~ContentPasswordManagerDriverFactory() =
    default;

// static
void ContentPasswordManagerDriverFactory::BindPasswordManagerDriver(
    mojo::PendingAssociatedReceiver<autofill::mojom::PasswordManagerDriver>
        pending_receiver,
    content::RenderFrameHost* render_frame_host) {
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host);
  // We try to bind to the driver of this RenderFrameHost,
  // but if driver is not ready for this RenderFrameHost for now,
  // the request will be just dropped, this would cause closing the message pipe
  // which would raise connection error to peer side.
  // Peer side could reconnect later when needed.
  // TODO(crbug.com/40815551): WebContents should never be null here; the
  // helper function above only returns a null WebContents if
  // `render_frame_host` is null, but that should never be the case here.
  if (!web_contents)
    return;

  // This is called by a Mojo registry for associated interfaces, which should
  // never attempt to bind interfaces for RenderFrameHosts with non-live
  // RenderFrames.
  CHECK(render_frame_host->IsRenderFrameLive());

  ContentPasswordManagerDriverFactory* factory =
      ContentPasswordManagerDriverFactory::FromWebContents(web_contents);
  if (!factory)
    return;

  factory->GetDriverForFrame(render_frame_host)
      ->BindPendingReceiver(std::move(pending_receiver));
}

ContentPasswordManagerDriver*
ContentPasswordManagerDriverFactory::GetDriverForFrame(
    content::RenderFrameHost* render_frame_host) {
  DCHECK_EQ(web_contents(),
            content::WebContents::FromRenderFrameHost(render_frame_host));

  // A RenderFrameHost without a live RenderFrame will never call
  // RenderFrameDeleted(), and the corresponding driver would never be cleaned
  // up.
  if (!render_frame_host->IsRenderFrameLive())
    return nullptr;

  if (render_frame_host->IsNestedWithinFencedFrame() &&
      !base::FeatureList::IsEnabled(blink::features::kFencedFramesAPIChanges)) {
    return nullptr;
  }

  // try_emplace() will return an iterator to the driver corresponding to
  // `render_frame_host`, creating a new one if `render_frame_host` is not
  // already a key in the map.
  auto [it, inserted] = frame_driver_map_.try_emplace(
      render_frame_host,
      // Args passed to the ContentPasswordManagerDriver
      // constructor if none exists for `render_frame_host`
      // yet.
      render_frame_host, password_client_);
  return &it->second;
}

void ContentPasswordManagerDriverFactory::DidFinishNavigation(
    content::NavigationHandle* navigation) {
  if (navigation->IsSameDocument() || !navigation->HasCommitted()) {
    return;
  }

  if (navigation->IsInPrimaryMainFrame()) {
    // Clear page specific data after main frame navigation.
    NotifyDidNavigateMainFrame(navigation->IsRendererInitiated(),
                               navigation->GetPageTransition(),
                               navigation->WasInitiatedByLinkClick(),
                               password_client_->GetPasswordManager());
  }

  ContentPasswordManagerDriver* driver =
      GetDriverForFrame(navigation->GetRenderFrameHost());
  if (!driver) {
    return;
  }
  // A cross-document navigation may commit in the same RenderFrameHost, and
  // hence with the same driver, as the previous document (e.g. the first
  // navigation away from the initial empty document). The previous document's
  // state must not be reused for the new document. BFCache restores and
  // prerendered page activations don't create a new document but reactivate an
  // existing one together with its driver, so the driver's state is kept.
  if (!navigation->IsServedFromBackForwardCache() &&
      !navigation->IsPrerenderedPageActivation()) {
    driver->ResetForNewDocument();
  }
  driver->DidNavigate();
}

void ContentPasswordManagerDriverFactory::RenderFrameDeleted(
    content::RenderFrameHost* render_frame_host) {
  frame_driver_map_.erase(render_frame_host);
}

void ContentPasswordManagerDriverFactory::WebContentsDestroyed() {
  web_contents()->RemoveUserData(UserDataKey());
  // Do not add code - `this` is now destroyed.
}

void ContentPasswordManagerDriverFactory::RequestSendLoggingAvailability() {
  for (auto& frame_and_driver : frame_driver_map_)
    frame_and_driver.second.SendLoggingAvailability();
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(ContentPasswordManagerDriverFactory);

}  // namespace password_manager
