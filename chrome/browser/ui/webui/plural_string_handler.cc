// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/plural_string_handler.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/values.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/web_ui.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/strings/grit/ui_strings.h"

PluralStringHandler::PluralStringHandler() {}

PluralStringHandler::~PluralStringHandler() {}

void PluralStringHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "getPluralString",
      base::BindRepeating(&PluralStringHandler::HandleGetPluralString,
                          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "getPluralStringTupleWithComma",
      base::BindRepeating(
          &PluralStringHandler::HandleGetPluralStringTupleWithComma,
          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "getPluralStringTupleWithPeriods",
      base::BindRepeating(
          &PluralStringHandler::HandleGetPluralStringTupleWithPeriods,
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

  auto string = GetPluralizedStringForMessageName(message_name, count);

  ResolveJavascriptCallback(*callback_id, base::Value(string));
}

void PluralStringHandler::HandleGetPluralStringTupleWithComma(
    const base::ListValue* args) {
  GetPluralStringTuple(args, IDS_CONCAT_TWO_STRINGS_WITH_COMMA);
}

void PluralStringHandler::HandleGetPluralStringTupleWithPeriods(
    const base::ListValue* args) {
  GetPluralStringTuple(args, IDS_CONCAT_TWO_STRINGS_WITH_PERIODS);
}

void PluralStringHandler::GetPluralStringTuple(const base::ListValue* args,
                                               int string_tuple_id) {
  AllowJavascript();
  CHECK_EQ(5U, args->GetSize());
  const base::Value* callback_id;
  CHECK(args->Get(0, &callback_id));

  std::string message_name1;
  CHECK(args->GetString(1, &message_name1));

  int count1;
  CHECK(args->GetInteger(2, &count1));

  std::string message_name2;
  CHECK(args->GetString(3, &message_name2));

  int count2;
  CHECK(args->GetInteger(4, &count2));

  auto string1 = GetPluralizedStringForMessageName(message_name1, count1);
  auto string2 = GetPluralizedStringForMessageName(message_name2, count2);

  ResolveJavascriptCallback(
      *callback_id, base::Value(l10n_util::GetStringFUTF8(string_tuple_id,
                                                          string1, string2)));
}

std::u16string PluralStringHandler::GetPluralizedStringForMessageName(
    std::string message_name,
    int count) {
  auto message_id_it = name_to_id_.find(message_name);
  CHECK(name_to_id_.end() != message_id_it);
  return l10n_util::GetPluralStringFUTF16(message_id_it->second, count);
}
