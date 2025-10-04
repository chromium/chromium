// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/core/viewer.h"

#include <memory>
#include <string>
#include <vector>

#include "base/feature_list.h"
#include "base/json/json_writer.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/escape.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/to_string.h"
#include "build/blink_buildflags.h"
#include "build/build_config.h"
#include "components/dom_distiller/core/distilled_page_prefs.h"
#include "components/dom_distiller/core/dom_distiller_features.h"
#include "components/dom_distiller/core/dom_distiller_service.h"
#include "components/dom_distiller/core/experiments.h"
#include "components/dom_distiller/core/proto/distilled_article.pb.h"
#include "components/dom_distiller/core/proto/distilled_page.pb.h"
#include "components/dom_distiller/core/task_tracker.h"
#include "components/dom_distiller/core/url_constants.h"
#include "components/dom_distiller/core/url_utils.h"
#include "components/grit/components_resources.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/template_expressions.h"
#include "url/gurl.h"

namespace dom_distiller {
namespace viewer {

namespace {

// LINT.IfChange(JSThemesAndFonts)

// JS Themes. Must agree with themeClasses in dom_distiller_viewer.js.
const char kDarkJsTheme[] = "dark";
const char kLightJsTheme[] = "light";
const char kSepiaJsTheme[] = "sepia";

// JS FontFamilies. Must agree with fontFamilyClasses in
// dom_distiller_viewer.js.
const char kSerifJsFontFamily[] = "serif";
const char kSansSerifJsFontFamily[] = "sans-serif";
const char kMonospaceJsFontFamily[] = "monospace";

// LINT.ThenChange(//components/dom_distiller/core/javascript/dom_distiller_viewer.js:JSThemesAndFonts)

// LINT.IfChange

// CSS Theme classes.  Must agree with classes in distilledpage_common.css.
const char kDarkCssClass[] = "dark";
const char kLightCssClass[] = "light";
const char kSepiaCssClass[] = "sepia";

// CSS FontFamily classes.  Must agree with classes in distilledpage_common.css.
const char kSerifCssClass[] = "serif";
const char kSansSerifCssClass[] = "sans-serif";
const char kMonospaceCssClass[] = "monospace";

// LINT.ThenChange(//components/dom_distiller/core/css/distilledpage_common.css)

std::string GetVersionedCss() {
#if BUILDFLAG(IS_ANDROID)
  if (base::FeatureList::IsEnabled(dom_distiller::kReaderModeDistillInApp)) {
    return ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
        IDR_DISTILLER_NEW_CSS);
  }
#endif
#if BUILDFLAG(IS_IOS)
  if (base::FeatureList::IsEnabled(dom_distiller::kEnableReaderModeNewCss)) {
    return ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
        IDR_DISTILLER_NEW_CSS);
  }
#endif
  return ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
      IDR_DISTILLER_CSS);
}

std::string GetPlatformSpecificCss() {
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  return "";
#else  // Desktop
  return ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
      IDR_DISTILLER_DESKTOP_CSS);
#endif
}

// Maps themes to JS themes.
const std::string GetJsTheme(mojom::Theme theme) {
  if (theme == mojom::Theme::kDark)
    return kDarkJsTheme;
  if (theme == mojom::Theme::kSepia)
    return kSepiaJsTheme;
  return kLightJsTheme;
}

// Maps themes to CSS classes.
const std::string GetThemeCssClass(mojom::Theme theme) {
  if (theme == mojom::Theme::kDark)
    return kDarkCssClass;
  if (theme == mojom::Theme::kSepia)
    return kSepiaCssClass;
  return kLightCssClass;
}

// Maps font families to JS font families.
const std::string GetJsFontFamily(mojom::FontFamily font_family) {
  if (font_family == mojom::FontFamily::kSerif)
    return kSerifJsFontFamily;
  if (font_family == mojom::FontFamily::kMonospace)
    return kMonospaceJsFontFamily;
  return kSansSerifJsFontFamily;
}

