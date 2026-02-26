// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/webui_toolbar/webui_toolbar_layout_css_helper.h"

#include <string_view>

#include "base/functional/bind.h"
#include "base/memory/ref_counted_memory.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/common/webui_url_constants.h"

namespace {

constexpr char kPathPrefix[] = "layout_constants";
constexpr char kPathV0[] = "layout_constants_v0.css";

std::string_view LayoutConstantToCssVarName(LayoutConstant layout_constant) {
  using enum LayoutConstant;
  switch (layout_constant) {
    case kAppMenuProfileRowAvatarIconSize:
      return "--app-menu-profile-row-avatar-icon-size";
    case kAppMenuMaximumCharacterLength:
      return "--app-menu-maximum-character-length";
    case kBookmarkBarHeight:
      return "--bookmark-bar-height";
    case kBookmarkBarButtonHeight:
      return "--bookmark-bar-button-height";
    case kBookmarkBarButtonPadding:
      return "--bookmark-bar-button-padding";
    case kBookmarkBarButtonImageLabelPadding:
      return "--bookmark-bar-button-image-label-padding";
    case kDownloadIconSize:
      return "--download-icon-size";
    case kLocationBarBubbleFontVerticalPadding:
      return "--location-bar-bubble-font-vertical-padding";
    case kLocationBarBubbleAnchorVerticalInset:
      return "--location-bar-bubble-anchor-vertical-inset";
    case kLocationBarChildInteriorPadding:
      return "--location-bar-child-interior-padding";
    case kLocationBarChildCornerRadius:
      return "--location-bar-child-corner-radius";
    case kLocationBarChipIconSize:
      return "--location-bar-chip-icon-size";
    case kLocationBarChipPadding:
      return "--location-bar-chip-padding";
    case kLocationBarElementPadding:
      return "--location-bar-element-padding";
    case kLocationBarHeight:
      return "--location-bar-height";
    case kLocationBarPageInfoIconVerticalPadding:
      return "--location-bar-page-info-icon-vertical-padding";
    case kLocationBarTrailingDecorationEdgePadding:
      return "--location-bar-trailing-decoration-edge-padding";
    case kLocationBarTrailingDecorationInnerPadding:
      return "--location-bar-trailing-decoration-inner-padding";
    case kLocationBarIconSize:
      return "--location-bar-icon-size";
    case kLocationBarLeadingIconSize:
      return "--location-bar-leading-icon-size";
    case kLocationBarTrailingIconSize:
      return "--location-bar-trailing-icon-size";
    case kLocationBarMargin:
      return "--location-bar-margin";
    case kMainBackgroundRegionCornerRadius:
      return "--main-background-region-corner-radius";
    case kNewTabButtonLeadingMargin:
      return "--new-tab-button-leading-margin";
    case kPageInfoIconSize:
      return "--page-info-icon-size";
    case kStarRatingIconSize:
      return "--star-rating-icon-size";
    case kTabAfterTitlePadding:
      return "--tab-after-title-padding";
    case kTabAlertIndicatorCaptureIconWidth:
      return "--tab-alert-indicator-capture-icon-width";
    case kTabAlertIndicatorIconWidth:
      return "--tab-alert-indicator-icon-width";
    case kTabCloseButtonSize:
      return "--tab-close-button-size";
    case kTabHeight:
      return "--tab-height";
    case kTabStripHeight:
      return "--tab-strip-height";
    case kTabStripPadding:
      return "--tab-strip-padding";
    case kTabSeparatorHeight:
      return "--tab-separator-height";
    case kTabPreTitlePadding:
      return "--tab-pre-title-padding";
    case kTabStackDistance:
      return "--tab-stack-distance";
    case kTabstripToolbarOverlap:
      return "--tabstrip-toolbar-overlap";
    case kToolbarButtonHeight:
      return "--toolbar-button-height";
    case kToolbarButtonIconSize:
      return "--toolbar-button-icon-size";
    case kToolbarDividerCornerRadius:
      return "--toolbar-divider-corner-radius";
    case kToolbarDividerHeight:
      return "--toolbar-divider-height";
    case kToolbarDividerSpacing:
      return "--toolbar-divider-spacing";
    case kToolbarDividerWidth:
      return "--toolbar-divider-width";
    case kToolbarElementPadding:
      return "--toolbar-element-padding";
    case kToolbarIconDefaultMargin:
      return "--toolbar-icon-default-margin";
    case kToolbarCornerRadius:
      return "--toolbar-corner-radius";
    case kToolbarHeightSidePanelInset:
      return "--toolbar-height-side-panel-inset";
    case kVerticalTabCornerRadius:
      return "--vertical-tab-corner-radius";
    case kVerticalTabHeight:
      return "--vertical-tab-height";
    case kVerticalTabPinnedHeight:
      return "--vertical-tab-pinned-height";
    case kVerticalTabMinWidth:
      return "--vertical-tab-min-width";
    case kVerticalTabPinnedBorderThickness:
      return "--vertical-tab-pinned-border-thickness";
    case kVerticalTabStripUncollapsedPadding:
      return "--vertical-tab-strip-uncollapsed-padding";
    case kVerticalTabStripCollapsedPadding:
      return "--vertical-tab-strip-collapsed-padding";
    case kVerticalTabStripCollapsedSeparatorWidth:
      return "--vertical-tab-strip-collapsed-separator-width";
    case kVerticalTabStripTopButtonIconSize:
      return "--vertical-tab-strip-top-button-icon-size";
    case kVerticalTabStripTopButtonPadding:
      return "--vertical-tab-strip-top-button-padding";
    case kVerticalTabStripBottomButtonIconSize:
      return "--vertical-tab-strip-bottom-button-icon-size";
    case kVerticalTabStripFlatEdgeButtonPadding:
      return "--vertical-tab-strip-flat-edge-button-padding";
    case kVerticalTabStripTopButtonContainerHeight:
      return "--vertical-tab-strip-top-button-container-height";
    case kVerticalTabStripNewTabButtonSize:
      return "--vertical-tab-strip-new-tab-button-size";
    case kVerticalTabStripTopContainerButtonSize:
      return "--vertical-tab-strip-top-container-button-size";
    case kWebAppMenuButtonSize:
      return "--web-app-menu-button-size";
    case kWebAppPageActionIconSize:
      return "--web-app-page-action-icon-size";
  }
  NOTREACHED();
}

}  // namespace

