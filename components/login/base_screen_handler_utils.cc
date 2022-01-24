// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/login/base_screen_handler_utils.h"

#include "base/strings/utf_string_conversions.h"
#include "components/account_id/account_id.h"

namespace login {

namespace {

template <typename StringListType>
bool ParseStringList(const base::Value* value, StringListType* out_value) {
  const base::ListValue* list = nullptr;
  if (!value->GetAsList(&list))
    return false;
  out_value->resize(list->GetList().size());
  for (size_t i = 0; i < list->GetList().size(); ++i) {
    if (!list->GetString(i, &((*out_value)[i])))
      return false;
  }
  return true;
}

}  // namespace

bool ParseValue(const base::Value* value, bool* out_value) {
  if (out_value && value->is_bool()) {
    *out_value = value->GetBool();
    return true;
  }
  return value->is_bool();
}

bool ParseValue(const base::Value* value, int* out_value) {
  if (out_value && value->is_int()) {
    *out_value = value->GetInt();
    return true;
  }
  return value->is_int();
}

bool ParseValue(const base::Value* value, double* out_value) {
  if (out_value && (value->is_double() || value->is_int())) {
    *out_value = value->GetDouble();
    return true;
  }
  return value->is_double() || value->is_int();
}

bool ParseValue(const base::Value* value, std::string* out_value) {
  if (out_value && value->is_string()) {
    *out_value = value->GetString();
    return true;
  }
  return value->is_string();
}

bool ParseValue(const base::Value* value, std::u16string* out_value) {
  if (out_value && value->is_string()) {
    *out_value = base::UTF8ToUTF16(value->GetString());
    return true;
  }
  return value->is_string();
}

bool ParseValue(const base::Value* value,
                const base::DictionaryValue** out_value) {
  if (out_value && value->is_dict()) {
    *out_value = static_cast<const base::DictionaryValue*>(value);
    return true;
  }
  return value->is_dict();
}

bool ParseValue(const base::Value* value, StringList* out_value) {
  return ParseStringList(value, out_value);
}

bool ParseValue(const base::Value* value, String16List* out_value) {
  return ParseStringList(value, out_value);
}

bool ParseValue(const base::Value* value, AccountId* out_value) {
  if (!value->is_string())
    return false;

  std::string serialized = value->GetString();
  if (AccountId::Deserialize(serialized, out_value))
    return true;

  *out_value = AccountId::FromUserEmail(serialized);
  LOG(ERROR) << "Failed to deserialize, parse as email, valid="
             << out_value->is_valid();
  return true;
}

base::Value MakeValue(bool v) {
  return base::Value(v);
}

base::Value MakeValue(int v) {
  return base::Value(v);
}

base::Value MakeValue(double v) {
  return base::Value(v);
}

base::Value MakeValue(const std::string& v) {
  return base::Value(v);
}

base::Value MakeValue(const std::u16string& v) {
  return base::Value(v);
}

base::Value MakeValue(const AccountId& v) {
  return base::Value(v.Serialize());
}

ParsedValueContainer<AccountId>::ParsedValueContainer() {
}

}  // namespace login
