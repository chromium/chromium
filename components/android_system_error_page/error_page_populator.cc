// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/android_system_error_page/error_page_populator.h"

#include "base/i18n/rtl.h"
#include "base/strings/escape.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/grit/components_resources.h"
#include "components/strings/grit/components_strings.h"
#include "net/base/net_errors.h"
#include "third_party/blink/public/platform/web_url_error.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "url/gurl.h"

namespace android_system_error_page {

namespace {
constexpr char kThrottledErrorDescription[] =
    "Request throttled. Visit http://dev.chromium.org/throttling for more "
    "information.";
}  // namespace

void PopulateErrorPageHtml(const blink::WebURLError& error,
                           std::string* error_html) {
  if (!error_html)
    return;

  std::string err;
  if (error.reason() == net::ERR_TEMPORARILY_THROTTLED)
    err = kThrottledErrorDescription;
  else
    err = net::ErrorToString(error.reason());

  // Create the error page based on the error reason.
  GURL gurl(error.url());
  std::string url_string = gurl.possibly_invalid_spec();
  int reason_id = IDS_ANDROID_ERROR_PAGE_WEBPAGE_CAN_NOT_BE_LOADED;

  if (err.empty())
    reason_id = IDS_ANDROID_ERROR_PAGE_WEBPAGE_TEMPORARILY_DOWN;

  std::vector<std::string> replacements;

  std::string escaped_url = base::EscapeForHTML(url_string);

  replacements.push_back(
      l10n_util::GetStringUTF8(IDS_ANDROID_ERROR_PAGE_WEBPAGE_NOT_AVAILABLE));
  replacements.push_back(
      l10n_util::GetStringFUTF8(reason_id, base::UTF8ToUTF16(escaped_url)));

  // Having chosen the base reason, chose what extra information to add.
  if (reason_id == IDS_ANDROID_ERROR_PAGE_WEBPAGE_TEMPORARILY_DOWN) {
    replacements.push_back(l10n_util::GetStringUTF8(
        IDS_ANDROID_ERROR_PAGE_WEBPAGE_TEMPORARILY_DOWN_SUGGESTIONS));
  } else {
    replacements.push_back(err);
  }
  if (base::i18n::IsRTL())
    replacements.push_back("direction: rtl;");
  else
    replacements.push_back("");
  *error_html = base::ReplaceStringPlaceholders(
      ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
          IDR_ANDROID_ERROR_PAGE_LOAD_ERROR_HTML),
      replacements, nullptr);
}

}  // namespace android_system_error_page
