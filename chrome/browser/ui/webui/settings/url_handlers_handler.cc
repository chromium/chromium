// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/url_handlers_handler.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/check.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/components/url_handler_launch_params.h"
#include "chrome/browser/web_applications/components/url_handler_prefs.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "components/url_formatter/elide_url.h"
#include "content/public/browser/web_ui.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace settings {

namespace {

struct EnabledRow {
  std::string origin_key;  // Eg. "https://contoso.com"
  std::string app_id;
  std::string short_name;
  std::u16string publisher;
  bool has_origin_wildcard;
  std::string path;                            // Eg. "/*" or "/news"
  absl::optional<std::u16string> display_url;  // Eg. "contoso.com/news"

  bool operator<(const EnabledRow& other) const {
    if (this == &other)
      return false;

    if (short_name < other.short_name)
      return true;
    if (origin_key < other.origin_key)
      return true;
    if (has_origin_wildcard < other.has_origin_wildcard)
      return true;
    if (path < other.path)
      return true;

    return false;
  }
};

struct DisabledRow {
  std::string origin_key;
  bool has_origin_wildcard;
  std::string path;  // Eg. "/*" or "/news"
  std::u16string display_url;

  bool operator<(const DisabledRow& other) const {
    if (this == &other)
      return false;

    if (origin_key < other.origin_key)
      return true;
    if (has_origin_wildcard < other.has_origin_wildcard)
      return true;
    if (path < other.path)
      return true;

    return false;
  }
};

// Example of returned Value:
// [
//   {
//     "app_entries": [
//       {
//         "app_id": "jncifgjpfigpfjphlanoeonmiedopibl",
//         "display_url": "example.com/abc",  // optional
//         "has_origin_wildcard": true,
//         "origin_key": "https://example.com",
//         "path": "/abc",
//         "publisher": "example.com",
//         "short_name": "Example App
//       }
//     ],
//     "display_origin": "*.example.com"
//   }
// ]
base::Value SerializeEnabledHandlersList(
    const base::flat_map<std::u16string, base::flat_set<EnabledRow>>&
        organizer) {
  base::Value result_list(base::Value::Type::LIST);

  for (const auto& kv : organizer) {
    const std::u16string& origin_str = kv.first;
    const base::flat_set<EnabledRow>& app_rows = kv.second;

    base::Value result_entry(base::Value::Type::DICTIONARY);
    result_entry.SetStringKey("display_origin", origin_str);
    base::Value app_entries(base::Value::Type::LIST);
    for (const auto& app_row : app_rows) {
      base::Value app_entry(base::Value::Type::DICTIONARY);
      app_entry.SetStringKey("origin_key", app_row.origin_key);
      app_entry.SetStringKey("app_id", app_row.app_id);
      app_entry.SetStringKey("short_name", app_row.short_name);
      app_entry.SetStringKey("publisher", app_row.publisher);
      app_entry.SetBoolKey("has_origin_wildcard", app_row.has_origin_wildcard);
      app_entry.SetStringKey("path", app_row.path);
      // "display_url" is optional. It's only needed when path != "/*".
      if (app_row.display_url.has_value())
        app_entry.SetStringKey("display_url", app_row.display_url.value());

      app_entries.Append(std::move(app_entry));
    }
    result_entry.SetKey("app_entries", std::move(app_entries));
    result_list.Append(std::move(result_entry));
  }
  return result_list;
}

// Example of returned value:
// [
//   {
//     "display_url": "*.example.com/abc",
//     "has_origin_wildcard": true,
//     "origin_key": "https://example.com",
//     "path": "/abc"
//   }
// ]
base::Value SerializeDisabledHandlersList(
    const base::flat_set<DisabledRow>& organizer) {
  base::Value disabled_entries(base::Value::Type::LIST);

  for (const DisabledRow& row : organizer) {
    base::Value entry(base::Value::Type::DICTIONARY);
    entry.SetStringKey("origin_key", row.origin_key);
    entry.SetBoolKey("has_origin_wildcard", row.has_origin_wildcard);
    entry.SetStringKey("path", row.path);
    entry.SetStringKey("display_url", row.display_url);
    disabled_entries.Append(std::move(entry));
  }
  return disabled_entries;
}

std::u16string FormatOriginString(const url::Origin& origin,
                                  bool has_origin_wildcard) {
  std::u16string origin_str = url_formatter::FormatOriginForSecurityDisplay(
      origin, url_formatter::SchemeDisplay::OMIT_HTTP_AND_HTTPS);

  if (has_origin_wildcard)
    origin_str = u"*." + origin_str;

  return origin_str;
}

}  // namespace

