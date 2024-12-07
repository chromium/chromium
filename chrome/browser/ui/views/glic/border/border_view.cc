// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/glic/border/border_view.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/view_class_properties.h"

namespace glic {

// static
BorderView* BorderView::FindBorderForWebContents(
    content::WebContents* web_contents) {
  Browser* browser = chrome::FindBrowserWithTab(web_contents);
  if (!browser || !browser->window()) {
    // We might not have a browser or browser window in unittests.
    return nullptr;
  }
  // `contents_web_view_` is initiated in browser view's ctor.
  CHECK(browser->GetBrowserView().contents_web_view());
  return browser->GetBrowserView().contents_web_view()->glic_border();
}

// static.
//
// TODO(liuwilliam): Currently there is only one border animation per Profile,
// and that's for the last active web contents, whose contents was requested by
// glic. We might expand the animation scope to multiple WebContents. Update the
// impl correspondingly.
void BorderView::CancelAllAnimationsForProfile(Profile* profile) {
  Browser* browser = chrome::FindBrowserWithProfile(profile);
  if (!browser || !browser->window()) {
    // Unittests, or the View tree is torn down.
    return;
  }
  CHECK(browser->GetBrowserView().contents_web_view());
  browser->GetBrowserView()
      .contents_web_view()
      ->glic_border()
      ->CancelAnimation();
}

BorderView::BorderView() = default;

BorderView::~BorderView() = default;

void BorderView::OnPaint(gfx::Canvas* canvas) {
  // Paint onto `canvas`.
  // Call `SchedulePaint()` if the animation hasn't finished.
}

void BorderView::OnChildViewAdded(views::View* observed_view,
                                  views::View* child) {
  // Use this API to make sure our border view is always the z-top-most of the
  // contents_web_view_ of the browser view.
}

void BorderView::OnAnimationStep(base::TimeTicks timestamp) {
  // Update the border style based on`timestamp` and the motion curve(s).
}

void BorderView::OnCompositingShuttingDown(ui::Compositor* compositor) {}

// TODO(crbug.com/381424645): Implementation.
void BorderView::StartAnimation() {}

void BorderView::CancelAnimation() {}

BEGIN_METADATA(BorderView)
END_METADATA

}  // namespace glic
