// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/autofill_field_promo_view_impl.h"

#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/views/autofill/popup/popup_view_utils.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/views/view_class_properties.h"

namespace {

views::View* GetContentsWebView(content::WebContents* web_contents) {
  Browser* browser = chrome::FindBrowserWithTab(web_contents);
  if (!browser) {
    return nullptr;
  }
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  return browser_view ? browser_view->contents_web_view() : nullptr;
}

}  // namespace

namespace autofill {

// static
base::WeakPtr<AutofillFieldPromoView> AutofillFieldPromoView::CreateAndShow(
    content::WebContents* web_contents,
    const gfx::RectF& element_bounds,
    const ui::ElementIdentifier& promo_element_identifier) {
  views::View* contents_web_view = GetContentsWebView(web_contents);
  if (!contents_web_view) {
    return nullptr;
  }

  base::WeakPtr<AutofillFieldPromoView> promo_view =
      contents_web_view
          ->AddChildView(base::WrapUnique<AutofillFieldPromoViewImpl>(
              new AutofillFieldPromoViewImpl(web_contents, element_bounds,
                                             promo_element_identifier)))
          ->GetWeakPtr();
  return promo_view;
}

AutofillFieldPromoViewImpl::AutofillFieldPromoViewImpl(
    content::WebContents* web_contents,
    const gfx::RectF& element_bounds,
    const ui::ElementIdentifier& promo_element_identifier)
    : web_contents_(web_contents) {
  SetViewBounds(element_bounds);
  SetProperty(views::kElementIdentifierKey, promo_element_identifier);
}

AutofillFieldPromoViewImpl::~AutofillFieldPromoViewImpl() = default;

bool AutofillFieldPromoViewImpl::OverlapsWithPictureInPictureWindow() const {
  return BoundsOverlapWithPictureInPictureWindow(GetBoundsInScreen());
}

void AutofillFieldPromoViewImpl::Close() {
  views::View* contents_web_view = GetContentsWebView(web_contents_);
  // If the contents web view no longer exists, neither do its children.
  if (!contents_web_view) {
    return;
  }
  if (contents_web_view->Contains(this)) {
    contents_web_view->RemoveChildView(this);
  }
  weak_ptr_factory_.InvalidateWeakPtrs();

  // The deletion of the view is asynchronous, so it can die after
  // `web_contents_` is destroyed. In order to avoid dangling pointers,
  // `web_contents_` is set to null.
  web_contents_ = nullptr;
  base::SingleThreadTaskRunner::GetCurrentDefault()->DeleteSoon(FROM_HERE,
                                                                this);
}

base::WeakPtr<AutofillFieldPromoView> AutofillFieldPromoViewImpl::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void AutofillFieldPromoViewImpl::SetViewBounds(
    const gfx::RectF& element_bounds) {
  // The type of `element_bounds` is changed from `gfx::RectF` to `gfx::Rect`.
  // They need to have the same type as the web contents bounds, because they
  // are later intersected with them.
  gfx::Rect element_bounds_container_bounds_intersection =
      gfx::ToEnclosingRect(element_bounds);
  // The coordinates of the element bounds are represented in a coordinate
  // system which has the origin in the top-left corner of the `web_contents_`.
  //
  // The coordinates of `web_contents_` are represented in a coordinate system
  // which has the origin in the top-left corner of the screen.
  //
  // O1 - origin of the coordinate system of `web_contents_` (top-left corner
  // of the screen) O2 - origin of the coordinate system of the element
  // (top-left corner of `web_contents_`)
  //
  //     O1
  //      *___________________________________________________________
  //      |              THIS AREA IS THE SCREEN                     |
  //      |                                                          |
  //      |             O2                                           |
  //      |              *_____________________________________      |
  //      |              |                                     |     |
  //      |              |    THIS AREA IS THE WEB CONTENTS    |     |
  //      |              |                                     |     |
  //      |              |      _________                      |     |
  //      |              |     |_________| <-- ELEMENT         |     |
  //      |              |                                     |     |
  //      |              |_____________________________________|     |
  //      |                                                          |
  //      |__________________________________________________________|
  //
  // In order to be able to intersect them, they need to be represented in the
  // same coordinate system. Thus, the bounds of the `web_contents_` are
  // translated so that O1 overlaps with O2.
  gfx::Rect web_contents_translated_bounds =
      web_contents_->GetContainerBounds();
  web_contents_translated_bounds -=
      web_contents_translated_bounds.OffsetFromOrigin();

  // Some elements may be partially outside the visible content area. Thus, we
  // take into consideration only the part of the element which is inside the
  // visible content area (i.e. the intersection between the web content bounds
  // and the element bounds). Otherwise, the view would be partially outside of
  // the visible content area and the IPH could be displayed there.
  element_bounds_container_bounds_intersection.Intersect(
      web_contents_translated_bounds);

  // The view is drawn such that it completely overlaps with the bottom part of
  // the element (it cannot be below the element, because the IPH would then
  // float in the air). Height is not relevant as long as it is greater than 0.
  SetBounds(element_bounds_container_bounds_intersection.x(),
            element_bounds_container_bounds_intersection.y() +
                element_bounds_container_bounds_intersection.height() - 1,
            element_bounds_container_bounds_intersection.width(), 1);
  SetPaintToLayer();
}

BEGIN_METADATA(AutofillFieldPromoViewImpl)
END_METADATA

}  // namespace autofill
