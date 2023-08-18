// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/new_tab_page/untrusted_source.h"

#include <string>
#include <utility>

#include "base/base64.h"
#include "base/files/file_util.h"
#include "base/i18n/rtl.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_piece.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/new_tab_page/one_google_bar/one_google_bar_data.h"
#include "chrome/browser/new_tab_page/one_google_bar/one_google_bar_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/search/ntp_user_data_logger.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/new_tab_page_resources.h"
#include "components/search/ntp_features.h"
#include "content/public/common/url_constants.h"
#include "net/base/url_util.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/re2/src/re2/re2.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/template_expressions.h"
#include "url/url_util.h"

namespace {

constexpr int kMaxUriDecodeLen = 2048;

std::string FormatTemplate(int resource_id,
                           const ui::TemplateReplacements& replacements) {
  ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
  scoped_refptr<base::RefCountedMemory> bytes =
      bundle.LoadDataResourceBytes(resource_id);
  base::StringPiece string_piece(reinterpret_cast<const char*>(bytes->front()),
                                 bytes->size());
  return ui::ReplaceTemplateExpressions(
      string_piece, replacements,
      /* skip_unexpected_placeholder_check= */ true);
}

std::string ReadBackgroundImageData(const base::FilePath& profile_path) {
  std::string data_string;
  base::ReadFileToString(profile_path.AppendASCII("background.jpg"),
                         &data_string);
  return data_string;
}

void ServeBackgroundImageData(content::URLDataSource::GotDataCallback callback,
                              std::string data_string) {
  std::move(callback).Run(
      base::MakeRefCounted<base::RefCountedString>(std::move(data_string)));
}

}  // namespace

UntrustedSource::UntrustedSource(Profile* profile)
    : one_google_bar_service_(
          OneGoogleBarServiceFactory::GetForProfile(profile)),
      profile_(profile) {
  // |one_google_bar_service_| is null in incognito, or when the feature is
  // disabled.
  if (one_google_bar_service_) {
    one_google_bar_service_observation_.Observe(one_google_bar_service_.get());
  }
}

UntrustedSource::~UntrustedSource() = default;

std::string UntrustedSource::GetContentSecurityPolicy(
    network::mojom::CSPDirectiveName directive) {
  switch (directive) {
    case network::mojom::CSPDirectiveName::ScriptSrc:
      return "script-src 'self' 'unsafe-inline' https:;";
    case network::mojom::CSPDirectiveName::ChildSrc:
      return "child-src https:;";
    case network::mojom::CSPDirectiveName::DefaultSrc:
      // TODO(https://crbug.com/1085325): Audit and tighten CSP.
      return std::string();
    case network::mojom::CSPDirectiveName::FrameAncestors:
      return base::StringPrintf("frame-ancestors %s",
                                chrome::kChromeUINewTabPageURL);
    case network::mojom::CSPDirectiveName::RequireTrustedTypesFor:
      return std::string();
    case network::mojom::CSPDirectiveName::TrustedTypes:
      return std::string();
    case network::mojom::CSPDirectiveName::FormAction:
      return "form-action https://ogs.google.com https://*.corp.google.com;";
    default:
      return content::URLDataSource::GetContentSecurityPolicy(directive);
  }
}

std::string UntrustedSource::GetSource() {
  return chrome::kChromeUIUntrustedNewTabPageUrl;
}

