// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/core/viewer.h"

#include <memory>
#include <string>
#include <vector>

#include "base/json/json_writer.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "components/dom_distiller/core/distilled_page_prefs.h"
#include "components/dom_distiller/core/dom_distiller_service.h"
#include "components/dom_distiller/core/experiments.h"
#include "components/dom_distiller/core/proto/distilled_article.pb.h"
#include "components/dom_distiller/core/proto/distilled_page.pb.h"
#include "components/dom_distiller/core/task_tracker.h"
#include "components/dom_distiller/core/url_constants.h"
#include "components/dom_distiller/core/url_utils.h"
#include "components/grit/components_resources.h"
#include "components/strings/grit/components_strings.h"
#include "net/base/escape.h"
#include "net/url_request/url_request.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "url/gurl.h"

namespace dom_distiller {
namespace viewer {

namespace {

// JS Themes. Must agree with useTheme() in dom_distiller_viewer.js.
const char kDarkJsTheme[] = "dark";
const char kLightJsTheme[] = "light";
const char kSepiaJsTheme[] = "sepia";

// CSS Theme classes.  Must agree with classes in distilledpage.css.
const char kDarkCssClass[] = "dark";
const char kLightCssClass[] = "light";
const char kSepiaCssClass[] = "sepia";

// JS FontFamilies. Must agree with useFontFamily() in dom_distiller_viewer.js.
const char kSerifJsFontFamily[] = "serif";
const char kSansSerifJsFontFamily[] = "sans-serif";
const char kMonospaceJsFontFamily[] = "monospace";

// CSS FontFamily classes.  Must agree with classes in distilledpage.css.
const char kSerifCssClass[] = "serif";
const char kSansSerifCssClass[] = "sans-serif";
const char kMonospaceCssClass[] = "monospace";

std::string GetPlatformSpecificCss() {
#if defined(OS_IOS)
  return base::StrCat(
      {ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
           IDR_DISTILLER_MOBILE_CSS),
       ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
           IDR_DISTILLER_IOS_CSS)});
#elif defined(OS_ANDROID)
  return ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
      IDR_DISTILLER_MOBILE_CSS);
#else  // Desktop
  return ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
      IDR_DISTILLER_DESKTOP_CSS);
#endif
}

// Maps themes to JS themes.
const std::string GetJsTheme(DistilledPagePrefs::Theme theme) {
  if (theme == DistilledPagePrefs::THEME_DARK)
    return kDarkJsTheme;
  if (theme == DistilledPagePrefs::THEME_SEPIA)
    return kSepiaJsTheme;
  return kLightJsTheme;
}

// Maps themes to CSS classes.
const std::string GetThemeCssClass(DistilledPagePrefs::Theme theme) {
  if (theme == DistilledPagePrefs::THEME_DARK)
    return kDarkCssClass;
  if (theme == DistilledPagePrefs::THEME_SEPIA)
    return kSepiaCssClass;
  return kLightCssClass;
}

// Maps font families to JS font families.
const std::string GetJsFontFamily(DistilledPagePrefs::FontFamily font_family) {
  if (font_family == DistilledPagePrefs::FONT_FAMILY_SERIF)
    return kSerifJsFontFamily;
  if (font_family == DistilledPagePrefs::FONT_FAMILY_MONOSPACE)
    return kMonospaceJsFontFamily;
  return kSansSerifJsFontFamily;
}

// Maps fontFamilies to CSS fontFamily classes.
const std::string GetFontCssClass(DistilledPagePrefs::FontFamily font_family) {
  if (font_family == DistilledPagePrefs::FONT_FAMILY_SERIF)
    return kSerifCssClass;
  if (font_family == DistilledPagePrefs::FONT_FAMILY_MONOSPACE)
    return kMonospaceCssClass;
  return kSansSerifCssClass;
}

void EnsureNonEmptyContent(std::string* content) {
  UMA_HISTOGRAM_BOOLEAN("DomDistiller.PageHasDistilledData", !content->empty());
  if (content->empty()) {
    *content =
        l10n_util::GetStringUTF8(IDS_DOM_DISTILLER_VIEWER_NO_DATA_CONTENT);
  }
}

std::string ReplaceHtmlTemplateValues(
    const std::string& original_url,
    const DistilledPagePrefs::Theme theme,
    const DistilledPagePrefs::FontFamily font_family) {
  std::string html_template =
      ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
          IDR_DOM_DISTILLER_VIEWER_HTML);
  std::vector<std::string> substitutions;

  std::ostringstream css;
  std::ostringstream svg;
#if defined(OS_IOS)
  // On iOS the content is inlined as there is no API to detect those requests
  // and return the local data once a page is loaded.
  css << "<style>" << viewer::GetCss() << "</style>";
  svg << viewer::GetLoadingImage();
