// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/chrome/browser_info.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <string_view>

#include "base/json/json_reader.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chrome/test/chromedriver/constants/version.h"

BrowserInfo::BrowserInfo() = default;

BrowserInfo::~BrowserInfo() = default;

BrowserInfo::BrowserInfo(const BrowserInfo&) = default;

BrowserInfo::BrowserInfo(BrowserInfo&&) = default;

BrowserInfo& BrowserInfo::operator=(const BrowserInfo&) = default;

BrowserInfo& BrowserInfo::operator=(BrowserInfo&&) = default;

Status BrowserInfo::FillFromBrowserVersionResponse(
    const base::Value::Dict& response) {
  const std::string* browser_string = response.FindString("product");
  if (!browser_string) {
    return Status(kUnknownError, "version doesn't include 'Browser'");
  }

  return ParseBrowserString(false, *browser_string, this);
}

Status BrowserInfo::ParseBrowserInfo(const std::string& data) {
  return ParseBrowserInfo(data, this);
}

Status BrowserInfo::ParseBrowserInfo(const std::string& data,
                                     BrowserInfo* browser_info) {
  std::optional<base::Value> value = base::JSONReader::Read(data);
  if (!value) {
    return Status(kUnknownError, "version info not in JSON");
  }

  auto* dict = value->GetIfDict();
  if (!dict) {
    return Status(kUnknownError, "version info not a dictionary");
  }

  const base::Value* android_package = dict->Find("Android-Package");
  if (android_package) {
    if (!android_package->is_string()) {
      return Status(kUnknownError, "'Android-Package' is not a string");
    }
    browser_info->android_package = android_package->GetString();
  }

  const std::string* browser_string = dict->FindString("Browser");
  if (!browser_string)
    return Status(kUnknownError, "version doesn't include 'Browser'");

  Status status = ParseBrowserString(android_package != nullptr,
                                     *browser_string, browser_info);
  if (status.IsError())
    return status;

  // "webSocketDebuggerUrl" is only returned on Chrome 62.0.3178 and above,
  // thus it's not an error if it's missing.
  const std::string* web_socket_url_in =
      dict->FindString("webSocketDebuggerUrl");
  if (web_socket_url_in)
    browser_info->web_socket_url = *web_socket_url_in;

  const std::string* blink_version = dict->FindString("WebKit-Version");
  if (!blink_version)
    return Status(kUnknownError, "version doesn't include 'WebKit-Version'");

  return ParseBlinkVersionString(*blink_version, &browser_info->blink_revision);
}

Status BrowserInfo::ParseBrowserString(bool has_android_package,
                                       const std::string& browser_string,
                                       BrowserInfo* browser_info) {
  if (has_android_package)
    browser_info->is_android = true;

  if (browser_string.empty()) {
    browser_info->browser_name = "content shell";
    return Status(kOk);
  }

  static const std::string kVersionPrefix =
      std::string(kUserAgentProductName) + "/";
  static const std::string kHeadlessVersionPrefix =
      std::string(kHeadlessUserAgentProductName) + "/";

  int build_no = 0;
  if (base::StartsWith(browser_string, kVersionPrefix,
                       base::CompareCase::SENSITIVE) ||
      base::StartsWith(browser_string, kHeadlessVersionPrefix,
                       base::CompareCase::SENSITIVE)) {
    std::string version = browser_string.substr(kVersionPrefix.length());
    bool headless_shell = false;
    if (base::StartsWith(browser_string, kHeadlessVersionPrefix,
                         base::CompareCase::SENSITIVE)) {
      version = browser_string.substr(kHeadlessVersionPrefix.length());
      headless_shell = true;
    }

    Status status = ParseBrowserVersionString(
        version, &browser_info->major_version, &build_no);
    if (status.IsError())
      return status;

    if (build_no != 0) {
      if (headless_shell) {
        browser_info->browser_name = kHeadlessShellCapabilityName;
        browser_info->is_headless_shell = true;
      } else {
        browser_info->browser_name = kBrowserCapabilityName;
      }
      browser_info->browser_version = version;
      browser_info->build_no = build_no;
      return Status(kOk);
    }
  }

  if (browser_string.find("Version/") == 0u ||   // KitKat
      (has_android_package && build_no == 0)) {  // Lollipop
    size_t pos = browser_string.find(kVersionPrefix);
    if (pos != std::string::npos) {
      browser_info->browser_name = "webview";
      browser_info->browser_version =
          browser_string.substr(pos + kVersionPrefix.length());
      browser_info->is_android = true;
      return ParseBrowserVersionString(browser_info->browser_version,
                                       &browser_info->major_version, &build_no);
    }
    return Status(kOk);
  }

  return Status(kUnknownError,
                base::StringPrintf("unrecognized %s version: %s",
                                   kBrowserShortName, browser_string.c_str()));
}

Status BrowserInfo::ParseBrowserVersionString(
    const std::string& browser_version,
    int* major_version,
    int* build_no) {
  std::vector<std::string_view> version_parts = base::SplitStringPiece(
      browser_version, ".", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  if (version_parts.size() != 4 ||
      !base::StringToInt(version_parts[0], major_version) ||
      !base::StringToInt(version_parts[2], build_no)) {
    return Status(kUnknownError,
                  "unrecognized browser version: " + browser_version);
  }
  return Status(kOk);
}

Status BrowserInfo::ParseBlinkVersionString(const std::string& blink_version,
                                            int* blink_revision) {
  size_t before = blink_version.find('@');
  size_t after = blink_version.find(')');
  if (before == std::string::npos || after == std::string::npos) {
    return Status(kUnknownError,
                  "unrecognized Blink version string: " + blink_version);
  }

  // Chrome OS reports its Blink revision as a git hash. In this case, ignore it
  // and don't set |blink_revision|. For Chrome (and for Chrome OS) we use the
  // build number instead of the blink revision for decisions about backwards
  // compatibility. Also accepts empty Blink revision (happens with some Chrome
  // OS builds).
  std::string revision = blink_version.substr(before + 1, after - before - 1);
  if (!IsGitHash(revision) && !base::StringToInt(revision, blink_revision) &&
      revision.length() > 0) {
    return Status(kUnknownError, "unrecognized Blink revision: " + revision);
  }

  return Status(kOk);
}

bool BrowserInfo::IsGitHash(const std::string& revision) {
  constexpr int kShortGitHashLength = 7;
  constexpr int kFullGitHashLength = 40;
  return kShortGitHashLength <= revision.size() &&
         revision.size() <= kFullGitHashLength &&
         base::ranges::all_of(revision, base::IsHexDigit<char>);
}