// Maps fontFamilies to CSS fontFamily classes.
const std::string GetFontCssClass(mojom::FontFamily font_family) {
  if (font_family == mojom::FontFamily::kSerif)
    return kSerifCssClass;
  if (font_family == mojom::FontFamily::kMonospace)
    return kMonospaceCssClass;
  return kSansSerifCssClass;
}

const std::string EnsureNonEmptyContent(const std::string& content) {
  if (content.empty()) {
    return l10n_util::GetStringUTF8(IDS_DOM_DISTILLER_VIEWER_NO_DATA_CONTENT);
  }
  return content;
}

std::string ReplaceHtmlTemplateValues(const mojom::Theme theme,
                                      const mojom::FontFamily font_family,
                                      const std::string& csp_nonce,
                                      bool use_offline_data) {
  std::string html_template =
      ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
          IDR_DOM_DISTILLER_VIEWER_HTML);

  // Replace placeholders of the form $i18n{foo} with translated strings
  // using ReplaceTemplateExpressions. Do this step first because
  // ReplaceStringPlaceholders, below, considers $i18n to be an error.
  ui::TemplateReplacements i18n_replacements;
  i18n_replacements["title"] =
      l10n_util::GetStringUTF8(IDS_DOM_DISTILLER_VIEWER_LOADING_TITLE);
  i18n_replacements["customizeAppearance"] =
      l10n_util::GetStringUTF8(IDS_DOM_DISTILLER_VIEWER_CUSTOMIZE_APPEARANCE);
  i18n_replacements["fontStyle"] =
      l10n_util::GetStringUTF8(IDS_DOM_DISTILLER_VIEWER_FONT_STYLE);
  i18n_replacements["sansSerifFont"] =
      l10n_util::GetStringUTF8(IDS_DOM_DISTILLER_VIEWER_SANS_SERIF_FONT);
  i18n_replacements["serifFont"] =
      l10n_util::GetStringUTF8(IDS_DOM_DISTILLER_VIEWER_SERIF_FONT);
  i18n_replacements["monospaceFont"] =
      l10n_util::GetStringUTF8(IDS_DOM_DISTILLER_VIEWER_MONOSPACE_FONT);
  i18n_replacements["pageColor"] =
      l10n_util::GetStringUTF8(IDS_DOM_DISTILLER_VIEWER_PAGE_COLOR);
  i18n_replacements["light"] =
      l10n_util::GetStringUTF8(IDS_DOM_DISTILLER_VIEWER_PAGE_COLOR_LIGHT);
  i18n_replacements["sepia"] =
      l10n_util::GetStringUTF8(IDS_DOM_DISTILLER_VIEWER_PAGE_COLOR_SEPIA);
  i18n_replacements["dark"] =
      l10n_util::GetStringUTF8(IDS_DOM_DISTILLER_VIEWER_PAGE_COLOR_DARK);
  i18n_replacements["fontSize"] =
      l10n_util::GetStringUTF8(IDS_DOM_DISTILLER_VIEWER_FONT_SIZE);
  i18n_replacements["small"] =
      l10n_util::GetStringUTF8(IDS_DOM_DISTILLER_VIEWER_FONT_SIZE_SMALL);
  i18n_replacements["large"] =
      l10n_util::GetStringUTF8(IDS_DOM_DISTILLER_VIEWER_FONT_SIZE_LARGE);
  i18n_replacements["close"] =
      l10n_util::GetStringUTF8(IDS_DOM_DISTILLER_VIEWER_CLOSE);

  html_template =
      ui::ReplaceTemplateExpressions(html_template, i18n_replacements);

  // There shouldn't be any unsubstituted i18n placeholders left.
  DCHECK_EQ(html_template.find("$i18n"), std::string::npos);

  // Now do other non-i18n string replacements.
  std::vector<std::string> substitutions;

  std::ostringstream csp;
  std::ostringstream css;
  std::ostringstream svg;
#if BUILDFLAG(IS_IOS) && !BUILDFLAG(USE_BLINK)
  // On iOS the content is inlined as there is no API to detect those requests
  // and return the local data once a page is loaded.
  css << "<style>" << viewer::GetCss() << "</style>";
  svg << viewer::GetLoadingImage();
#else
  css << "<link rel=\"stylesheet\" href=\"/" << kViewerCssPath << "\">";
  svg << "<img src=\"/" << kViewerLoadingImagePath << "\">";