#else
  css << "<link rel=\"stylesheet\" href=\"/" << kViewerCssPath << "\">";
  svg << "<img src=\"/" << kViewerLoadingImagePath << "\">";
#endif  // defined(OS_IOS)

  substitutions.push_back(
      l10n_util::GetStringUTF8(IDS_DOM_DISTILLER_VIEWER_LOADING_TITLE));  // $1

  substitutions.push_back(css.str());  // $2
  substitutions.push_back(GetThemeCssClass(theme) + " " +
                          GetFontCssClass(font_family));  // $3

  substitutions.push_back(
      l10n_util::GetStringUTF8(IDS_DOM_DISTILLER_VIEWER_LOADING_TITLE));  // $4
  substitutions.push_back(l10n_util::GetStringUTF8(
      IDS_DOM_DISTILLER_JAVASCRIPT_DISABLED_CONTENT));  // $5

  substitutions.push_back(svg.str());  // $6

  substitutions.push_back(original_url);  // $7
  substitutions.push_back(l10n_util::GetStringUTF8(
      IDS_DOM_DISTILLER_VIEWER_CLOSE_READER_VIEW));  // $8

  return base::ReplaceStringPlaceholders(html_template, substitutions, nullptr);
}

}  // namespace

const std::string GetUnsafeIncrementalDistilledPageJs(
    const DistilledPageProto* page_proto,
    bool is_last_page) {
  std::string output(page_proto->html());
  EnsureNonEmptyContent(&output);
  base::Value value(output);
  base::JSONWriter::Write(value, &output);
  std::string page_update("addToPage(");
  page_update += output + ");";
  return page_update + GetToggleLoadingIndicatorJs(is_last_page);
}

const std::string GetErrorPageJs() {
  std::string title(l10n_util::GetStringUTF8(
      IDS_DOM_DISTILLER_VIEWER_FAILED_TO_FIND_ARTICLE_TITLE));
  std::string page_update(GetSetTitleJs(title));

  base::Value value(l10n_util::GetStringUTF8(
      IDS_DOM_DISTILLER_VIEWER_FAILED_TO_FIND_ARTICLE_CONTENT));
  std::string output;
  base::JSONWriter::Write(value, &output);
  page_update += "addToPage(" + output + ");";
  page_update += GetSetTextDirectionJs(std::string("auto"));
  page_update += GetToggleLoadingIndicatorJs(true);
  return page_update;
}

const std::string GetSetTitleJs(std::string title) {
  base::Value value(title);
  std::string output;
  base::JSONWriter::Write(value, &output);
  return "setTitle(" + output + ");";
}

const std::string GetSetTextDirectionJs(const std::string& direction) {
  base::Value value(direction);
  std::string output;
  base::JSONWriter::Write(value, &output);
  return "setTextDirection(" + output + ");";
}

const std::string GetToggleLoadingIndicatorJs(bool is_last_page) {
  if (is_last_page)
    return "showLoadingIndicator(true);";
  return "showLoadingIndicator(false);";
}

const std::string GetUnsafeArticleTemplateHtml(
    const std::string& original_url,
    DistilledPagePrefs::Theme theme,
    DistilledPagePrefs::FontFamily font_family) {
  return ReplaceHtmlTemplateValues(original_url, theme, font_family);
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

  std::string output(unsafe_output_stream.str());
  EnsureNonEmptyContent(&output);
  base::JSONWriter::Write(base::Value(output), &output);
  std::string page_update("addToPage(");
  page_update += output + ");";
  return page_update + GetToggleLoadingIndicatorJs(true);
}

const std::string GetCss() {
  return base::StrCat(
      {ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
           IDR_DISTILLER_CSS),
       GetPlatformSpecificCss()});
}

const std::string GetLoadingImage() {
  return ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
      IDR_DISTILLER_LOADING_IMAGE);
}

const std::string GetJavaScript() {
  return ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
      IDR_DOM_DISTILLER_VIEWER_JS);
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
    return std::unique_ptr<ViewerHandle>();
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
  return std::unique_ptr<ViewerHandle>();
}

const std::string GetDistilledPageThemeJs(DistilledPagePrefs::Theme theme) {
  return "useTheme('" + GetJsTheme(theme) + "');";
}

const std::string GetDistilledPageFontFamilyJs(
    DistilledPagePrefs::FontFamily font_family) {
  return "useFontFamily('" + GetJsFontFamily(font_family) + "');";
}

const std::string GetDistilledPageFontScalingJs(float scaling) {
  return "useFontScaling(" + base::NumberToString(scaling) + ");";
}

}  // namespace viewer
}  // namespace dom_distiller
