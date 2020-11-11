// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/plural_string_handler.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/values.h"
#include "content/public/browser/web_ui.h"
#include "ui/base/l10n/l10n_util.h"

PluralStringHandler::PluralStringHandler() {}

PluralStringHandler::~PluralStringHandler() {}

void PluralStringHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "getPluralString",
      base::BindRepeating(&PluralStringHandler::HandleGetPluralString,
                          base::Unretained(this)));
}

void PluralStringHandler::AddLocalizedString(const std::string& name, int id) {
  name_to_id_[name] = id;
}

void PluralStringHandler::HandleGetPluralString(const base::ListValue* args) {
  AllowJavascript();
  CHECK_EQ(3U, args->GetSize());
  const base::Value* callback_id;
  CHECK(args->Get(0, &callback_id));

  std::string message_name;
  CHECK(args->GetString(1, &message_name));

  int count;
  CHECK(args->GetInteger(2, &count));

  auto message_id_it = name_to_id_.find(message_name);
  CHECK(name_to_id_.end() != message_id_it);

  ResolveJavascriptCallback(*callback_id,
                            base::Value(l10n_util::GetPluralStringFUTF8(
                                message_id_it->second, count)));
}
