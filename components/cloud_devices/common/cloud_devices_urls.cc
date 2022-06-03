// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cloud_devices/common/cloud_devices_urls.h"

#include "base/command_line.h"
#include "base/strings/string_util.h"
#include "components/cloud_devices/common/cloud_devices_switches.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/base/url_util.h"

namespace cloud_devices {

const char kCloudPrintAuthScope[] =
    "https://www.googleapis.com/auth/cloudprint";

namespace {

// Url must not be matched by "urls" section of
// cloud_print_app/manifest.json. If it's matched, print driver dialog will
// open sign-in page in separate window.
const char kCloudPrintURL[] = "https://www.google.com/cloudprint";

}

// Returns the root service URL for the cloud print service.  The default is to
// point at the Google Cloud Print service.  This can be overridden by the
// command line or by the user preferences.
GURL GetCloudPrintURL() {
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  GURL cloud_print_url(
      command_line->GetSwitchValueASCII(switches::kCloudPrintURL));
  if (cloud_print_url.is_empty())
    cloud_print_url = GURL(kCloudPrintURL);
  return cloud_print_url;
}

GURL GetCloudPrintRelativeURL(const std::string& relative_path) {
  GURL url = GetCloudPrintURL();
  std::string path;
  static const char kURLPathSeparator[] = "/";
  base::TrimString(url.path(), kURLPathSeparator, &path);
  std::string trimmed_path;
  base::TrimString(relative_path, kURLPathSeparator, &trimmed_path);
  path += kURLPathSeparator;
  path += trimmed_path;
  GURL::Replacements replacements;
  replacements.SetPathStr(path);
  return url.ReplaceComponents(replacements);
}

GURL GetCloudPrintAddAccountURL() {
  GURL url(GaiaUrls::GetInstance()->add_account_url());
  url = net::AppendQueryParameter(url, "service", "cloudprint");
  url = net::AppendQueryParameter(url, "sarp", "1");
  std::string continue_str = GetCloudPrintURL().spec();
  url = net::AppendQueryParameter(url, "continue", continue_str);
  return url;
}

GURL GetCloudPrintEnableURL(const std::string& proxy_id) {
  GURL url = GetCloudPrintRelativeURL("enable_chrome_connector/enable.html");
  url = net::AppendQueryParameter(url, "proxy", proxy_id);
  return url;
}

GURL GetCloudPrintEnableWithSigninURL(const std::string& proxy_id) {
  GURL url(GaiaUrls::GetInstance()->service_login_url());
  url = net::AppendQueryParameter(url, "service", "cloudprint");
  url = net::AppendQueryParameter(url, "sarp", "1");
  std::string continue_str = GetCloudPrintEnableURL(proxy_id).spec();
  return net::AppendQueryParameter(url, "continue", continue_str);
}

GURL GetCloudPrintManageDeviceURL(const std::string& device_id) {
  std::string ref = "printers/" + device_id;
  GURL::Replacements replacements;
  replacements.SetRefStr(ref);
  return GetCloudPrintURL().ReplaceComponents(replacements);
}

GURL GetCloudPrintPrintersURL() {
  GURL::Replacements replacements;
  replacements.SetRefStr("printers");
  return GetCloudPrintURL().ReplaceComponents(replacements);
}

GURL GetCloudPrintSigninURL() {
  GURL url(GaiaUrls::GetInstance()->service_login_url());
  url = net::AppendQueryParameter(url, "service", "cloudprint");
  url = net::AppendQueryParameter(url, "sarp", "1");
  std::string continue_str = GetCloudPrintURL().spec();
  url = net::AppendQueryParameter(url, "continue", continue_str);
  return url;
}

bool IsCloudPrintURL(const GURL& url) {
  const GURL& cloud_print_url = GetCloudPrintURL();
  return url.host_piece() == cloud_print_url.host_piece() &&
         url.scheme_piece() == cloud_print_url.scheme_piece() &&
         base::StartsWith(url.path(), cloud_print_url.path(),
                          base::CompareCase::SENSITIVE);
}

}  // namespace cloud_devices
