// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/font_handler.h"

#include <stddef.h>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/i18n/rtl.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/font_list_async.h"
#include "content/public/browser/web_ui.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension_urls.h"

#if defined(OS_MACOSX)
#include "chrome/browser/ui/webui/settings_utils.h"
#endif

namespace {

const char kAdvancedFontSettingsExtensionId[] =
    "caclkomlalccbpcdllchkeecicepbmbm";

}  // namespace

namespace settings {

FontHandler::FontHandler(content::WebUI* webui)
    : profile_(Profile::FromWebUI(webui)) {
#if defined(OS_MACOSX)
  // Perform validation for saved fonts.
  settings_utils::ValidateSavedFonts(profile_->GetPrefs());
#endif
}

FontHandler::~FontHandler() {}

void FontHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "fetchFontsData", base::BindRepeating(&FontHandler::HandleFetchFontsData,
                                            base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "observeAdvancedFontExtensionAvailable",
      base::BindRepeating(
          &FontHandler::HandleObserveAdvancedFontExtensionAvailable,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "openAdvancedFontSettings",
      base::BindRepeating(&FontHandler::HandleOpenAdvancedFontSettings,
                          base::Unretained(this)));
}

void FontHandler::OnJavascriptAllowed() {
  extension_registry_observer_.Add(
      extensions::ExtensionRegistry::Get(profile_));
}

void FontHandler::OnJavascriptDisallowed() {
  extension_registry_observer_.RemoveAll();
}

void FontHandler::HandleFetchFontsData(const base::ListValue* args) {
  CHECK_EQ(1U, args->GetSize());
  std::string callback_id;
  CHECK(args->GetString(0, &callback_id));

  content::GetFontListAsync(base::Bind(&FontHandler::FontListHasLoaded,
                                       weak_ptr_factory_.GetWeakPtr(),
                                       callback_id));
}

void FontHandler::HandleObserveAdvancedFontExtensionAvailable(
    const base::ListValue* /*args*/) {
  AllowJavascript();
  NotifyAdvancedFontSettingsAvailability();
}

void FontHandler::HandleOpenAdvancedFontSettings(
    const base::ListValue* /*args*/) {
  const extensions::Extension* extension = GetAdvancedFontSettingsExtension();
  if (!extension)
    return;
  extensions::ExtensionTabUtil::OpenOptionsPage(
      extension,
      chrome::FindBrowserWithWebContents(web_ui()->GetWebContents()));
}

const extensions::Extension* FontHandler::GetAdvancedFontSettingsExtension() {
  extensions::ExtensionService* service =
      extensions::ExtensionSystem::Get(profile_)->extension_service();
  if (!service->IsExtensionEnabled(kAdvancedFontSettingsExtensionId))
    return nullptr;
  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(profile_);
  return registry->GetInstalledExtension(kAdvancedFontSettingsExtensionId);
}

void FontHandler::NotifyAdvancedFontSettingsAvailability() {
  FireWebUIListener("advanced-font-settings-installed",
                    base::Value(GetAdvancedFontSettingsExtension() != nullptr));
}

void FontHandler::OnExtensionLoaded(content::BrowserContext*,
                                    const extensions::Extension*) {
  NotifyAdvancedFontSettingsAvailability();
}

void FontHandler::OnExtensionUnloaded(content::BrowserContext*,
                                      const extensions::Extension*,
                                      extensions::UnloadedExtensionReason) {
  NotifyAdvancedFontSettingsAvailability();
}

void FontHandler::FontListHasLoaded(std::string callback_id,
                                    std::unique_ptr<base::ListValue> list) {
  // Font list. Selects the directionality for the fonts in the given list.
  for (size_t i = 0; i < list->GetSize(); i++) {
    base::ListValue* font;
    bool has_font = list->GetList(i, &font);
    DCHECK(has_font);

    base::string16 value;
    bool has_value = font->GetString(1, &value);
    DCHECK(has_value);

    bool has_rtl_chars = base::i18n::StringContainsStrongRTLChars(value);
    font->AppendString(has_rtl_chars ? "rtl" : "ltr");
  }

  base::DictionaryValue response;
  response.Set("fontList", std::move(list));

  GURL extension_url(extension_urls::GetWebstoreItemDetailURLPrefix());
  response.SetString(
      "extensionUrl",
      extension_url.Resolve(kAdvancedFontSettingsExtensionId).spec());

  ResolveJavascriptCallback(base::Value(callback_id), response);
}

}  // namespace settings
