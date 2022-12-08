// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/accessibility/caption_bubble_session_observer_views.h"

#include <memory>

#include "components/live_caption/caption_bubble_session_observer.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"

namespace captions {

CaptionBubbleSessionObserverViews::CaptionBubbleSessionObserverViews(
    content::WebContents* web_contents)
    : CaptionBubbleSessionObserver(),
      content::WebContentsObserver(web_contents),
      web_contents_id_(web_contents->GetBrowserContext()->UniqueId()) {}

CaptionBubbleSessionObserverViews::~CaptionBubbleSessionObserverViews() =
    default;

void CaptionBubbleSessionObserverViews::DidFinishNavigation(
    content::NavigationHandle* handle) {
  if (!handle->HasCommitted()) {
    return;
  }

  if (end_session_callback_ && handle->IsInPrimaryMainFrame() &&
      (!handle->IsSameOrigin() ||
       handle->GetReloadType() != content::ReloadType::NONE)) {
    end_session_callback_.Run(web_contents_id_);
  }
}

void CaptionBubbleSessionObserverViews::WebContentsDestroyed() {
  if (end_session_callback_)
    end_session_callback_.Run(web_contents_id_);
}

void CaptionBubbleSessionObserverViews::SetEndSessionCallback(
    EndSessionCallback callback) {
  end_session_callback_ = std::move(callback);
}

}  // namespace captions
