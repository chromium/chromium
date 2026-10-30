// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/google_chrome_scheme_util.h"

#include <string>
#include <string_view>

#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/common/chrome_features.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_WIN)
#include "base/strings/string_util_win.h"
#include "base/strings/utf_string_conversions.h"
#endif

namespace startup {

namespace {

bool TryStripSchemePrefix(base::FilePath::StringViewType& arg,
                          std::string_view scheme) {
  if (arg.size() <= scheme.size() ||
      !base::EqualsCaseInsensitiveASCII(arg.substr(0, scheme.size()), scheme)) {
    return false;
  }

  base::FilePath::StringViewType rest = arg.substr(scheme.size());
  // First check for "scheme://"
  if (auto suffix = base::RemovePrefix(rest, FILE_PATH_LITERAL("://"))) {
    arg = *suffix;
    return true;
  }

  // Fallback: check for "scheme:" (opaque). This supports cases where
  // GURL parsing might have treated the scheme as opaque (e.g.
  // google-chrome:http://example.com) or if the user omitted slashes.
  if (auto suffix = base::RemovePrefix(rest, FILE_PATH_LITERAL(":"))) {
    arg = *suffix;
    return true;
  }

  return false;
}

}  // namespace

bool StripGoogleChromeScheme(base::FilePath::StringViewType& arg) {
  const std::string direct_launch_scheme =
      shell_integration::GetDirectLaunchUrlScheme();

  if (direct_launch_scheme.empty()) {
    return false;  // Direct launch not supported.
  }

  // Optimization: Check prefix first to avoid activating experiment
  // unnecessarily.
  base::FilePath::StringViewType temp = arg;
  if (!TryStripSchemePrefix(temp, direct_launch_scheme)) {
    return false;
  }

  // We want to activate the experiment when it is relevant for better
  // stats collection. We plan to remove this flag once we establish it works
  // fine.
  if (!base::FeatureList::IsEnabled(features::kGoogleChromeScheme)) {
    return false;
  }

  arg = temp;
  return true;
}

std::optional<GURL> ExtractGoogleChromeSchemeInnerUrl(const GURL& url) {
  const std::string& spec = url.spec();
#if BUILDFLAG(IS_WIN)
  std::wstring url_view_storage = base::UTF8ToWide(spec);
  base::FilePath::StringViewType url_view = url_view_storage;
#else
  base::FilePath::StringViewType url_view = spec;
#endif

  // Use strict checking to ensure we only handle the scheme registered for this
  // browser instance (e.g. "google-chrome" for Stable). This matches
  // administrator expectations.
  if (StripGoogleChromeScheme(url_view)) {
#if BUILDFLAG(IS_WIN)
    return GURL(base::AsStringPiece16(url_view));
#else
    return GURL(url_view);
#endif
  }
  return std::nullopt;
}

}  // namespace startup
