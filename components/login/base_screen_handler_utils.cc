// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/login/base_screen_handler_utils.h"

#include "base/strings/utf_string_conversions.h"
#include "components/account_id/account_id.h"

namespace login {

namespace {

// Helpers to let ParseListString produce output of appropriate type.
void ConvertFromUTF8(const std::string& in, std::string& out) {
  out = in;
}

void ConvertFromUTF8(const std::string& in, std::u16string& out) {
  out = base::UTF8ToUTF16(in);
}

template <typename StringListType>
bool ParseStringList(const base::Value* value, StringListType* out_value) {
  if (!value->is_list())
    return false;
  base::Value::ConstListView list = value->GetList();
  out_value->resize(list.size());
  for (size_t i = 0; i < list.size(); ++i) {
    if (!list[i].is_string())
      return false;
    ConvertFromUTF8(list[i].GetString(), (*out_value)[i]);
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