// static
std::string WebUIToolbarLayoutCssHelper::GenerateLayoutConstantsCss() {
  std::string css_string;
  // At time this was implemented, actual usage was about 2.3K
  css_string.reserve(3 * 1024);
  css_string.append(":host{");

  for (int layout_constant_num = 0;
       layout_constant_num <= static_cast<int>(LayoutConstant::kLast);
       ++layout_constant_num) {
    LayoutConstant layout_constant =
        static_cast<LayoutConstant>(layout_constant_num);
    base::StrAppend(
        &css_string,
        {LayoutConstantToCssVarName(layout_constant), ":",
         base::NumberToString(GetLayoutConstant(layout_constant)), "px;"});
  }

  css_string.push_back('}');
  return css_string;
}

// static
bool WebUIToolbarLayoutCssHelper::ShouldHandleRequest(const std::string& path) {
  // We expect URLs of form layout_constants[version suffix].css. This form
  // is used because query params don't play well with
  // LocalResourceLoaderConfig.
  return base::StartsWith(path, kPathPrefix) && base::EndsWith(path, ".css");
}

// static
void WebUIToolbarLayoutCssHelper::HandleRequest(
    const std::string& path,
    content::WebUIDataSource::GotDataCallback callback) {
  std::move(callback).Run(base::MakeRefCounted<base::RefCountedString>(
      GenerateLayoutConstantsCss()));
}

// static
void WebUIToolbarLayoutCssHelper::SetAsRequestFilter(
    content::WebUIDataSource* source) {
  source->SetRequestFilter(
      base::BindRepeating(&WebUIToolbarLayoutCssHelper::ShouldHandleRequest),
      base::BindRepeating(&WebUIToolbarLayoutCssHelper::HandleRequest));
}

// static
void WebUIToolbarLayoutCssHelper::PopulateLocalResourceLoaderConfig(
    blink::mojom::LocalResourceLoaderConfig* config) {
  std::string layout_css = GenerateLayoutConstantsCss();
  url::Origin layout_css_origin =
      url::Origin::Create(GURL(chrome::kChromeUIWebUIToolbarURL));
  auto& source = config->sources[layout_css_origin];
  if (source.is_null()) {
    source = blink::mojom::LocalResourceSource::New();
  }

  source->path_to_resource_map[kPathV0] =
      blink::mojom::LocalResourceValue::NewResponseBody(std::move(layout_css));
}
