// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/cookie_controls/cookie_controls_bubble_coordinator.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/cookie_controls/cookie_controls_bubble_view_controller.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

namespace content {
class WebContents;
}

CookieControlsBubbleCoordinator::CookieControlsBubbleCoordinator(
    views::View* anchor_view)
    : anchor_view_(anchor_view) {}

CookieControlsBubbleCoordinator::~CookieControlsBubbleCoordinator() = default;

void CookieControlsBubbleCoordinator::ShowBubble(
    content::WebContents* web_contents,
    content_settings::CookieControlsController* controller) {
  // TODO(crbug.com/1446230): Add ShowBubble logic.
}

void CookieControlsBubbleCoordinator::OnViewIsDeleting(
    views::View* observed_view) {
  // TODO(crbug.com/1446230): Add OnViewIsDeleting logic.
}
