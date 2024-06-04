// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/lens_overlay_page_action_icon_view.h"

#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/lens/lens_overlay_controller.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_view_views.h"
#include "chrome/browser/user_education/user_education_service.h"
#include "components/lens/lens_features.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/metadata/metadata_impl_macros.h"

LensOverlayPageActionIconView::LensOverlayPageActionIconView(
    Browser* browser,
    IconLabelBubbleView::Delegate* icon_label_bubble_delegate,
    PageActionIconView::Delegate* page_action_icon_delegate)
    : PageActionIconView(nullptr,
                         0,
                         icon_label_bubble_delegate,
                         page_action_icon_delegate,
                         "LensOverlay"),
      browser_(browser) {
  CHECK(browser_);
  image_container_view()->SetFlipCanvasOnPaintForRTLUI(false);
  SetUpForInOutAnimation();

  SetLabel(l10n_util::GetStringUTF16(IDS_CONTENT_CONTEXT_LENS_OVERLAY));
  SetUseTonalColorsWhenExpanded(true);
  SetPaintLabelOverSolidBackground(true);
}

LensOverlayPageActionIconView::~LensOverlayPageActionIconView() = default;

void LensOverlayPageActionIconView::UpdateImpl() {
  bool location_bar_has_focus = false;
  if (BrowserView* const browser_view =
          BrowserView::GetBrowserViewForBrowser(browser_);
      browser_view && browser_view->GetLocationBarView()) {
    if (const views::FocusManager* const focus_manager = GetFocusManager()) {
      location_bar_has_focus = browser_view->GetLocationBarView()->Contains(
          focus_manager->GetFocusedView());
    }
  }
  const bool lens_overlay_available =
      GetWebContents() &&
      LensOverlayController::GetController(GetWebContents()) != nullptr;
  SetVisible(location_bar_has_focus && lens_overlay_available);
  ResetSlideAnimation(true);

  // TODO(pbos): Investigate why this call seems to be required to pick up that
  // this should still be painted in an expanded state. I.e. without this call
  // the last call to IconLabelBubbleView::UpdateBackground() seems to think
  // that the label isn't showing / shouldn't paint over a solid background.
  UpdateBackground();
}

void LensOverlayPageActionIconView::OnExecuting(
    PageActionIconView::ExecuteSource source) {
  LensOverlayController* const controller =
      LensOverlayController::GetController(GetWebContents());
  CHECK(controller);

  controller->ShowUI(lens::LensOverlayInvocationSource::kOmnibox);
  UserEducationService::MaybeNotifyPromoFeatureUsed(
      GetWebContents()->GetBrowserContext(), lens::features::kLensOverlay);
}

views::BubbleDialogDelegate* LensOverlayPageActionIconView::GetBubble() const {
  return nullptr;
}

const gfx::VectorIcon& LensOverlayPageActionIconView::GetVectorIcon() const {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  return vector_icons::kGoogleLensMonochromeLogoIcon;
#else
  return vector_icons::kSearchIcon;
#endif
}

gfx::Size LensOverlayPageActionIconView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  // TODO: tluk - Currently all page action icons are treated as non-resizable
  // by LocationBarLayout. Page actions should be updated to be resizable by
  // the LocationBarLayout, until then control the icon's preferred size
  // based on the available space.
  const gfx::Size full_size =
      PageActionIconView::CalculatePreferredSize(available_size);
  const gfx::Size reduced_size = GetSizeForLabelWidth(0);
  return available_size.width() < full_size.width() ? reduced_size : full_size;
}

BEGIN_METADATA(LensOverlayPageActionIconView)
END_METADATA
