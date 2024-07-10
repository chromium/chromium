// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/lens_overlay_page_action_icon_view.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/lens/lens_overlay_controller.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_view_views.h"
#include "chrome/browser/ui/webui/new_tab_page/new_tab_page_ui.h"
#include "chrome/browser/ui/webui/new_tab_page_third_party/new_tab_page_third_party_ui.h"
#include "chrome/browser/ui/webui/ntp/new_tab_ui.h"
#include "chrome/browser/user_education/user_education_service.h"
#include "components/lens/lens_features.h"
#include "components/omnibox/browser/omnibox_prefs.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/view_class_properties.h"

namespace {

// TODO(tluk): Similar bespoke checks are used throughout the codebase, this
// approach is taken from BookmarkTabHelper. This should be factored out as a
// common util and other callsites converted to use this.
bool IsNewTabPage(content::WebContents* const web_contents) {
  // Use the committed entry (or the visible entry, if the committed entry is
  // the initial NavigationEntry) so the bookmarks bar disappears at the same
  // time the page does.
  CHECK(web_contents);
  content::NavigationEntry* entry =
      web_contents->GetController().GetLastCommittedEntry();
  if (entry->IsInitialEntry()) {
    entry = web_contents->GetController().GetVisibleEntry();
  }
  const GURL& url = entry->GetURL();
  return NewTabUI::IsNewTab(url) || NewTabPageUI::IsNewTabPageOrigin(url) ||
         NewTabPageThirdPartyUI::IsNewTabPageOrigin(url) ||
         search::NavEntryIsInstantNTP(web_contents, entry);
}

}  // namespace

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

  SetProperty(views::kElementIdentifierKey,
              kLensOverlayPageActionIconElementId);
  GetViewAccessibility().SetName(
      l10n_util::GetStringUTF16(IDS_CONTENT_LENS_OVERLAY_ENTRYPOINT_LABEL),
      ax::mojom::NameFrom::kAttribute);

  if (!lens::features::IsOmniboxEntrypointAlwaysVisible()) {
    SetLabel(
        l10n_util::GetStringUTF16(IDS_CONTENT_LENS_OVERLAY_ENTRYPOINT_LABEL));
    SetUseTonalColorsWhenExpanded(true);
    SetPaintLabelOverSolidBackground(true);
  }
}

LensOverlayPageActionIconView::~LensOverlayPageActionIconView() = default;

void LensOverlayPageActionIconView::UpdateImpl() {
  bool enabled = browser_->profile()->GetPrefs()->GetBoolean(
      omnibox::kShowGoogleLensShortcut);

  bool location_bar_has_focus = false;
  if (BrowserView* const browser_view =
          BrowserView::GetBrowserViewForBrowser(browser_);
      browser_view && browser_view->GetLocationBarView()) {
    if (const views::FocusManager* const focus_manager = GetFocusManager()) {
      location_bar_has_focus = browser_view->GetLocationBarView()->Contains(
          focus_manager->GetFocusedView());
    }
  }

  // The overlay is unavailable on the NTP as it is unlikely to be useful to
  // users on the page, it would also appear immediately when a new tab or
  // window is created due to focus immediatey jumping into the location bar.
  auto* web_Contents = GetWebContents();
  const bool lens_overlay_available =
      web_Contents &&
      LensOverlayController::GetController(web_Contents) != nullptr &&
      !IsNewTabPage(web_Contents);

  const bool should_show_lens_overlay =
      enabled && lens_overlay_available &&
      (lens::features::IsOmniboxEntrypointAlwaysVisible() ||
       location_bar_has_focus);
  SetVisible(should_show_lens_overlay);
  ResetSlideAnimation(true);

  // TODO(pbos): Investigate why this call seems to be required to pick up that
  // this should still be painted in an expanded state. I.e. without this call
  // the last call to IconLabelBubbleView::UpdateBackground() seems to think
  // that the label isn't showing / shouldn't paint over a solid background.
  UpdateBackground();

  if (update_callback_for_testing_) {
    std::move(update_callback_for_testing_).Run();
  }
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
  // TODO(tluk): Update GetSizeForLabelWidth() to correctly calculate padding
  // for empty label widths and replace the calculation below.
  const gfx::Size full_size =
      PageActionIconView::CalculatePreferredSize(available_size);
  const gfx::Insets view_insets = GetInsets();
  const gfx::Size reduced_size =
      image_container_view()->GetPreferredSize() +
      gfx::Size(view_insets.left() * 2, view_insets.height());

  // Size icon to its full width if there are no size constraints.
  if (!available_size.width().is_bounded()) {
    return full_size;
  }

  // Handle minimum size requests.
  int available_width = available_size.width().value();
  if (available_width == 0) {
    return reduced_size;
  }

  // Adjust the available width by the minimum size of the parent's other
  // children. This is necessary as the PageActionIconContainer's BoxLayout
  // passes in the total size available to each of its child views, and the
  // combined preferred size calculations of the children may not correctly
  // respect the available size.
  available_width -= parent()->GetMinimumSize().width() - reduced_size.width();

  // TODO(crbug.com/350541615): Currently all page action icons are treated as
  // non-resizable by LocationBarLayout. Page actions should be updated to be
  // resizable by the LocationBarLayout, until then control the icon's preferred
  // size based on the available space.
  return available_width < full_size.width() ? reduced_size : full_size;
}

BEGIN_METADATA(LensOverlayPageActionIconView)
END_METADATA