#endif  // BUILDFLAG(IS_IOS) && !BUILDFLAG(USE_BLINK)

  if (use_offline_data) {
    // CSP policy to mitigate leaking of data from different origins.
    csp << "<meta http-equiv=\"Content-Security-Policy\" content=\"";
    csp << "default-src 'none'; ";
    csp << "script-src 'nonce-" << csp_nonce << "'; ";
    // YouTube videos are embedded as an iframe.
    csp << "frame-src http://www.youtube.com; ";
    csp << "style-src 'unsafe-inline' https://fonts.googleapis.com; ";
    // Allows the fallback font-face from the main stylesheet.
    csp << "font-src https://fonts.gstatic.com; ";
    // Images will be inlined as data-uri if they are valid.
    csp << "img-src data:; ";
    csp << "form-action 'none'; ";
    csp << "base-uri 'none'; ";
    csp << "\">";
  }
  substitutions.push_back(csp.str());  // $1
  substitutions.push_back(css.str());  // $2
  substitutions.push_back(GetThemeCssClass(theme) + " " +
                          GetFontCssClass(font_family));  // $3

  substitutions.push_back(l10n_util::GetStringUTF8(
      IDS_DOM_DISTILLER_JAVASCRIPT_DISABLED_CONTENT));  // $4

  substitutions.push_back(svg.str());  // $5

  return base::ReplaceStringPlaceholders(html_template, substitutions, nullptr);
}

}  // namespace

const std::string GetUnsafeIncrementalDistilledPageJs(
    const DistilledPageProto* page_proto,
    bool is_last_page) {
  return GetAddToPageJs(page_proto->html()) +
         GetToggleLoadingIndicatorJs(is_last_page);
}

const std::string GetErrorPageJs() {
  std::string title(l10n_util::GetStringUTF8(
      IDS_DOM_DISTILLER_VIEWER_FAILED_TO_FIND_ARTICLE_TITLE));
  std::string page_update(GetSetTitleJs(title));

  page_update += GetAddToPageJs(l10n_util::GetStringUTF8(
      IDS_DOM_DISTILLER_VIEWER_FAILED_TO_FIND_ARTICLE_CONTENT));
  page_update += GetSetTextDirectionJs(std::string("auto"));
  page_update += GetToggleLoadingIndicatorJs(true);
  return page_update;
}

const std::string GetSetTitleJs(std::string title) {
#if BUILDFLAG(IS_IOS)
  base::Value suffix_value("");
#else  // Desktop and Android.
  std::string suffix(
      l10n_util::GetStringUTF8(IDS_DOM_DISTILLER_VIEWER_TITLE_SUFFIX));
  base::Value suffix_value(" - " + suffix);
#endif
  base::Value title_value(title);
  std::string suffix_js = base::WriteJson(suffix_value).value_or("");
  std::string title_js = base::WriteJson(title_value).value_or("");
  return "setTitle(" + title_js + ", " + suffix_js + ");";
}

const std::string GetSetTextDirectionJs(const std::string& direction) {
  base::Value value(direction);
  std::string output = base::WriteJson(value).value_or("");
  return "setTextDirection(" + output + ");";
}

const std::string GetToggleLoadingIndicatorJs(bool is_last_page) {
  if (is_last_page)
    return "showLoadingIndicator(true);";
  return "showLoadingIndicator(false);";
}

const std::string GetArticleTemplateHtml(mojom::Theme theme,
                                         mojom::FontFamily font_family,
                                         const std::string& csp_nonce,
                                         bool use_offline_data) {
  return ReplaceHtmlTemplateValues(theme, font_family, csp_nonce,
                                   use_offline_data);
}

const std::string GetUnsafeArticleContentJs(
    const DistilledArticleProto* article_proto) {
  DCHECK(article_proto);
  std::ostringstream unsafe_output_stream;
  if (article_proto->pages_size() > 0 && article_proto->pages(0).has_html()) {
    for (int page_num = 0; page_num < article_proto->pages_size(); ++page_num) {
      unsafe_output_stream << article_proto->pages(page_num).html();
    }
  }

  return GetAddToPageJs(unsafe_output_stream.str()) +
         GetToggleLoadingIndicatorJs(true);
}