UrlHandlersHandler::UrlHandlersHandler(
    PrefService* local_state,
    Profile* profile,
    web_app::WebAppRegistrar* web_app_registrar)
    : local_state_(local_state),
      profile_(profile),
      web_app_registrar_(web_app_registrar) {
  DCHECK(local_state_);
  DCHECK(profile_);
  DCHECK(web_app_registrar_);
}

UrlHandlersHandler::~UrlHandlersHandler() = default;

void UrlHandlersHandler::OnJavascriptAllowed() {
  DCHECK(local_state_);

  local_state_pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
  local_state_pref_change_registrar_->Init(local_state_);
  local_state_pref_change_registrar_->Add(
      prefs::kWebAppsUrlHandlerInfo,
      base::BindRepeating(
          &UrlHandlersHandler::OnUrlHandlersLocalStatePrefChanged,
          // Using base::Unretained(this) here is safe because the lifetime of
          // this UrlHandlersHandler instance is the same or longer than that of
          // local_state_pref_change_registrar_.
          // local_state_pref_change_registrar_ is destroyed either in
          // |OnJavascriptDisallowed| or when this UrlHandlersHandler is
          // destroyed.
          base::Unretained(this)));
}

void UrlHandlersHandler::OnJavascriptDisallowed() {
  local_state_pref_change_registrar_.reset();
}

void UrlHandlersHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "getUrlHandlers",
      base::BindRepeating(&UrlHandlersHandler::HandleGetUrlHandlers,
                          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "resetUrlHandlerSavedChoice",
      base::BindRepeating(&UrlHandlersHandler::HandleResetUrlHandlerSavedChoice,
                          base::Unretained(this)));
}

void UrlHandlersHandler::OnUrlHandlersLocalStatePrefChanged() {
  UpdateModel();
}

void UrlHandlersHandler::UpdateModel() {
  base::Value enabled_handlers_list = GetEnabledHandlersList();
  base::Value disabled_handlers_list = GetDisabledHandlersList();

  // TODO(crbug.com/1217423): Implement a handler on the WebUI side to accept
  // this data and update the UI.
  FireWebUIListener("updateUrlHandlers", enabled_handlers_list,
                    disabled_handlers_list);
}

void UrlHandlersHandler::HandleGetUrlHandlers(const base::ListValue* args) {
  CHECK_EQ(1U, args->GetList().size());
  const base::Value& callback_id = args->GetList()[0];
  AllowJavascript();

  base::Value result(base::Value::Type::DICTIONARY);
  result.SetKey("enabled", base::Value(GetEnabledHandlersList()));
  result.SetKey("disabled", base::Value(GetDisabledHandlersList()));
  ResolveJavascriptCallback(callback_id, result);
}

void UrlHandlersHandler::HandleResetUrlHandlerSavedChoice(
    const base::ListValue* args) {
  CHECK_EQ(4U, args->GetList().size());
  const std::string& origin = args->GetList()[0].GetString();
  bool has_origin_wildcard = args->GetList()[1].GetBool();
  const std::string& path = args->GetList()[2].GetString();
  // If app_id is an empty string, reset saved choices for all applicable
  // entries regardless of app_id.
  const std::string& app_id = args->GetList()[3].GetString();
  absl::optional<std::string> app_id_opt =
      app_id.empty() ? absl::nullopt : absl::make_optional(app_id);

  web_app::url_handler_prefs::ResetSavedChoice(local_state_, app_id_opt,
                                               profile_->GetPath(), origin,
                                               has_origin_wildcard, path);

  // No need to call UpdateModel() here - we should receive a notification
  // that local state prefs have changed and we will update the view
  // then.
}