void UntrustedSource::StartDataRequest(
    const GURL& url,
    const content::WebContents::Getter& wc_getter,
    content::URLDataSource::GotDataCallback callback) {
  const std::string path = url.has_path() ? url.path().substr(1) : "";
  GURL url_param = GURL(url.query());
  if (path == "one-google-bar" && one_google_bar_service_) {
    std::string query_params;
    net::GetValueForKeyInQuery(url, "paramsencoded", &query_params);
    base::Base64Decode(query_params, &query_params);
    bool wait_for_refresh =
        one_google_bar_service_->SetAdditionalQueryParams(query_params);
    one_google_bar_callbacks_.push_back(std::move(callback));
    if (one_google_bar_service_->one_google_bar_data().has_value() &&
        !wait_for_refresh &&
        base::FeatureList::IsEnabled(ntp_features::kCacheOneGoogleBar)) {
      OnOneGoogleBarDataUpdated();
    }
    if (one_google_bar_callbacks_.size() == 1) {
      one_google_bar_load_start_time_ = base::TimeTicks::Now();
      one_google_bar_service_->Refresh();
    }
    return;
  }
  if (path == "one_google_bar.js") {
    ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
    std::move(callback).Run(bundle.LoadDataResourceBytes(
        IDR_NEW_TAB_PAGE_UNTRUSTED_ONE_GOOGLE_BAR_JS));
    return;
  }
  if (path == "image" && url_param.is_valid() &&
      (url_param.SchemeIs(url::kHttpsScheme) ||
       url_param.SchemeIs(content::kChromeUIUntrustedScheme))) {
    ui::TemplateReplacements replacements;
    replacements["url"] = url_param.spec();
    std::move(callback).Run(base::MakeRefCounted<base::RefCountedString>(
        FormatTemplate(IDR_NEW_TAB_PAGE_UNTRUSTED_IMAGE_HTML, replacements)));
    return;
  }
  if (path == "background_image") {
    ServeBackgroundImage(url_param, GURL(), "cover", "no-repeat", "no-repeat",
                         "center", "center", "inherit", std::move(callback));
    return;
  }
  if (path == "custom_background_image") {
    // Parse all query parameters to hash map and decode values.
    std::unordered_map<std::string, std::string> params;
    url::Component query(0, url.query().length());
    url::Component key, value;
    while (
        url::ExtractQueryKeyValue(url.query().c_str(), &query, &key, &value)) {
      url::RawCanonOutputW<kMaxUriDecodeLen> output;
      url::DecodeURLEscapeSequences(
          url.query().c_str() + value.begin, value.len,
          url::DecodeURLMode::kUTF8OrIsomorphic, &output);
      params.insert(
          {url.query().substr(key.begin, key.len),
           base::UTF16ToUTF8(std::u16string(output.data(), output.length()))});
    }
    // Extract desired values.
    ServeBackgroundImage(
        params.count("url") == 1 ? GURL(params["url"]) : GURL(),
        params.count("url2x") == 1 ? GURL(params["url2x"]) : GURL(),
        params.count("size") == 1 ? params["size"] : "cover",
        params.count("repeatX") == 1 ? params["repeatX"] : "no-repeat",
        params.count("repeatY") == 1 ? params["repeatY"] : "no-repeat",
        params.count("positionX") == 1 ? params["positionX"] : "center",
        params.count("positionY") == 1 ? params["positionY"] : "center", "none",
        std::move(callback));
    return;
  }
  if (path == "background_image.js") {
    ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
    std::move(callback).Run(bundle.LoadDataResourceBytes(
        IDR_NEW_TAB_PAGE_UNTRUSTED_BACKGROUND_IMAGE_JS));
    return;
  }
  if (path == "background.jpg") {
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::TaskPriority::USER_VISIBLE, base::MayBlock()},
        base::BindOnce(&ReadBackgroundImageData, profile_->GetPath()),
        base::BindOnce(&ServeBackgroundImageData, std::move(callback)));
    return;
  }
  std::move(callback).Run(base::MakeRefCounted<base::RefCountedString>());
}

std::string UntrustedSource::GetMimeType(const GURL& url) {
  const base::StringPiece stripped_path = url.path_piece();
  if (base::EndsWith(stripped_path, ".js",
                     base::CompareCase::INSENSITIVE_ASCII)) {
    return "application/javascript";
  }
  if (base::EndsWith(stripped_path, ".jpg",
                     base::CompareCase::INSENSITIVE_ASCII)) {
    return "image/jpg";
  }

  return "text/html";
}

bool UntrustedSource::AllowCaching() {
  return false;
}

