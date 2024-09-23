// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/base/avatar_icon_util.h"

#include <string>
#include <vector>

#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "third_party/re2/src/re2/re2.h"
#include "url/gurl.h"

namespace {

// Separator of URL path components.
constexpr char kURLPathSeparator[] = "/";

// Constants describing legacy image URL format.
// See https://crbug.com/733306#c3 for details.
constexpr size_t kLegacyURLPathComponentsCount = 6;
constexpr size_t kLegacyURLPathComponentsCountWithOptions = 7;
constexpr size_t kLegacyURLPathOptionsComponentPosition = 5;
// Constants describing content image URL format.
// See https://crbug.com/911332#c15 for details.
constexpr size_t kContentURLPathMinComponentsCount = 2;
constexpr size_t kContentURLPathMaxComponentsCount = 3;
constexpr char kContentURLOptionsStartChar = '=';
// Various options that can be embedded in image URL.
constexpr char kImageURLOptionSeparator[] = "-";
constexpr char kImageURLOptionSizePattern[] = R"(s\d+)";
constexpr char kImageURLOptionSizeFormat[] = "s%d";
constexpr char kImageURLOptionSquareCrop[] = "c";
// Option to disable default avatar if user doesn't have a custom one.
constexpr char kImageURLOptionNoSilhouette[] = "ns";

std::string BuildImageURLOptionsString(int image_size,
                                       bool no_silhouette,
                                       const std::string& existing_options) {
  std::vector<std::string> url_options =
      base::SplitString(existing_options, kImageURLOptionSeparator,
                        base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);

  RE2 size_pattern(kImageURLOptionSizePattern);
  std::erase_if(url_options, [&size_pattern](const std::string& str) {
    return RE2::FullMatch(str, size_pattern);
  });
  std::erase(url_options, kImageURLOptionSquareCrop);
  std::erase(url_options, kImageURLOptionNoSilhouette);

  url_options.push_back(
      base::StringPrintf(kImageURLOptionSizeFormat, image_size));
  url_options.push_back(kImageURLOptionSquareCrop);
  if (no_silhouette)
    url_options.push_back(kImageURLOptionNoSilhouette);
  return base::JoinString(url_options, kImageURLOptionSeparator);
}

// Returns an empty vector if |url_components| couldn't be processed as a legacy
// image URL.
std::vector<std::string> TryProcessAsLegacyImageURL(
    std::vector<std::string> url_components,
    int image_size,
    bool no_silhouette) {
  if (url_components.back().empty())
    return {};

  if (url_components.size() == kLegacyURLPathComponentsCount) {
    url_components.insert(
        url_components.begin() + kLegacyURLPathOptionsComponentPosition,
        BuildImageURLOptionsString(image_size, no_silhouette, std::string()));
    return url_components;
  }

  if (url_components.size() == kLegacyURLPathComponentsCountWithOptions) {
    std::string options =
        url_components.at(kLegacyURLPathOptionsComponentPosition);
    url_components[kLegacyURLPathOptionsComponentPosition] =
        BuildImageURLOptionsString(image_size, no_silhouette, options);
    return url_components;
  }

  return {};
}

// Returns an empty vector if |url_components| couldn't be processed as a
// content image URL.
std::vector<std::string> TryProcessAsContentImageURL(
    std::vector<std::string> url_components,
    int image_size,
    bool no_silhouette) {
  if (url_components.size() < kContentURLPathMinComponentsCount ||
      url_components.size() > kContentURLPathMaxComponentsCount ||
      url_components.back().empty()) {
    return {};
  }

  std::string* options_component = &url_components.back();
  // Extract existing options from |options_component|.
  const size_t options_pos =
      options_component->find(kContentURLOptionsStartChar);
  std::string component_without_options =
      options_component->substr(0, options_pos);
  std::string existing_options =
      options_pos == std::string::npos
          ? ""
          : options_component->substr(options_pos + 1);
  // Update options in |options_component|.
  *options_component =
      component_without_options + kContentURLOptionsStartChar +
      BuildImageURLOptionsString(image_size, no_silhouette, existing_options);
  return url_components;
}

}  // namespace

namespace signin {

const int kAccountInfoImageSize = 256;

GURL GetAvatarImageURLWithOptions(const GURL& old_url,
                                  int image_size,
                                  bool no_silhouette) {
  DCHECK(old_url.is_valid());

  std::vector<std::string> components =
      base::SplitString(old_url.path(), kURLPathSeparator,
                        base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);

  auto new_components =
      TryProcessAsContentImageURL(components, image_size, no_silhouette);

  if (new_components.empty()) {
    new_components =
        TryProcessAsLegacyImageURL(components, image_size, no_silhouette);
  }

  if (new_components.empty()) {
    // URL doesn't match any known patterns, so return unchanged.
    return old_url;
  }

  std::string new_path = base::JoinString(new_components, kURLPathSeparator);
  GURL::Replacements replacement;
  replacement.SetPathStr(new_path);
  return old_url.ReplaceComponents(replacement);
}

}  // namespace signin
