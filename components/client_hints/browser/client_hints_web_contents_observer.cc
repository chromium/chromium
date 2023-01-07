// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/client_hints/browser/client_hints_web_contents_observer.h"

#include "content/public/browser/browser_context.h"
#include "content/public/browser/client_hints_controller_delegate.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "ui/gfx/geometry/size_f.h"

namespace client_hints {

ClientHintsWebContentsObserver::ClientHintsWebContentsObserver(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<ClientHintsWebContentsObserver>(
          *web_contents) {}

ClientHintsWebContentsObserver::~ClientHintsWebContentsObserver() = default;

void ClientHintsWebContentsObserver::OnVisibilityChanged(
    content::Visibility visibility) {
  if (visibility == content::Visibility::VISIBLE) {
    UpdateVisibleMainFrameViewportSize();
  }
}

void ClientHintsWebContentsObserver::PrimaryMainFrameWasResized(
    bool width_changed) {
  content::Visibility visibility = web_contents()->GetVisibility();
  if (visibility == content::Visibility::VISIBLE) {
    UpdateVisibleMainFrameViewportSize();
  }
}

void ClientHintsWebContentsObserver::UpdateVisibleMainFrameViewportSize() {
  content::RenderWidgetHostView* view =
      web_contents()->GetPrimaryMainFrame()->GetRenderWidgetHost()->GetView();
  if (!view) {
    return;
  }

  gfx::Size visible_main_frame_viewport_size = view->GetVisibleViewportSize();

  content::ClientHintsControllerDelegate* client_hints_controller_delegate =
      web_contents()->GetBrowserContext()->GetClientHintsControllerDelegate();
  if (client_hints_controller_delegate) {
    client_hints_controller_delegate->SetMostRecentMainFrameViewportSize(
        visible_main_frame_viewport_size);
  }
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(ClientHintsWebContentsObserver);

}  // namespace client_hints