bool UntrustedSource::ShouldReplaceExistingSource() {
  return false;
}

bool UntrustedSource::ShouldServeMimeTypeAsContentTypeHeader() {
  return true;
}

bool UntrustedSource::ShouldServiceRequest(
    const GURL& url,
    content::BrowserContext* browser_context,
    int render_process_id) {
  if (!url.SchemeIs(content::kChromeUIUntrustedScheme) || !url.has_path()) {
    return false;
  }
  const std::string path = url.path().substr(1);
  return path == "one-google-bar" || path == "one_google_bar.js" ||
         path == "image" || path == "background_image" ||
         path == "custom_background_image" || path == "background_image.js" ||
         path == "background.jpg";
}

void UntrustedSource::OnOneGoogleBarDataUpdated() {
  absl::optional<OneGoogleBarData> data =
      one_google_bar_service_->one_google_bar_data();

  if (one_google_bar_load_start_time_.has_value()) {
    NTPUserDataLogger::LogOneGoogleBarFetchDuration(
        /*success=*/data.has_value(),
        /*duration=*/base::TimeTicks::Now() - *one_google_bar_load_start_time_);
    one_google_bar_load_start_time_ = absl::nullopt;
  }

  std::string html;
  if (data.has_value()) {
    ui::TemplateReplacements replacements;
    replacements["textdirection"] = base::i18n::IsRTL() ? "rtl" : "ltr";
    replacements["barHtml"] = data->bar_html;
    replacements["inHeadScript"] = data->in_head_script;
    replacements["inHeadStyle"] = data->in_head_style;
    replacements["afterBarScript"] = data->after_bar_script;
    replacements["endOfBodyHtml"] = data->end_of_body_html;
    replacements["endOfBodyScript"] = data->end_of_body_script;

    html = FormatTemplate(IDR_NEW_TAB_PAGE_UNTRUSTED_ONE_GOOGLE_BAR_HTML,
                          replacements);
  }

  auto html_ref_counted =
      base::MakeRefCounted<base::RefCountedString>(std::move(html));
  for (auto& callback : one_google_bar_callbacks_) {
    std::move(callback).Run(html_ref_counted);
  }
  one_google_bar_callbacks_.clear();
}

void UntrustedSource::OnOneGoogleBarServiceShuttingDown() {
  one_google_bar_service_observation_.Reset();
  one_google_bar_service_ = nullptr;
}

void UntrustedSource::ServeBackgroundImage(
    const GURL& url,
    const GURL& url_2x,
    const std::string& size,
    const std::string& repeat_x,
    const std::string& repeat_y,
    const std::string& position_x,
    const std::string& position_y,
    const std::string& scrim_display,
    content::URLDataSource::GotDataCallback callback) {
  if (!url.is_valid() || !(url.SchemeIs(url::kHttpsScheme) ||
                           url.SchemeIs(content::kChromeUIUntrustedScheme))) {
    std::move(callback).Run(base::MakeRefCounted<base::RefCountedString>());
    return;
  }
  ui::TemplateReplacements replacements;
  replacements["url"] = url.spec();
  if (url_2x.is_valid()) {
    replacements["backgroundUrl"] =
        base::StringPrintf("-webkit-image-set(url(%s) 1x, url(%s) 2x)",
                           url.spec().c_str(), url_2x.spec().c_str());
  } else {
    replacements["backgroundUrl"] =
        base::StringPrintf("url(%s)", url.spec().c_str());
  }
  replacements["size"] = size;
  replacements["repeatX"] = repeat_x;
  replacements["repeatY"] = repeat_y;
  replacements["positionX"] = position_x;
  replacements["positionY"] = position_y;
  replacements["scrimDisplay"] = scrim_display;
  std::move(callback).Run(
      base::MakeRefCounted<base::RefCountedString>(FormatTemplate(
          IDR_NEW_TAB_PAGE_UNTRUSTED_BACKGROUND_IMAGE_HTML, replacements)));
}
