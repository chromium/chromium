// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/api/url_handlers/url_handlers_parser.h"

#include <memory>

#include "base/command_line.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handlers/offline_enabled_info.h"
#include "extensions/common/switches.h"
#include "net/base/network_change_notifier.h"
#include "url/gurl.h"

using net::NetworkChangeNotifier;

// TODO(sergeygs): Use the same strategy that externally_connectable does for
// parsing the manifest: declare a schema for the manifest entry in
// manifest_types.json, then use it here.
//
// See:
// chrome/common/extensions/api/manifest_types.json
// chrome/common/extensions/manifest_handlers/externally_connectable.*
//
// Do the same in (at least) file_handlers_parser.cc as well.

namespace extensions {

namespace {

const UrlHandlerInfo* GetMatchingUrlHandler(const Extension* extension,
                                            const GURL& url) {
  const std::vector<UrlHandlerInfo>* handlers =
      UrlHandlers::GetUrlHandlers(extension);
  if (!handlers)
    return nullptr;

  for (const auto& handler : *handlers) {
    if (handler.patterns.MatchesURL(url))
      return &handler;
  }

  return nullptr;
}

}  // namespace

namespace mkeys = manifest_keys;
namespace merrors = manifest_errors;

UrlHandlerInfo::UrlHandlerInfo() {
}

UrlHandlerInfo::UrlHandlerInfo(UrlHandlerInfo&& other) = default;

UrlHandlerInfo::~UrlHandlerInfo() {
}

UrlHandlers::UrlHandlers() {
}

UrlHandlers::~UrlHandlers() {
}

// static
const std::vector<UrlHandlerInfo>* UrlHandlers::GetUrlHandlers(
    const Extension* extension) {
  UrlHandlers* info = static_cast<UrlHandlers*>(
      extension->GetManifestData(mkeys::kUrlHandlers));
  return info ? &info->handlers : nullptr;
}

// static
bool UrlHandlers::CanPlatformAppHandleUrl(const Extension* app,
                                          const GURL& url) {
  DCHECK(app->is_platform_app());
  return !!GetMatchingPlatformAppUrlHandler(app, url);
}

// static
// TODO(crbug.com/40124309): Clean up this function and related paths.
bool UrlHandlers::CanBookmarkAppHandleUrl(const Extension* app,
                                          const GURL& url) {
  return !!GetMatchingUrlHandler(app, url);
}

// static
const UrlHandlerInfo* UrlHandlers::GetMatchingPlatformAppUrlHandler(
    const Extension* app,
    const GURL& url) {
  DCHECK(app->is_platform_app());

  if (NetworkChangeNotifier::IsOffline() &&
      !OfflineEnabledInfo::IsOfflineEnabled(app))
    return nullptr;
  return GetMatchingUrlHandler(app, url);
}

UrlHandlersParser::UrlHandlersParser() {
}

UrlHandlersParser::~UrlHandlersParser() {
}

bool ParseUrlHandler(const std::string& handler_id,
                     const base::Value::Dict& handler_info,
                     std::vector<UrlHandlerInfo>* url_handlers,
                     std::u16string* error,
                     Extension* extension) {
  DCHECK(error);

  UrlHandlerInfo handler;
  handler.id = handler_id;

  if (const std::string* ptr =
          handler_info.FindString(mkeys::kUrlHandlerTitle)) {
    handler.title = *ptr;
  } else {
    *error = merrors::kInvalidURLHandlerTitle;
    return false;
  }

  const base::Value::List* manif_patterns =
      handler_info.FindList(mkeys::kMatches);
  if (!manif_patterns || manif_patterns->empty()) {
    *error = ErrorUtils::FormatErrorMessageUTF16(
        merrors::kInvalidURLHandlerPattern, handler_id);
    return false;
  }

  for (const auto& entry : *manif_patterns) {
    std::string str_pattern =
        entry.is_string() ? entry.GetString() : std::string();
    // TODO(sergeygs): Limit this to non-top-level domains.
    // TODO(sergeygs): Also add a verification to the CWS installer that the
    // URL patterns claimed here belong to the app's author verified sites.
    URLPattern pattern(URLPattern::SCHEME_HTTP | URLPattern::SCHEME_HTTPS);
    if (pattern.Parse(str_pattern) != URLPattern::ParseResult::kSuccess) {
      *error = ErrorUtils::FormatErrorMessageUTF16(
          merrors::kInvalidURLHandlerPatternElement, handler_id);
      return false;
    }
    handler.patterns.AddPattern(pattern);
  }

  url_handlers->push_back(std::move(handler));

  return true;
}

bool UrlHandlersParser::Parse(Extension* extension, std::u16string* error) {
  std::unique_ptr<UrlHandlers> info(new UrlHandlers);
  const base::Value::Dict* all_handlers =
      extension->manifest()->available_values().FindDict(mkeys::kUrlHandlers);
  if (!all_handlers) {
    *error = merrors::kInvalidURLHandlers;
    return false;
  }

  DCHECK(extension->is_platform_app());

  for (const auto item : *all_handlers) {
    // A URL handler entry is a title and a list of URL patterns to handle.
    const base::Value::Dict* handler = item.second.GetIfDict();
    if (!handler) {
      *error = merrors::kInvalidURLHandlerPatternElement16;
      return false;
    }

    if (!ParseUrlHandler(item.first, *handler, &info->handlers, error,
                         extension)) {
      // Text in |error| is set by ParseUrlHandler.
      return false;
    }
  }

  extension->SetManifestData(mkeys::kUrlHandlers, std::move(info));

  return true;
}

base::span<const char* const> UrlHandlersParser::Keys() const {
  static constexpr const char* kKeys[] = {mkeys::kUrlHandlers};
  return kKeys;
}

}  // namespace extensions
