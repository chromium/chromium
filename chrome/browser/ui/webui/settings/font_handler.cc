// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/font_handler.h"

#include <stddef.h>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/i18n/rtl.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/font_list_async.h"
#include "content/public/browser/web_ui.h"

#if defined(OS_MAC)
#include "chrome/browser/ui/webui/settings/settings_utils.h"
#endif

namespace settings {

FontHandler::FontHandler(Profile* profile) {
#if defined(OS_MAC)
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

void FontHandler::OnJavascriptDisallowed() {}

void FontHandler::HandleFetchFontsData(const base::ListValue* args) {
  CHECK_EQ(1U, args->GetSize());
  std::string callback_id;
  CHECK(args->GetString(0, &callback_id));

  AllowJavascript();
  content::GetFontListAsync(base::BindOnce(&FontHandler::FontListHasLoaded,
                                           weak_ptr_factory_.GetWeakPtr(),
                                           callback_id));
}

void FontHandler::FontListHasLoaded(std::string callback_id,
                                    std::unique_ptr<base::ListValue> list) {
  // Font list. Selects the directionality for the fonts in the given list.
  for (size_t i = 0; i < list->GetSize(); i++) {
    base::ListValue* font;
    bool has_font = list->GetList(i, &font);
    DCHECK(has_font);

    std::u16string value;
    bool has_value = font->GetString(1, &value);
    DCHECK(has_value);

    bool has_rtl_chars = base::i18n::StringContainsStrongRTLChars(value);
    font->AppendString(has_rtl_chars ? "rtl" : "ltr");
  }

  base::DictionaryValue response;
  response.Set("fontList", std::move(list));

  ResolveJavascriptCallback(base::Value(callback_id), response);
}

}  // namespace settings