// Example of returned value:
// [
//   {
//     "display_origin": "example.com",
//     "app_entries": [
//       {
//         "app_id": "jncifgjpfigpfjphlanoeonmiedopibl",
//         "has_origin_wildcard": false,
//         "origin_key": "https://example.com",
//         "path": "/abc",
//         "publisher": "example.com",
//         "short_name": "Example App",
//         "display_url": "example.com/abc"  // optional
//       }
//     ]
//   }
// ]
base::Value UrlHandlersHandler::GetEnabledHandlersList() {
  const base::Value* const pref_value =
      local_state_->Get(prefs::kWebAppsUrlHandlerInfo);
  if (!pref_value || !pref_value->is_dict())
    return base::Value(base::Value::Type::LIST);

  // Ensure that both keys and values are sorted.
  base::flat_map<std::u16string, base::flat_set<EnabledRow>> organizer;
  for (const auto kv : pref_value->DictItems()) {
    const auto& origin_key = kv.first;
    auto origin = url::Origin::Create(GURL(kv.first));

    for (const auto& handler : kv.second.GetList()) {
      // Only process handlers from current profile.
      if (!web_app::url_handler_prefs::IsHandlerForProfile(
              handler, profile_->GetPath())) {
        continue;
      }

      absl::optional<const web_app::url_handler_prefs::HandlerView>
          handler_view =
              web_app::url_handler_prefs::GetConstHandlerView(handler);

      const std::string& short_name =
          web_app_registrar_->GetAppShortName(handler_view->app_id);

      url::Origin app_origin = url::Origin::Create(
          GURL(web_app_registrar_->GetAppStartUrl(handler_view->app_id)));

      std::u16string publisher = url_formatter::FormatOriginForSecurityDisplay(
          app_origin, url_formatter::SchemeDisplay::OMIT_HTTP_AND_HTTPS);

      // For every include_path that has kInApp choice, add a "row" to
      // app_entries. Each "row" has the same app_id, etc, but different path
      // information. This create a list that is easy to process in WebUI.
      for (const auto& include_path_dict :
           handler_view->include_paths.GetList()) {
        const std::string* path = include_path_dict.FindStringKey("path");
        absl::optional<int> choice = include_path_dict.FindIntKey("choice");
        if (!path || !choice)
          continue;

        // Only show entries that open in app.
        if (*choice != static_cast<int>(web_app::UrlHandlerSavedChoice::kInApp))
          continue;

        EnabledRow app_row;
        app_row.origin_key = origin_key;
        app_row.app_id = handler_view->app_id;
        app_row.short_name = short_name;
        app_row.publisher = publisher;
        app_row.has_origin_wildcard = handler_view->has_origin_wildcard;
        app_row.path = *path;

        std::u16string origin_str =
            FormatOriginString(origin, handler_view->has_origin_wildcard);

        // Only include a formatted URL with path if the path is not /*.
        // Eg. example.com/* only needs to display example.com, while
        // example.com/abc needs to be displayed in full.
        if (*path != "/*")
          app_row.display_url = origin_str + base::UTF8ToUTF16(*path);

        // Add app_row to the correct bucket according to displayed origin.
        organizer[origin_str].insert(app_row);
      }
    }
  }
  return SerializeEnabledHandlersList(organizer);
}

// Example of returned Value:
// [
//   {
//     "display_url": "https://example.com/",
//     "has_origin_wildcard": true,
//     "origin_key": "https://example.com",
//     "path": "/*"
//   }
// ]
base::Value UrlHandlersHandler::GetDisabledHandlersList() {
  const base::Value* const pref_value =
      local_state_->Get(prefs::kWebAppsUrlHandlerInfo);
  if (!pref_value || !pref_value->is_dict())
    return base::Value(base::Value::Type::LIST);

  // Ensure that values are deduplicated and sorted.
  base::flat_set<DisabledRow> organizer;
  for (const auto kv : pref_value->DictItems()) {
    const auto& origin_key = kv.first;
    url::Origin origin = url::Origin::Create(GURL(kv.first));

    for (const auto& handler : kv.second.GetList()) {
      // Only process handlers from current profile.
      if (!web_app::url_handler_prefs::IsHandlerForProfile(
              handler, profile_->GetPath())) {
        continue;
      }

      absl::optional<const web_app::url_handler_prefs::HandlerView>
          handler_view =
              web_app::url_handler_prefs::GetConstHandlerView(handler);
      if (!handler_view)
        continue;

      for (const base::Value& include_path_dict :
           handler_view->include_paths.GetList()) {
        if (!include_path_dict.is_dict())
          continue;

        absl::optional<int> choice = include_path_dict.FindIntKey("choice");
        if (!choice ||
            *choice !=
                static_cast<int>(web_app::UrlHandlerSavedChoice::kInBrowser))
          continue;

        const std::string* path = include_path_dict.FindStringKey("path");
        if (!path)
          continue;

        DisabledRow row;
        row.origin_key = origin_key;
        row.has_origin_wildcard = handler_view->has_origin_wildcard;
        row.path = *path;

        std::u16string origin_str =
            FormatOriginString(origin, handler_view->has_origin_wildcard);
        std::u16string display_url =
            *path == "/*" ? origin_str : origin_str + base::UTF8ToUTF16(*path);
        row.display_url = display_url;
        organizer.insert(row);
      }
    }
  }

  return SerializeDisabledHandlersList(organizer);
}

}  // namespace settings
