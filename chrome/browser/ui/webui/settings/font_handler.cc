// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/font_handler.h"

#include <stddef.h>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/i18n/rtl.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/font_list_async.h"
#include "content/public/browser/web_ui.h"

#if BUILDFLAG(IS_MAC)
#include "chrome/browser/ui/webui/settings/settings_utils.h"
#endif

namespace settings {

FontHandler::FontHandler(Profile* profile) {
#if BUILDFLAG(IS_MAC)
  // Perform validation for saved fonts.
  settings_utils::ValidateSavedFonts(profile->GetPrefs());
#endif
}

FontHandler::~FontHandler() {}

void FontHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "fetchFontsData", base::BindRepeating(&FontHandler::HandleFetchFontsData,
                                            base::Unretained(this)));
}

void FontHandler::OnJavascriptAllowed() {}

void FontHandler::OnJavascriptDisallowed() {
  weak_ptr_factory_.InvalidateWeakPtrs();
}

void FontHandler::HandleFetchFontsData(const base::Value::List& args) {
  CHECK_EQ(1U, args.size());
  const std::string& callback_id = args[0].GetString();

  AllowJavascript();
  content::GetFontListAsync(base::BindOnce(&FontHandler::FontListHasLoaded,
                                           weak_ptr_factory_.GetWeakPtr(),
                                           callback_id));
}

void FontHandler::FontListHasLoaded(std::string callback_id,
                                    base::Value::List list) {
  // Font list. Selects the directionality for the fonts in the given list.
  for (auto& i : list) {
    DCHECK(i.is_list());
    base::Value::List& font = i.GetList();

    DCHECK(font.size() >= 2u && font[1].is_string());
    std::u16string value = base::UTF8ToUTF16(font[1].GetString());

    bool has_rtl_chars = base::i18n::StringContainsStrongRTLChars(value);
    font.Append(has_rtl_chars ? "rtl" : "ltr");
  }

  base::Value::Dict response;
  response.Set("fontList", std::move(list));

  ResolveJavascriptCallback(base::Value(callback_id), response);
}

}  // namespace settings
