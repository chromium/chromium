// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_capture/browser/onscreen_content_provider.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "components/content_capture/browser/content_capture_consumer.h"
#include "components/content_capture/browser/content_capture_receiver.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "third_party/blink/public/mojom/favicon/favicon_url.mojom.h"

namespace content_capture {

WEB_CONTENTS_USER_DATA_KEY_IMPL(OnscreenContentProvider);

OnscreenContentProvider::OnscreenContentProvider(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<OnscreenContentProvider>(*web_contents) {
  web_contents->ForEachRenderFrameHostWithAction(
      [this](content::RenderFrameHost* render_frame_host) {
        // Don't cross into inner WebContents since we wouldn't be notified of
        // its changes.
        if (content::WebContents::FromRenderFrameHost(render_frame_host) !=
            this->web_contents()) {
          return content::RenderFrameHost::FrameIterationAction::kSkipChildren;
        }
        if (render_frame_host->IsRenderFrameLive()) {
          RenderFrameCreated(render_frame_host);
        }
        return content::RenderFrameHost::FrameIterationAction::kContinue;
      });
}

OnscreenContentProvider::~OnscreenContentProvider() = default;

// static
OnscreenContentProvider* OnscreenContentProvider::Create(
    content::WebContents* web_contents) {
  DCHECK(!FromWebContents(web_contents));
  CreateForWebContents(web_contents);
  return FromWebContents(web_contents);
}

// static
void OnscreenContentProvider::BindContentCaptureReceiver(
    mojo::PendingAssociatedReceiver<mojom::ContentCaptureReceiver>
        pending_receiver,
    content::RenderFrameHost* render_frame_host) {
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host);

  if (!web_contents)
    return;

  OnscreenContentProvider* manager =
      OnscreenContentProvider::FromWebContents(web_contents);
  if (!manager)
    return;

  auto* receiver = manager->ContentCaptureReceiverForFrame(render_frame_host);
  if (receiver)
    receiver->BindPendingReceiver(std::move(pending_receiver));
}

void OnscreenContentProvider::AddConsumer(ContentCaptureConsumer& consumer) {
  consumers_.push_back(&consumer);
}

void OnscreenContentProvider::RemoveConsumer(ContentCaptureConsumer& consumer) {
  for (auto it = consumers_.begin(); it != consumers_.end(); ++it) {
    if (*it == &consumer) {
      ContentCaptureSession session;
      if (BuildContentCaptureSessionForMainFrame(&session)) {
        consumer.DidRemoveSession(session);
      }
      consumers_.erase(it);
      return;
    }
  }
  NOTREACHED_IN_MIGRATION();
}

ContentCaptureReceiver* OnscreenContentProvider::ContentCaptureReceiverForFrame(
    content::RenderFrameHost* render_frame_host) const {
  auto mapping = frame_map_.find(render_frame_host->GetGlobalId());
  return mapping == frame_map_.end() ? nullptr : mapping->second.get();
}

void OnscreenContentProvider::RenderFrameCreated(
    content::RenderFrameHost* render_frame_host) {
  // The frame might not have content, but it could be parent of other frame. we
  // always create the ContentCaptureReceiver for ContentCaptureSession
  // purpose.
  if (ContentCaptureReceiverForFrame(render_frame_host))
    return;
  frame_map_.insert(std::make_pair(
      render_frame_host->GetGlobalId(),
      std::make_unique<ContentCaptureReceiver>(render_frame_host)));
}

void OnscreenContentProvider::RenderFrameDeleted(
    content::RenderFrameHost* render_frame_host) {
  if (auto* content_capture_receiver =
          ContentCaptureReceiverForFrame(render_frame_host)) {
    content_capture_receiver->RemoveSession();
  }
  frame_map_.erase(render_frame_host->GetGlobalId());
}

void OnscreenContentProvider::ReadyToCommitNavigation(
    content::NavigationHandle* navigation_handle) {
  // Don't remove the session for the same document navigation.
  if (!navigation_handle->IsSameDocument()) {
    if (auto* rfh = content::RenderFrameHost::FromID(
            navigation_handle->GetPreviousRenderFrameHostId())) {
      if (auto* receiver = ContentCaptureReceiverForFrame(rfh)) {
        receiver->RemoveSession();
      }
    }
  }

  if (auto* receiver = ContentCaptureReceiverForFrame(
          navigation_handle->GetRenderFrameHost())) {
    if (web_contents()->GetBrowserContext()->IsOffTheRecord() ||
        !ShouldCapture(navigation_handle->GetURL())) {
      receiver->StopCapture();
      return;
    }
    receiver->StartCapture();
  }
}

void OnscreenContentProvider::TitleWasSet(content::NavigationEntry* entry) {
  // Set the title to the mainframe.
  if (auto* receiver = ContentCaptureReceiverForFrame(
          web_contents()->GetPrimaryMainFrame())) {
    // To match what the user sees, intentionally get the title from WebContents
    // instead of NavigationEntry, though they might be same.
    receiver->SetTitle(web_contents()->GetTitle());
  }
}

void OnscreenContentProvider::DidCaptureContent(
    ContentCaptureReceiver* content_capture_receiver,
    const ContentCaptureFrame& data) {
  // The root of |data| is frame, we need get its ancestor only.
  ContentCaptureSession parent_session;
  BuildContentCaptureSession(content_capture_receiver, true /* ancestor_only */,
                             &parent_session);
  for (content_capture::ContentCaptureConsumer* consumer : consumers_) {
    consumer->DidCaptureContent(parent_session, data);
  }
}

void OnscreenContentProvider::DidUpdateContent(
    ContentCaptureReceiver* content_capture_receiver,
    const ContentCaptureFrame& data) {
  ContentCaptureSession parent_session;
  BuildContentCaptureSession(content_capture_receiver, true /* ancestor_only */,
                             &parent_session);
  for (content_capture::ContentCaptureConsumer* consumer : consumers_) {
    consumer->DidUpdateContent(parent_session, data);
  }
}

void OnscreenContentProvider::DidRemoveContent(
    ContentCaptureReceiver* content_capture_receiver,
    const std::vector<int64_t>& data) {
  ContentCaptureSession session;
  // The |data| is a list of text content id, the session should include
  // |content_capture_receiver| associated frame.
  BuildContentCaptureSession(content_capture_receiver,
                             false /* ancestor_only */, &session);
  for (content_capture::ContentCaptureConsumer* consumer : consumers_) {
    consumer->DidRemoveContent(session, data);
  }
}

void OnscreenContentProvider::DidRemoveSession(
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

  for (content_capture::ContentCaptureConsumer* consumer : consumers_) {
    consumer->DidRemoveSession(session);
  }
}

void OnscreenContentProvider::DidUpdateTitle(
    ContentCaptureReceiver* content_capture_receiver) {
  ContentCaptureSession session;
  BuildContentCaptureSession(content_capture_receiver,
                             /*ancestor_only=*/false, &session);

  // Shall only update mainframe's title.
  DCHECK(session.size() == 1);

  for (content_capture::ContentCaptureConsumer* consumer : consumers_) {
    consumer->DidUpdateTitle(*session.begin());
  }
}

void OnscreenContentProvider::DidUpdateFaviconURL(
    content::RenderFrameHost* render_frame_host,
    const std::vector<blink::mojom::FaviconURLPtr>& candidates) {
  if (ContentCaptureReceiver::
          disable_get_favicon_from_web_contents_for_testing()) {
    return;
  }
  NotifyFaviconURLUpdated(render_frame_host, candidates);
}

void OnscreenContentProvider::NotifyFaviconURLUpdated(
    content::RenderFrameHost* render_frame_host,
    const std::vector<blink::mojom::FaviconURLPtr>& candidates) {
  DCHECK(render_frame_host->IsInPrimaryMainFrame());
  if (auto* receiver = ContentCaptureReceiverForFrame(render_frame_host)) {
    receiver->UpdateFaviconURL(candidates);
  }
}

void OnscreenContentProvider::DidUpdateFavicon(
    ContentCaptureReceiver* content_capture_receiver) {
  ContentCaptureSession session;
  BuildContentCaptureSession(content_capture_receiver,
                             /*ancestor_only=*/false, &session);

  // Shall only update mainframe's title.
  DCHECK(session.size() == 1);
  for (content_capture::ContentCaptureConsumer* consumer : consumers_) {
    consumer->DidUpdateFavicon(*session.begin());
  }
}

void OnscreenContentProvider::BuildContentCaptureSession(
    ContentCaptureReceiver* content_capture_receiver,
    bool ancestor_only,
    ContentCaptureSession* session) {
  if (!ancestor_only)
    session->push_back(content_capture_receiver->GetContentCaptureFrame());

  content::RenderFrameHost* rfh =
      content_capture_receiver->rfh()->GetParentOrOuterDocument();
  while (rfh) {
    ContentCaptureReceiver* receiver = ContentCaptureReceiverForFrame(rfh);
    // TODO(michaelbai): Only creates ContentCaptureReceiver here, clean up the
    // code in RenderFrameCreated().
    if (!receiver) {
      RenderFrameCreated(rfh);
      receiver = ContentCaptureReceiverForFrame(rfh);
      DCHECK(receiver);
    }
    session->push_back(receiver->GetContentCaptureFrame());
    rfh = receiver->rfh()->GetParentOrOuterDocument();
  }
}

bool OnscreenContentProvider::BuildContentCaptureSessionLastSeen(
    ContentCaptureReceiver* content_capture_receiver,
    ContentCaptureSession* session) {
  session->push_back(
      content_capture_receiver->GetContentCaptureFrameLastSeen());
  content::RenderFrameHost* rfh =
      content_capture_receiver->rfh()->GetParentOrOuterDocument();
  while (rfh) {
    ContentCaptureReceiver* receiver = ContentCaptureReceiverForFrame(rfh);
    if (!receiver)
      return false;
    session->push_back(receiver->GetContentCaptureFrameLastSeen());
    rfh = receiver->rfh()->GetParentOrOuterDocument();
  }
  return true;
}

bool OnscreenContentProvider::BuildContentCaptureSessionForMainFrame(
    ContentCaptureSession* session) {
  if (auto* receiver = ContentCaptureReceiverForFrame(
          web_contents()->GetPrimaryMainFrame())) {
    session->push_back(receiver->GetContentCaptureFrame());
    return true;
  }
  return false;
}

bool OnscreenContentProvider::ShouldCapture(const GURL& url) {
  for (content_capture::ContentCaptureConsumer* consumer : consumers_) {
    if (consumer->ShouldCapture(url))
      return true;
  }
  return false;
}
}  // namespace content_capture
