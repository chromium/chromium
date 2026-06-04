// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/webui_toolbar/webui_toolbar_layout_css_helper.h"

#include <string_view>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/memory/ref_counted_memory.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/location_bar/icon_label_bubble_view.h"
#include "chrome/common/webui_url_constants.h"
#include "ui/base/pointer/touch_ui_controller.h"
#include "ui/gfx/animation/animation.h"
#include "ui/views/style/typography_provider.h"

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
    case kAppMenuButtonImageLabelPadding:
      return "--app-menu-button-image-label-padding";
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
    case kLocationBarChildInternalSpacing:
      return "--location-bar-child-internal-spacing";
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
    case kLocationBarPageInfoIconLabelExtraTrailingPadding:
      return "--location-bar-page-info-icon-label-extra-trailing-padding";
    case kLocationBarPageInfoIconDangerousLeadingPadding:
      return "--location-bar-page-info-icon-dangerous-leading-padding";
    case kLocationBarPageInfoIconDangerousTrailingPadding:
      return "--location-bar-page-info-icon-dangerous-trailing-padding";
    case kLocationBarTrailingDecorationEdgePadding:
      return "--location-bar-trailing-decoration-edge-padding";
    case kLocationBarTrailingDecorationInnerPadding:
      return "--location-bar-trailing-decoration-inner-padding";
    case kLocationBarIconSize:
      return "--location-bar-icon-size";
    case kLocationBarIconLabelBubbleSpaceBesideSeparator:
      return "--location-bar-icon-label-bubble-space-beside-separator";
    case kLocationBarIconLabelBubbleSeparatorWidth:
      return "--location-bar-icon-label-bubble-separator-width";
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
    case kSidePanelInset:
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
    case kVerticalTabStripHorizontalPadding:
      return "--vertical-tab-strip-horizontal-padding";
    case kVerticalTabStripUncollapsedVerticalPadding:
      return "--vertical-tab-strip-uncollapsed-vertical-padding";
    case kVerticalTabStripCollapsedVerticalPadding:
      return "--vertical-tab-strip-collapsed-vertical-padding";
    case kVerticalTabStripComboButtonIconSize:
      return "--vertical-tab-strip-combo-button-icon-size";
    case kVerticalTabStripButtonIconSize:
      return "--vertical-tab-strip-button-icon-size";
    case kVerticalTabStripTopButtonPadding:
      return "--vertical-tab-strip-top-button-padding";
    case kVerticalTabStripFlatEdgeButtonPadding:
      return "--vertical-tab-strip-flat-edge-button-padding";
    case kVerticalTabStripTopButtonContainerHeight:
      return "--vertical-tab-strip-top-button-container-height";
    case kVerticalTabStripNewTabButtonSize:
      return "--vertical-tab-strip-new-tab-button-size";
    case kVerticalTabStripCollapseButtonSize:
      return "--vertical-tab-strip-collapse-button-size";
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
  // At the time of this update, actual usage was about 3.7K.
  css_string.reserve(5 * 1024);

  // Add some boolean flags.
  css_string.append(
      "@property --touch-mode {\n"
      "  syntax: \"<number>\";\n"
      "  inherits: true;\n"
      "  initial-value: 0;\n"
      "}\n");

  css_string.append(":host{");

  if (ui::TouchUiController::Get()->touch_ui()) {
    css_string.append("--touch-mode: 1;");
  } else {
    css_string.append("--touch-mode: 0;");
  }

  if (gfx::Animation::ShouldRenderRichAnimation()) {
    css_string.append("--animations-enabled: 1;");
  } else {
    css_string.append("--animations-enabled: 0;");
  }

  // Add LayoutConstant values.
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

  // Add insets.
  AddInsets("--location-bar-page-info-icon-padding",
            GetLayoutInsets(LOCATION_BAR_PAGE_INFO_ICON_PADDING), css_string);

  AddInsets("--location-bar-icon-interior-padding",
            GetLayoutInsets(LOCATION_BAR_ICON_INTERIOR_PADDING), css_string);

  // Add fonts.
  const auto& typography_provider = views::TypographyProvider::Get();
  AddFontVariables("--omnibox-primary", CONTEXT_OMNIBOX_PRIMARY,
                   views::style::STYLE_PRIMARY, typography_provider,
                   css_string);
  AddFontVariables("--omnibox-primary-body-3-emphasis", CONTEXT_OMNIBOX_PRIMARY,
                   views::style::STYLE_BODY_3_EMPHASIS, typography_provider,
                   css_string);
  AddFontVariables("--omnibox-chip", CONTEXT_OMNIBOX_PRIMARY,
                   views::style::STYLE_BODY_4_EMPHASIS, typography_provider,
                   css_string);
  AddFontVariables("--permission-chip", views::style::CONTEXT_BUTTON_MD,
                   views::style::STYLE_PRIMARY, typography_provider,
                   css_string);

  // Add durations.
  base::StrAppend(&css_string,
                  {"--duration-selected-keyword-separator-fade-in: ",
                   base::NumberToString(
                       IconLabelBubbleView::kIconLabelBubbleFadeInDurationMs),
                   "ms;"});

  base::StrAppend(&css_string,
                  {"--duration-selected-keyword-separator-fade-out: ",
                   base::NumberToString(
                       IconLabelBubbleView::kIconLabelBubbleFadeOutDurationMs),
                   "ms;"});

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

