// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/parsers/parser_utils/command_line_arguments_sanitizer.h"

#include <stdio.h>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/chrome_cleaner/os/file_path_sanitization.h"
#include "url/gurl.h"
#include "url/url_util.h"

namespace chrome_cleaner {
namespace {

// It may be possible that the return value of GURL.host() is %-scaped, use
// this function to get the ASCII version.
std::wstring GetUnescapedHost(GURL url) {
  std::string url_host = url.host();

  url::RawCanonOutputT<char16_t> unescaped_url_host;
  url::DecodeURLEscapeSequences(url_host.c_str(), url_host.length(),
                                url::DecodeURLMode::kUTF8OrIsomorphic,
                                &unescaped_url_host);
  return std::wstring(base::as_wcstr(unescaped_url_host.data()),
                      unescaped_url_host.length());
}

// Tries to parse the url string using GURL and returns the concatenation of
// the scheme and the host. If the url is not valid it returns the same string
// that was provided. If the url has no scheme, http:// is assumed.
std::wstring SanitizeUrl(const std::wstring& possible_url) {
  bool has_scheme = (possible_url.find(L"://") != std::string::npos);
  std::wstring scheme = (has_scheme) ? L"" : L"http://";
  GURL url(base::AsStringPiece16(scheme + possible_url));

  if (!url.is_valid())
    return possible_url;

  // If the url we received did not have a scheme and it does not have a
  // dot on its host then assume it is a file path and don't sanitize it.
  if (!has_scheme && url.host().find(".") == std::string::npos)
    return possible_url;

  return base::UTF8ToWide(url.scheme()) + L"://" + GetUnescapedHost(url);
}
}  // namespace

std::vector<std::wstring> SanitizeArguments(const std::wstring& arguments) {
  // We prepend a dummy to the string because it takes the first element as the
  // executable path.
  const base::CommandLine command_line =
      base::CommandLine::FromString(L"dummy " + arguments);

  const base::CommandLine::SwitchMap& switches = command_line.GetSwitches();

  std::vector<std::wstring> sanitized_arguments;
  for (auto it = switches.begin(); it != switches.end(); it++) {
    sanitized_arguments.push_back(
        L"--" + base::UTF8ToWide(it->first) +
        ((it->second.empty())
             ? L""
             : L"=" + SanitizePath(base::FilePath(SanitizeUrl(it->second)))));
  }

  const base::CommandLine::StringVector& vector_arguments =
      command_line.GetArgs();
  for (const auto& argument : vector_arguments) {
    sanitized_arguments.push_back(
        SanitizePath(base::FilePath(SanitizeUrl(argument))));
  }

  return sanitized_arguments;
}

}  // namespace chrome_cleaner
