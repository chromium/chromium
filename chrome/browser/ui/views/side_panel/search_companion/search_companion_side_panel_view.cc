// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/search_companion/search_companion_side_panel_view.h"

#include "base/functional/bind.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"
#include "chrome/browser/web_applications/web_app_icon_downloader.h"
#include "chrome/grit/generated_resources.h"
#include "components/download/content/factory/navigation_monitor_factory.h"
#include "components/download/content/public/download_navigation_observer.h"
#include "components/favicon_base/favicon_util.h"
#include "components/keyed_service/core/simple_factory_key.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/theme_provider.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/image/image.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/layout/layout_provider.h"

namespace {
std::unique_ptr<views::WebView> CreateWebView(
    views::View* host,
    content::BrowserContext* browser_context) {
  auto web_view = std::make_unique<views::WebView>(browser_context);
  // Set a flex behavior for the WebView to always fill out the extra space in
  // the parent view. In the minimum case, it will scale down to 0.
  web_view->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kUnbounded));
  // Set background of webview to the same background as the toolbar. This is to
  // prevent personal color themes from showing in the side panel when
  // navigating to a new Suggest results panel.
  web_view->SetBackground(views::CreateThemedSolidBackground(kColorToolbar));
  return web_view;
}

void ReplaceAll(std::string& str,
                const std::string& old,
                const std::string& repl) {
  size_t pos = 0;
  while ((pos = str.find(old, pos)) != std::string::npos) {
    str.replace(pos, old.length(), repl);
    pos += repl.length();
  }
}

std::string EscapeStringForHTML(std::string str) {
  ReplaceAll(str, std::string("&"), std::string("&amp;"));
  ReplaceAll(str, std::string("'"), std::string("&apos;"));
  ReplaceAll(str, std::string("\""), std::string("&quot;"));
  ReplaceAll(str, std::string(">"), std::string("&gt;"));
  ReplaceAll(str, std::string("<"), std::string("&lt;"));
  // Breaks injection if we don't truncate in this way.
  ReplaceAll(str, std::string("#"), std::string("hashtag"));

  return str;
}

}  // namespace

namespace search_companion {

static const char kStaticNoContentResponse[] =
    "data:text/html;charset=utf-8,"
    "<!DOCTYPE html>"
    "<p>No Content Available</p>";

static const char kStaticResponseTemplate[] =
    "data:text/html;charset=utf-8,"
    "<!DOCTYPE html>"
    "<p>Page URL: %s</p><p>Suggest Response: \"%s\"</p><p>Content Annotation "
    "Response: \"%s\"</p>"
    "Image Content Response: \"%s\"</p>";

SearchCompanionSidePanelView::SearchCompanionSidePanelView(
    BrowserView* browser_view)
    : browser_view_(browser_view) {
  auto* browser_context = browser_view->GetProfile();
  // Align views vertically top to bottom.
  SetOrientation(views::LayoutOrientation::kVertical);
  SetMainAxisAlignment(views::LayoutAlignment::kStart);
  // Allow view to be focusable in order to receive focus when side panel is
  // opened.
  SetFocusBehavior(FocusBehavior::ALWAYS);
  // Stretch views to fill horizontal bounds.
  SetCrossAxisAlignment(views::LayoutAlignment::kStretch);

  web_view_ = AddChildView(CreateWebView(this, browser_context));
  web_view_->SetVisible(true);
}

content::WebContents* SearchCompanionSidePanelView::GetWebContents() {
  return web_view_->GetWebContents();
}

void SearchCompanionSidePanelView::GetAccessibleNodeData(
    ui::AXNodeData* node_data) {
  return web_view_->GetAccessibleNodeData(node_data);
}

void SearchCompanionSidePanelView::OnBoundsChanged(
    const gfx::Rect& previous_bounds) {
  // Handle later
}

void SearchCompanionSidePanelView::UpdateContent(
    const std::string& page_url,
    const std::string& suggest_response,
    const std::string& content_annotation_response,
    const std::string& image_content_response) {
  GetWebContents()->Resize(bounds());
  if (suggest_response.empty() && content_annotation_response.empty()) {
    web_view_->GetWebContents()->GetController().LoadURL(
        GURL(kStaticNoContentResponse), content::Referrer(),
        ui::PAGE_TRANSITION_FROM_API, std::string());
  } else {
    // response.substr() to crop initial characters: ")]}'"
    web_view_->GetWebContents()->GetController().LoadURL(
        GURL(base::StringPrintf(
            kStaticResponseTemplate, page_url.c_str(),
            EscapeStringForHTML(suggest_response.substr(4)).c_str(),
            EscapeStringForHTML(content_annotation_response).c_str(),
            EscapeStringForHTML(image_content_response).c_str())),
        content::Referrer(), ui::PAGE_TRANSITION_FROM_API, std::string());
  }
}

SearchCompanionSidePanelView::~SearchCompanionSidePanelView() = default;

}  // namespace search_companion