const std::string GetAddToPageJs(const std::string& unsafe_content) {
  std::string output =
      base::WriteJson(base::Value(EnsureNonEmptyContent(unsafe_content)))
          .value_or("");
  return "addToPage(" + output + ");";
}

const std::string GetCss() {
  return base::StrCat(
      {ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
           IDR_DISTILLER_COMMON_CSS),
       GetVersionedCss(), GetPlatformSpecificCss()});
}

const std::string GetLoadingImage() {
  return ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
      IDR_DISTILLER_LOADING_IMAGE);
}

static std::string GetMinPinchZoomScale() {
  float min_scale = kMinFontScaleAndroidCCT;
#if BUILDFLAG(IS_ANDROID)
  // Make the minimum pinch zoom value to be 1.0 for distillation in app to
  // align with prefs UI.
  if (base::FeatureList::IsEnabled(kReaderModeDistillInApp)) {
    min_scale = kMinFontScaleAndroidInApp;
  }
#endif
  return base::NumberToString(min_scale);
}

static std::string GetMaxPinchZoomScale() {
  float max_scale = kMaxFontScaleAndroidCCT;
#if BUILDFLAG(IS_ANDROID)
  // Make the maximum pinch zoom value to be 2.5 for distillation in app to
  // align with prefs UI.
  if (base::FeatureList::IsEnabled(kReaderModeDistillInApp)) {
    max_scale = kMaxFontScaleAndroidInApp;
  }
#endif
  return base::NumberToString(max_scale);
}

const std::string GetJavaScript() {
  std::string js =
      ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
          IDR_DOM_DISTILLER_VIEWER_JS);
  base::ReplaceFirstSubstringAfterOffset(&js, 0, "$MIN_SCALE",
                                         GetMinPinchZoomScale());
  base::ReplaceFirstSubstringAfterOffset(&js, 0, "$MAX_SCALE",
                                         GetMaxPinchZoomScale());
  return js;
}

std::unique_ptr<ViewerHandle> CreateViewRequest(
    DomDistillerServiceInterface* dom_distiller_service,
    const GURL& url,
    ViewRequestDelegate* view_request_delegate,
    const gfx::Size& render_view_size) {
  if (!url_utils::IsDistilledPage(url)) {
    return nullptr;
  }
  std::string entry_id = url_utils::GetValueForKeyInUrl(url, kEntryIdKey);
  bool has_valid_entry_id = !entry_id.empty();
  entry_id = base::ToUpperASCII(entry_id);

  GURL requested_url(url_utils::GetOriginalUrlFromDistillerUrl(url));
  bool has_valid_url = url_utils::IsUrlDistillable(requested_url);

  if (has_valid_entry_id && has_valid_url) {
    // It is invalid to specify a query param for both |kEntryIdKey| and
    // |kUrlKey|.
    return nullptr;
  }

  if (has_valid_entry_id) {
    return nullptr;
  }
  if (has_valid_url) {
    return dom_distiller_service->ViewUrl(
        view_request_delegate,
        dom_distiller_service->CreateDefaultDistillerPage(render_view_size),
        requested_url);
  }

  // It is invalid to not specify a query param for |kEntryIdKey| or |kUrlKey|.
  return nullptr;
}

const std::string GetDistilledPageThemeJs(mojom::Theme theme) {
  return "useTheme('" + GetJsTheme(theme) + "');";
}

const std::string GetDistilledPageFontFamilyJs(mojom::FontFamily font_family) {
  return "useFontFamily('" + GetJsFontFamily(font_family) + "');";
}

const std::string GetDistilledPageFontScalingJs(float scaling,
                                                bool restore_center) {
  return "useFontScaling(" + base::NumberToString(scaling) + ", " +
         base::ToString(restore_center) + ");";
}

const std::string SetDistilledPageBaseFontSize(float baseFontSize) {
  return "useBaseFontSize(" + base::NumberToString(baseFontSize) + ");";
}

}  // namespace viewer
}  // namespace dom_distiller
