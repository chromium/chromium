// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/chrome/browser_info.h"

#include <stddef.h>

#include <memory>

#include "base/json/json_reader.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chrome/test/chromedriver/constants/version.h"

BrowserInfo::BrowserInfo()
    : major_version(0),
      build_no(kToTBuildNo),
      blink_revision(kToTBlinkRevision),
      is_android(false),
      is_headless(false) {}

BrowserInfo::~BrowserInfo() {}

Status ParseBrowserInfo(const std::string& data, BrowserInfo* browser_info) {
  std::unique_ptr<base::Value> value = base::JSONReader::ReadDeprecated(data);
  if (!value.get())
    return Status(kUnknownError, "version info not in JSON");

  base::DictionaryValue* dict;
  if (!value->GetAsDictionary(&dict))
    return Status(kUnknownError, "version info not a dictionary");

  bool has_android_package = dict->HasKey("Android-Package");
  if (has_android_package) {
    if (!dict->GetString("Android-Package", &browser_info->android_package))
      return Status(kUnknownError, "'Android-Package' is not a string");
  }

  std::string browser_string;
  if (!dict->GetString("Browser", &browser_string))
    return Status(kUnknownError, "version doesn't include 'Browser'");

  Status status =
      ParseBrowserString(has_android_package, browser_string, browser_info);
  if (status.IsError())
    return status;

  // "webSocketDebuggerUrl" is only returned on Chrome 62.0.3178 and above,
  // thus it's not an error if it's missing.
  dict->GetString("webSocketDebuggerUrl", &browser_info->web_socket_url);

  std::string blink_version;
  if (!dict->GetString("WebKit-Version", &blink_version))
    return Status(kUnknownError, "version doesn't include 'WebKit-Version'");

  return ParseBlinkVersionString(blink_version, &browser_info->blink_revision);
}

Status ParseBrowserString(bool has_android_package,
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
    bool headless = false;
    if (base::StartsWith(browser_string, kHeadlessVersionPrefix,
                         base::CompareCase::SENSITIVE)) {
      version = browser_string.substr(kHeadlessVersionPrefix.length());
      headless = true;
    }

    Status status = ParseBrowserVersionString(
        version, &browser_info->major_version, &build_no);
    if (status.IsError())
      return status;

    if (build_no != 0) {
      if (headless) {
        browser_info->browser_name =
            base::StringPrintf("headless %s", kBrowserCapabilityName);
        browser_info->is_headless = true;
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

Status ParseBrowserVersionString(const std::string& browser_version,
                                 int* major_version, int* build_no) {
  std::vector<base::StringPiece> version_parts = base::SplitStringPiece(
      browser_version, ".", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  if (version_parts.size() != 4 ||
      !base::StringToInt(version_parts[0], major_version) ||
      !base::StringToInt(version_parts[2], build_no)) {
    return Status(kUnknownError,
                  "unrecognized browser version: " + browser_version);
  }
  return Status(kOk);
}

Status ParseBlinkVersionString(const std::string& blink_version,
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

bool IsGitHash(const std::string& revision) {
  const int kShortGitHashLength = 7;
  const int kFullGitHashLength = 40;
  return kShortGitHashLength <= revision.size()
      && revision.size() <= kFullGitHashLength
      && base::ContainsOnlyChars(revision, "0123456789abcdefABCDEF");
}