// static
std::string WebUIToolbarLayoutCssHelper::EscapeCssFontName(
    std::string_view in) {
  // References here: CSS3 Syntax 4.3.5 and 3.3
  std::string out;
  out.reserve(in.size());
  for (char c : in) {
    if (c == '"') {
      out += "\\\"";
    } else if (c == '\\') {
      out += "\\\\";
    } else if (c < 32) {
      // CSS pre-processing and parsing will change the meanings of CR, LF,
      // CR-LF and FF; but for simplicity we just escape everything before
      // space.
      out += '\\';
      base::AppendHexEncodedByte(c, out);
      out += ' ';
    } else {
      out += c;
    }
  }
  return out;
}

// static
void WebUIToolbarLayoutCssHelper::AddFontVariables(
    std::string_view prefix,
    int context,
    int style,
    const views::TypographyProvider& typography_provider,
    std::string& out) {
  const gfx::FontList& font = typography_provider.GetFont(context, style);
  DCHECK_EQ(1u, font.GetFonts().size());
  std::string_view font_family = font.GetPrimaryFont().GetFontName();
  // Convert internal Mac font name back to CSS name.
  if (font_family == ".AppleSystemUIFont") {
    font_family = "system-ui";
  }
  base::StrAppend(
      &out,
      // clang-format off
      {prefix, "-font-family:\"",
       EscapeCssFontName(font_family), "\";",
       prefix, "-font-size:", base::NumberToString(font.GetFontSize()), "px;",
       prefix, "-font-weight:",
       base::NumberToString(static_cast<int>(font.GetFontWeight())), ";",
       prefix, "-line-height:",
       base::NumberToString(typography_provider.GetLineHeight(context, style)),
       "px;"});
  // clang-format on
}

// static
void WebUIToolbarLayoutCssHelper::AddInsets(std::string_view prefix,
                                            const gfx::Insets& insets,
                                            std::string& css_string) {
  base::StrAppend(&css_string,
                  {prefix, "-top:", base::NumberToString(insets.top()), "px;"});
  base::StrAppend(
      &css_string,
      {prefix, "-bottom:", base::NumberToString(insets.bottom()), "px;"});
  base::StrAppend(
      &css_string,
      {prefix, "-left:", base::NumberToString(insets.left()), "px;"});
  base::StrAppend(
      &css_string,
      {prefix, "-right:", base::NumberToString(insets.right()), "px;"});
}
