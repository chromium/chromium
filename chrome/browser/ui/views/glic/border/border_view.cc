// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/glic/border/border_view.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/canvas.h"
#include "ui/views/view_class_properties.h"

namespace glic {

// static
BorderView* BorderView::FindBorderForWebContents(
    const content::WebContents* web_contents) {
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
void BorderView::CancelAllAnimationsForProfile(Profile* profile) {
  std::vector<Browser*> browsers = chrome::FindAllBrowsersWithProfile(profile);
  for (auto* browser : browsers) {
    if (!browser || !browser->window()) {
      // Unittests, or the View tree is torn down.
      continue;
    }
    CHECK(browser->GetBrowserView().contents_web_view());
    browser->GetBrowserView()
        .contents_web_view()
        ->glic_border()
        ->CancelAnimation();
  }
}

BorderView::BorderView() = default;

BorderView::~BorderView() = default;

void BorderView::OnPaint(gfx::Canvas* canvas) {
  // TODO(baranerf): Modify this to a variable width when adding animation.
  constexpr static int kBorderWidth = 5;

  views::View::OnPaint(canvas);
  cc::PaintFlags flags;
  flags.setStyle(cc::PaintFlags::kStroke_Style);
  flags.setColor(GetColorProvider()->GetColor(ui::kColorSysPrimary));
  flags.setStrokeWidth(kBorderWidth);
  canvas->DrawRect(GetContentsBounds(), flags);
}

void BorderView::OnChildViewAdded(views::View* observed_view,
                                  views::View* child) {
  MakeTopMostChild(observed_view, child);
}

void BorderView::OnChildViewReordered(views::View* observed_view,
                                      views::View* child) {
  MakeTopMostChild(observed_view, child);
}

void BorderView::OnAnimationStep(base::TimeTicks timestamp) {
  // Update the border style based on`timestamp` and the motion curve(s).
}

void BorderView::OnCompositingShuttingDown(ui::Compositor* compositor) {}

void BorderView::StartAnimation() {
  if (animation_ongoing_) {
    // The user can click on the glic icon after the window is shown. The
    // animation is already playing at that time.
    return;
  }
  if (!parent()) {
    base::debug::DumpWithoutCrashing();
    return;
  }
  animation_ongoing_ = true;
  SetBoundsRect(parent()->GetVisibleBounds());
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
}

void BorderView::CancelAnimation() {
  if (!animation_ongoing_) {
    return;
  }
  animation_ongoing_ = false;
  // `DestroyLayer()` schedules another paint to repaint the affected area by
  // the destroyed layer.
  DestroyLayer();
}

void BorderView::MakeTopMostChild(View* parent_view, View* child) {
  CHECK(parent());
  if (parent_view != parent()) {
    return;
  }
  if (reorder_in_progress_) {
    if (this != child) {
      // While we are reordering `this`, another sibling is also reordering
      // itself.
      base::debug::DumpWithoutCrashing();
    } else {
      // An in-progress `this->MakeTopMostChild(parent, this)` is in progress,
      // which dispatches a `ViewObserver::OnChildViewReordered`.
    }
    return;
  }
  base::AutoReset<bool> reset(&reorder_in_progress_, true);
  size_t z_topmost = parent_view->children().size();
  parent_view->ReorderChildView(this, z_topmost);
}

BEGIN_METADATA(BorderView)
END_METADATA

}  // namespace glic
