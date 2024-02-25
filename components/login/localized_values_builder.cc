// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/login/localized_values_builder.h"

#include "base/values.h"
#include "ui/base/l10n/l10n_util.h"

namespace login {

LocalizedValuesBuilder::LocalizedValuesBuilder(base::Value::Dict* dict)
    : dict_(dict) {}

void LocalizedValuesBuilder::Add(const std::string& key,
                                 const std::string& message) {
  dict_->Set(key, message);
}

void LocalizedValuesBuilder::Add(const std::string& key,
                                 const std::u16string& message) {
  dict_->Set(key, message);
}

void LocalizedValuesBuilder::Add(const std::string& key, int message_id) {
  dict_->Set(key, l10n_util::GetStringUTF16(message_id));
}

void LocalizedValuesBuilder::AddF(const std::string& key,
                                  int message_id,
                                  const std::u16string& a) {
  dict_->Set(key, l10n_util::GetStringFUTF16(message_id, a));
}

void LocalizedValuesBuilder::AddF(const std::string& key,
                                  int message_id,
                                  const std::u16string& a,
                                  const std::u16string& b) {
  dict_->Set(key, l10n_util::GetStringFUTF16(message_id, a, b));
}

void LocalizedValuesBuilder::AddF(const std::string& key,
                                  int message_id,
                                  const std::u16string& a,
                                  const std::u16string& b,
                                  const std::u16string& c) {
  dict_->Set(key, l10n_util::GetStringFUTF16(message_id, a, b, c));
}

void LocalizedValuesBuilder::AddF(const std::string& key,
                                  int message_id,
                                  int message_id_a) {
  AddF(key, message_id, l10n_util::GetStringUTF16(message_id_a));
}

}  // namespace login
