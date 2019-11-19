// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_capture/browser/content_capture_receiver_manager.h"

#include <utility>

#include "components/content_capture/browser/content_capture_receiver.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"

namespace content_capture {
namespace {

const void* const kUserDataKey = &kUserDataKey;

}  // namespace

ContentCaptureReceiverManager::ContentCaptureReceiverManager(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {
  const std::vector<content::RenderFrameHost*> frames =
      web_contents->GetAllFrames();
  for (content::RenderFrameHost* frame : frames)
    RenderFrameCreated(frame);

  web_contents->SetUserData(
      kUserDataKey, std::unique_ptr<ContentCaptureReceiverManager>(this));
}

ContentCaptureReceiverManager::~ContentCaptureReceiverManager() = default;

// static
ContentCaptureReceiverManager* ContentCaptureReceiverManager::FromWebContents(
    content::WebContents* contents) {
  return static_cast<ContentCaptureReceiverManager*>(
      contents->GetUserData(kUserDataKey));
}

// static
void ContentCaptureReceiverManager::BindContentCaptureReceiver(
    mojo::PendingAssociatedReceiver<mojom::ContentCaptureReceiver>
        pending_receiver,
    content::RenderFrameHost* render_frame_host) {
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host);

  if (!web_contents)
    return;

  ContentCaptureReceiverManager* manager =
      ContentCaptureReceiverManager::FromWebContents(web_contents);
  if (!manager)
    return;

  auto* receiver = manager->ContentCaptureReceiverForFrame(render_frame_host);
  if (receiver)
    receiver->BindPendingReceiver(std::move(pending_receiver));
}

ContentCaptureReceiver*
ContentCaptureReceiverManager::ContentCaptureReceiverForFrame(
    content::RenderFrameHost* render_frame_host) const {
  auto mapping = frame_map_.find(render_frame_host);
  return mapping == frame_map_.end() ? nullptr : mapping->second.get();
}

void ContentCaptureReceiverManager::RenderFrameCreated(
    content::RenderFrameHost* render_frame_host) {
  // The frame might not have content, but it could be parent of other frame. we
  // always create the ContentCaptureReceiver for ContentCaptureSession
  // purpose.
  if (ContentCaptureReceiverForFrame(render_frame_host))
    return;
  frame_map_.insert(std::make_pair(
      render_frame_host,
      std::make_unique<ContentCaptureReceiver>(render_frame_host)));
}

void ContentCaptureReceiverManager::RenderFrameDeleted(
    content::RenderFrameHost* render_frame_host) {
  frame_map_.erase(render_frame_host);
}

void ContentCaptureReceiverManager::ReadyToCommitNavigation(
    content::NavigationHandle* navigation_handle) {
  auto* receiver =
      ContentCaptureReceiverForFrame(navigation_handle->GetRenderFrameHost());
  if (web_contents()->GetBrowserContext()->IsOffTheRecord() ||
      !ShouldCapture(navigation_handle->GetURL())) {
    receiver->StopCapture();
    return;
  }
  receiver->StartCapture();
}

void ContentCaptureReceiverManager::DidCaptureContent(
    ContentCaptureReceiver* content_capture_receiver,
    const ContentCaptureData& data) {
  // The root of |data| is frame, we need get its ancestor only.
  ContentCaptureSession parent_session;
  BuildContentCaptureSession(content_capture_receiver, true /* ancestor_only */,
                             &parent_session);
  DidCaptureContent(parent_session, data);
}

void ContentCaptureReceiverManager::DidUpdateContent(
    ContentCaptureReceiver* content_capture_receiver,
    const ContentCaptureData& data) {
  ContentCaptureSession parent_session;
  BuildContentCaptureSession(content_capture_receiver, true /* ancestor_only */,
                             &parent_session);
  DidUpdateContent(parent_session, data);
}

void ContentCaptureReceiverManager::DidRemoveContent(
    ContentCaptureReceiver* content_capture_receiver,
    const std::vector<int64_t>& data) {
  ContentCaptureSession session;
  // The |data| is a list of text content id, the session should include
  // |content_capture_receiver| associated frame.
  BuildContentCaptureSession(content_capture_receiver,
                             false /* ancestor_only */, &session);
  DidRemoveContent(session, data);
}

void ContentCaptureReceiverManager::DidRemoveSession(
    ContentCaptureReceiver* content_capture_receiver) {
  ContentCaptureSession session;
  // The session should include the removed frame that the
  // |content_capture_receiver| associated with.
  // We want the last reported content capture session, instead of the current
  // one for the scenario like below:
  // Main frame navigates to different url which has the same origin of previous
  // one, it triggers the previous child frame being removed but the main RFH
  // unchanged, if we use BuildContentCaptureSession() which always use the
  // current URL to build session, the new session will be created for current
  // main frame URL, the returned ContentCaptureSession is wrong.
  if (!BuildContentCaptureSessionLastSeen(content_capture_receiver, &session))
    return;
  DidRemoveSession(session);
}

void ContentCaptureReceiverManager::BuildContentCaptureSession(
    ContentCaptureReceiver* content_capture_receiver,
    bool ancestor_only,
    ContentCaptureSession* session) {
  if (!ancestor_only)
    session->push_back(content_capture_receiver->GetFrameContentCaptureData());

  content::RenderFrameHost* rfh = content_capture_receiver->rfh()->GetParent();
  while (rfh) {
    ContentCaptureReceiver* receiver = ContentCaptureReceiverForFrame(rfh);
    // TODO(michaelbai): Only creates ContentCaptureReceiver here, clean up the
    // code in RenderFrameCreated().
    if (!receiver) {
      RenderFrameCreated(rfh);
      receiver = ContentCaptureReceiverForFrame(rfh);
    }
    session->push_back(receiver->GetFrameContentCaptureData());
    rfh = receiver->rfh()->GetParent();
  }
}

bool ContentCaptureReceiverManager::BuildContentCaptureSessionLastSeen(
    ContentCaptureReceiver* content_capture_receiver,
    ContentCaptureSession* session) {
  session->push_back(
      content_capture_receiver->GetFrameContentCaptureDataLastSeen());
  content::RenderFrameHost* rfh = content_capture_receiver->rfh()->GetParent();
  while (rfh) {
    ContentCaptureReceiver* receiver = ContentCaptureReceiverForFrame(rfh);
    if (!receiver)
      return false;
    session->push_back(receiver->GetFrameContentCaptureDataLastSeen());
    rfh = receiver->rfh()->GetParent();
  }
  return true;
}

}  // namespace content_capture
