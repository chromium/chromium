// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/manifest_handlers/settings_overrides_handler.h"

#include <memory>
#include <utility>

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/url_formatter/url_formatter.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/feature_switch.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handlers/permissions_parser.h"
#include "extensions/common/permissions/api_permission_set.h"
#include "extensions/common/permissions/manifest_permission.h"
#include "extensions/common/permissions/permissions_info.h"
#include "extensions/common/permissions/settings_override_permission.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_message_utils.h"
#include "url/gurl.h"

using extensions::api::manifest_types::ChromeSettingsOverrides;
using extensions::mojom::APIPermissionID;

namespace extensions {
namespace {

std::optional<GURL> CreateManifestURL(const std::string& url) {
  std::optional<GURL> manifest_url(std::in_place, url);
  if (!manifest_url->is_valid() || !manifest_url->SchemeIsHTTPOrHTTPS())
    return std::nullopt;
  return manifest_url;
}

std::optional<GURL> ParseHomepage(const ChromeSettingsOverrides& overrides,
                                  std::u16string* error) {
  if (!overrides.homepage)
    return std::nullopt;
  std::optional<GURL> manifest_url = CreateManifestURL(*overrides.homepage);
  if (!manifest_url) {
    *error = extensions::ErrorUtils::FormatErrorMessageUTF16(
        manifest_errors::kInvalidHomepageOverrideURL, *overrides.homepage);
  }
  return manifest_url;
}

std::vector<GURL> ParseStartupPage(const ChromeSettingsOverrides& overrides,
                                   std::u16string* error) {
  std::vector<GURL> urls;
  if (!overrides.startup_pages)
    return urls;

  for (std::vector<std::string>::const_iterator i =
       overrides.startup_pages->begin(); i != overrides.startup_pages->end();
       ++i) {
    std::optional<GURL> manifest_url = CreateManifestURL(*i);
    if (!manifest_url) {
      *error = extensions::ErrorUtils::FormatErrorMessageUTF16(
          manifest_errors::kInvalidStartupOverrideURL, *i);
    } else {
      urls.push_back(std::move(*manifest_url));
    }
  }
  return urls;
}

std::optional<ChromeSettingsOverrides::SearchProvider> ParseSearchEngine(
    ChromeSettingsOverrides* overrides,
    std::u16string* error) {
  if (!overrides->search_provider)
    return std::nullopt;
  if (!CreateManifestURL(overrides->search_provider->search_url)) {
    *error = extensions::ErrorUtils::FormatErrorMessageUTF16(
        manifest_errors::kInvalidSearchEngineURL,
        overrides->search_provider->search_url);
    return std::nullopt;
  }
  if (overrides->search_provider->prepopulated_id)
    return std::move(*overrides->search_provider);

  auto get_missing_key_error = [](const char* missing_key) {
    return extensions::ErrorUtils::FormatErrorMessageUTF16(
        manifest_errors::kInvalidSearchEngineMissingKeys, missing_key);
  };

  if (!overrides->search_provider->name) {
    *error = get_missing_key_error("name");
    return std::nullopt;
  }
  if (!overrides->search_provider->keyword) {
    *error = get_missing_key_error("keyword");
    return std::nullopt;
  }
  if (!overrides->search_provider->encoding) {
    *error = get_missing_key_error("encoding");
    return std::nullopt;
  }
  if (!overrides->search_provider->favicon_url) {
    *error = get_missing_key_error("favicon_url");
    return std::nullopt;
  }
  if (!CreateManifestURL(*overrides->search_provider->favicon_url)) {
    *error = extensions::ErrorUtils::FormatErrorMessageUTF16(
        manifest_errors::kInvalidSearchEngineURL,
        *overrides->search_provider->favicon_url);
    return std::nullopt;
  }
  return std::move(*overrides->search_provider);
}

std::string FormatUrlForDisplay(const GURL& url) {
  // A www. prefix is not informative and thus not worth the limited real estate
  // in the permissions UI.
  return url_formatter::StripWWW(url.host());
}

}  // namespace

SettingsOverrides::SettingsOverrides() {}

SettingsOverrides::~SettingsOverrides() {}

// static
const SettingsOverrides* SettingsOverrides::Get(
    const Extension* extension) {
  return static_cast<SettingsOverrides*>(
      extension->GetManifestData(manifest_keys::kSettingsOverride));
}

SettingsOverridesHandler::SettingsOverridesHandler() {}

SettingsOverridesHandler::~SettingsOverridesHandler() {}

bool SettingsOverridesHandler::Parse(Extension* extension,
                                     std::u16string* error) {
  const base::Value::Dict* dict =
      extension->manifest()->FindDictPath(manifest_keys::kSettingsOverride);
  CHECK(dict != nullptr);
  auto settings = ChromeSettingsOverrides::FromValue(*dict);
  if (!settings.has_value()) {
    *error = settings.error();
    return false;
  }

  // TODO(crbug.com/40703390): Any of {homepage, search_engine, startup_pages}'s
  // parse failure should result in hard error. Currently, Parse fails only when
  // all of these fail to parse.
  auto info = std::make_unique<SettingsOverrides>();
  std::u16string homepage_error;
  info->homepage = ParseHomepage(*settings, &homepage_error);
  std::u16string search_engine_error;
  info->search_engine =
      ParseSearchEngine(&settings.value(), &search_engine_error);
  std::u16string startup_pages_error;
  info->startup_pages = ParseStartupPage(*settings, &startup_pages_error);
  if (!info->homepage && !info->search_engine && info->startup_pages.empty()) {
    if (!homepage_error.empty()) {
      *error = homepage_error;
    } else if (!search_engine_error.empty()) {
      *error = search_engine_error;
    } else if (!startup_pages_error.empty()) {
      *error = startup_pages_error;
    } else {
      *error = ErrorUtils::FormatErrorMessageUTF16(
          manifest_errors::kInvalidEmptyDictionary,
          manifest_keys::kSettingsOverride);
    }
    return false;
  }

  if (info->search_engine) {
    PermissionsParser::AddAPIPermission(
        extension, new SettingsOverrideAPIPermission(
                       PermissionsInfo::GetInstance()->GetByID(
                           APIPermissionID::kSearchProvider),
                       FormatUrlForDisplay(*CreateManifestURL(
                           info->search_engine->search_url))));
  }
  if (!info->startup_pages.empty()) {
    PermissionsParser::AddAPIPermission(
        extension,
        new SettingsOverrideAPIPermission(
            PermissionsInfo::GetInstance()->GetByID(
                APIPermissionID::kStartupPages),
            // We only support one startup page even though the type of the
            // manifest property is a list, only the first one is used.
            FormatUrlForDisplay(info->startup_pages[0])));
  }
  if (info->homepage) {
    PermissionsParser::AddAPIPermission(
        extension,
        new SettingsOverrideAPIPermission(
            PermissionsInfo::GetInstance()->GetByID(APIPermissionID::kHomepage),
            FormatUrlForDisplay(*(info->homepage))));
  }
  extension->SetManifestData(manifest_keys::kSettingsOverride, std::move(info));
  return true;
}

base::span<const char* const> SettingsOverridesHandler::Keys() const {
  static constexpr const char* kKeys[] = {manifest_keys::kSettingsOverride};
  return kKeys;
}

}  // namespace extensions
