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
#include "chrome/browser/ui/tabs/organizer/organizer_panel_utils.h"
#include "chrome/browser/ui/views/animations/organizer_panel_animations.h"
#include "chrome/browser/ui/views/frame/custom_corners_background.h"
#include "chrome/browser/ui/views/tabs/organizer/layout_constants.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "extensions/buildflags/buildflags.h"
#include "ui/accessibility/ax_enums.mojom-shared.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/webview/web_contents_set_background_color.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view_class_properties.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/ui/views/tabs/organizer/organizer_panel_extension_view.h"
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

namespace {

// Custom WebView which forwards accelerators to the browser.
//
// This is required because on Mac, keypresses are routed directly to the
// WebContents without touching the normal browser event handling logic unless
// they're forwarded (for example, by an UnhandledKeyboardEventHandler).
class OrganizerPanelWebView : public views::WebView {
  METADATA_HEADER(OrganizerPanelWebView, views::WebView)
 public:
  explicit OrganizerPanelWebView(Profile* profile) : WebView(profile) {}
  ~OrganizerPanelWebView() override = default;

  // views::WebView:
  bool HandleKeyboardEvent(
      content::WebContents* source,
      const input::NativeWebKeyboardEvent& event) override {
    return keyboard_event_handler_.HandleKeyboardEvent(event,
                                                       GetFocusManager());
  }

 private:
  views::UnhandledKeyboardEventHandler keyboard_event_handler_;
};

BEGIN_METADATA(OrganizerPanelWebView)
END_METADATA

// The normal implementation of the panel view.
class OrganizerPanelViewImpl : public OrganizerPanelView {
 public:
  explicit OrganizerPanelViewImpl(BrowserWindowInterface& browser)
      : OrganizerPanelView(browser) {
    SetLayoutManager(std::make_unique<views::FillLayout>());
    auto* const web_view = AddChildView(
        std::make_unique<OrganizerPanelWebView>(browser.GetProfile()));
    webui::SetBrowserWindowInterface(web_view->GetWebContents(), &browser);
    views::WebContentsSetBackgroundColor::CreateForWebContentsWithColor(
        web_view->GetWebContents(), SK_ColorTRANSPARENT);
    web_view->LoadInitialURL(GURL(chrome::kChromeUIOrganizerPanelURL));
    web_view->SetProperty(
        views::kFlexBehaviorKey,
        views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                                 views::MaximumFlexSizeRule::kUnbounded));
    web_view->SetProperty(views::kElementIdentifierKey, kWebViewElementId);
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
  SetProperty(views::kElementIdentifierKey, kOrganizerPanelElementId);

  auto& accessibility = GetViewAccessibility();
  accessibility.SetRole(ax::mojom::Role::kPane);
  accessibility.SetName(l10n_util::GetStringUTF16(IDS_ORGANIZER_PANEL));
  SetFocusBehavior(FocusBehavior::NEVER);
}

OrganizerPanelView::~OrganizerPanelView() = default;

bool OrganizerPanelView::IsInExtensionModeForTesting() const {
  return false;
}

BEGIN_METADATA(OrganizerPanelView)
END_METADATA
