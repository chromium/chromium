// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/favicon_base/favicon_url_parser.h"

#include <string_view>

#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "components/favicon_base/favicon_types.h"
#include "net/base/url_util.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/gfx/favicon_size.h"

namespace chrome {

namespace {

// Returns true if |search| is a substring of |path| which starts at
// |start_index|.
bool HasSubstringAt(const std::string& path,
                    size_t start_index,
                    const std::string& search) {
  return path.compare(start_index, search.length(), search) == 0;
}

// Same as base::StringToInt() but guarantees that the output number is positive
// (greater than zero), returns false in all other cases.
bool StringToPositiveInt(std::string_view input, int* output) {
  int result;
  if (!base::StringToInt(input, &result))
    return false;
  if (result <= 0)
    return false;
  *output = result;
  return result;
}

// Parse with legacy FaviconUrlFormat::kFavicon format.
bool ParseFaviconPathWithLegacyFormat(const std::string& path,
                                      chrome::ParsedFaviconPath* parsed) {
  // Parameters which can be used in chrome://favicon path. See file
  // "favicon_url_parser.h" for a description of what each one does.
  const char kIconURLParameter[] = "iconurl/";
  const char kSizeParameter[] = "size/";

  *parsed = chrome::ParsedFaviconPath();

  if (path.empty())
    return false;

  size_t parsed_index = 0;
  if (HasSubstringAt(path, parsed_index, kSizeParameter)) {
    parsed_index += strlen(kSizeParameter);

    size_t slash = path.find("/", parsed_index);
    if (slash == std::string::npos)
      return false;

    size_t scale_delimiter = path.find("@", parsed_index);
    std::string size_str;
    std::string scale_str;
    if (scale_delimiter == std::string::npos) {
      // Support the legacy size format of 'size/aa/' where 'aa' is the desired
      // size in DIP for the sake of not regressing the extensions which use it.
      size_str = path.substr(parsed_index, slash - parsed_index);
    } else {
      size_str = path.substr(parsed_index, scale_delimiter - parsed_index);
      scale_str = path.substr(scale_delimiter + 1,
                              slash - scale_delimiter - 1);
    }

    if (!StringToPositiveInt(size_str, &parsed->size_in_dip))
      return false;

    if (!scale_str.empty() &&
        !webui::ParseScaleFactor(scale_str, &parsed->device_scale_factor)) {
      return false;
    }

    parsed_index = slash + 1;
  }

  if (HasSubstringAt(path, parsed_index, kIconURLParameter)) {
    parsed_index += strlen(kIconURLParameter);
    parsed->icon_url = path.substr(parsed_index);
  } else {
    parsed->page_url = path.substr(parsed_index);
  }

  // The parsed index needs to be returned in order to allow Instant Extended
  // to translate favicon URLs using advanced parameters.
  // Example:
  //   "chrome-search://favicon/size/16@2x/<renderer-id>/<most-visited-id>"
  // would be translated to:
  //   "chrome-search://favicon/size/16@2x/<most-visited-item-with-given-id>".
  parsed->path_index = parsed_index;
  return true;
}

// Parse with FaviconUrlFormat::kFavicon2 format.
bool ParseFaviconPathWithFavicon2Format(const std::string& path,
                                        chrome::ParsedFaviconPath* parsed) {
  if (path.empty())
    return false;

  GURL query_url = GURL("chrome://favicon2/").Resolve(path);

  *parsed = chrome::ParsedFaviconPath();

  for (net::QueryIterator it(query_url); !it.IsAtEnd(); it.Advance()) {
    const std::string_view key = it.GetKey();
    // Note: each of these keys can be used in chrome://favicon2 path. See file
    // "favicon_url_parser.h" for a description of what each one does.
    if (key == "allowGoogleServerFallback") {
      const std::string val = it.GetUnescapedValue();
      if (!(val == "0" || val == "1"))
        return false;
      parsed->allow_favicon_server_fallback = val == "1";
    } else if (key == "showFallbackMonogram") {
      parsed->show_fallback_monogram = true;
    } else if (key == "forceLightMode") {
      parsed->force_light_mode = true;
    } else if (key == "iconUrl") {
      parsed->icon_url = it.GetUnescapedValue();
    } else if (key == "pageUrl") {
      parsed->page_url = it.GetUnescapedValue();
    } else if (key == "scaleFactor" &&
               !webui::ParseScaleFactor(it.GetUnescapedValue(),
                                        &parsed->device_scale_factor)) {
      return false;
    } else if (key == "size" && !StringToPositiveInt(it.GetUnescapedValue(),
                                                     &parsed->size_in_dip)) {
      return false;
    }
  }

  if (parsed->page_url.empty() && parsed->icon_url.empty())
    return false;

  if (parsed->allow_favicon_server_fallback && parsed->page_url.empty()) {
    // Since the server is queried by page url, we'll fail if no non-empty page
    // url is provided and the fallback parameter is still set to true.
    NOTIMPLEMENTED();
    return false;
  }

  return true;
}

}  // namespace

ParsedFaviconPath::ParsedFaviconPath() = default;

ParsedFaviconPath::ParsedFaviconPath(const ParsedFaviconPath& other) = default;

ParsedFaviconPath& ParsedFaviconPath::operator=(
    const ParsedFaviconPath& other) = default;

bool ParseFaviconPath(const std::string& path,
                      FaviconUrlFormat format,
                      ParsedFaviconPath* parsed) {
  switch (format) {
    case FaviconUrlFormat::kFaviconLegacy:
      return ParseFaviconPathWithLegacyFormat(path, parsed);
    case FaviconUrlFormat::kFavicon2:
      return ParseFaviconPathWithFavicon2Format(path, parsed);
  }
  NOTREACHED_IN_MIGRATION();
  return false;
}

}  // namespace chrome
