// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/organizer/organizer_panel_view.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/animation/browser_animation_controller.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/views/animations/organizer_panel_animations.h"
#include "chrome/browser/ui/views/frame/custom_corners_background.h"
#include "chrome/browser/ui/views/tabs/organizer/layout_constants.h"
#include "chrome/browser/ui/views/tabs/organizer/organizer_panel_utils.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "extensions/buildflags/buildflags.h"
#include "ui/accessibility/ax_enums.mojom-shared.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/webview/web_contents_set_background_color.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/view_class_properties.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/ui/views/tabs/organizer/organizer_panel_extension_view.h"
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

namespace {

// The normal implementation of the panel view.
class OrganizerPanelViewImpl : public OrganizerPanelView {
 public:
  explicit OrganizerPanelViewImpl(BrowserWindowInterface& browser)
      : OrganizerPanelView(browser) {
    auto web_view = std::make_unique<views::WebView>(browser.GetProfile());
    webui::SetBrowserWindowInterface(web_view->GetWebContents(), &browser);
    views::WebContentsSetBackgroundColor::CreateForWebContentsWithColor(
        web_view->GetWebContents(), SK_ColorTRANSPARENT);
    web_view->LoadInitialURL(GURL(chrome::kChromeUIOrganizerPanelURL));
    web_view->SetProperty(
        views::kFlexBehaviorKey,
        views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                                 views::MaximumFlexSizeRule::kUnbounded));
    web_view->SetProperty(views::kElementIdentifierKey, kWebViewElementId);
    AddChildView(std::move(web_view));
  }
};

}  // namespace

// static
std::unique_ptr<OrganizerPanelView> OrganizerPanelView::Create(
    BrowserWindowInterface& browser) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  if (organizer_panel::IsShowExtensionsSidePanelUiInOrganizerPanelEnabled()) {
    return std::make_unique<OrganizerPanelExtensionView>(browser);
  }
#endif
  return std::make_unique<OrganizerPanelViewImpl>(browser);
}

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(OrganizerPanelView, kWebViewElementId);

OrganizerPanelView::OrganizerPanelView(BrowserWindowInterface& browser)
    : animation_subscription_(
          BrowserAnimationController::From(&browser)->Subscribe(
              OrganizerPanelAnimations::kOrganizerPanel,
              base::BindRepeating(
                  [](OrganizerPanelView* view,
                     const BrowserAnimationController*,
                     BrowserAnimationUpdate) { view->InvalidateLayout(); },
                  base::Unretained(this)))) {
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
  layer()->SetIsFastRoundedCorner(true);

  SetPreferredSize(gfx::Size(organizer_panel::kOrganizerPanelMinWidth, 0));
  SetProperty(views::kElementIdentifierKey, kOrganizerPanelViewElementId);

  auto& accessibility = GetViewAccessibility();
  accessibility.SetRole(ax::mojom::Role::kPane);
  accessibility.SetName(l10n_util::GetStringUTF16(IDS_ORGANIZER_PANEL));
  SetFocusBehavior(FocusBehavior::NEVER);
}

OrganizerPanelView::~OrganizerPanelView() = default;

bool OrganizerPanelView::IsInExtensionModeForTesting() const {
  return false;
}

void OrganizerPanelView::Layout(PassKey) {
  LayoutSuperclass<views::View>(this);

  // Set clip region based on parent.
  if (parent()) {
    const gfx::Rect clip_bounds = GetLocalBounds();
    const gfx::Rect parent_bounds = views::View::ConvertRectToTarget(
        parent(), this, parent()->GetLocalBounds());
    gfx::Rect clip_rect = clip_bounds;
    clip_rect.Intersect(parent_bounds);
    layer()->SetClipRect(clip_rect);
    gfx::RoundedCornersF corners;
    if (parent()->background()) {
      if (auto* const background =
              parent()->background()->AsA<CustomCornersBackground>();
          background && background->is_visible()) {
        // Note: this mirrors for RtL.
        corners = background->GetRoundedCornerRadii().value_or(
            gfx::RoundedCornersF());
        // These bounds are also mirrored for RtL.
        if (clip_bounds.x() < parent_bounds.x()) {
          corners.set_upper_left(0.f);
          corners.set_lower_left(0.f);
        }
        if (clip_bounds.right() > parent_bounds.right()) {
          corners.set_upper_right(0.f);
          corners.set_lower_right(0.f);
        }
      }
    }
    layer()->SetRoundedCornerRadius(corners);
  }
}

BEGIN_METADATA(OrganizerPanelView)
END_METADATA
