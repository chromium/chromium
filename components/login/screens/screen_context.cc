// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/login/screens/screen_context.h"

#include <memory>
#include <utility>

#include "base/logging.h"

namespace login {

namespace {

template <typename StringListType>
base::ListValue* StringListToListValue(const StringListType& list) {
  base::ListValue* result = new base::ListValue();
  for (auto it = list.begin(); it != list.end(); ++it) {
    result->AppendString(*it);
  }
  return result;
}

}  // namespace

ScreenContext::ScreenContext() {
}

ScreenContext::~ScreenContext() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

bool ScreenContext::SetBoolean(const KeyType& key, bool value) {
  return Set(key, new base::Value(value));
}

bool ScreenContext::SetInteger(const KeyType& key, int value) {
  return Set(key, new base::Value(value));
}

bool ScreenContext::SetDouble(const KeyType& key, double value) {
  return Set(key, new base::Value(value));
}

bool ScreenContext::SetString(const KeyType& key, const std::string& value) {
  return Set(key, new base::Value(value));
}

bool ScreenContext::SetString(const KeyType& key, const base::string16& value) {
  return Set(key, new base::Value(value));
}

bool ScreenContext::SetStringList(const KeyType& key, const StringList& value) {
  return Set(key, StringListToListValue(value));
}

bool ScreenContext::SetString16List(const KeyType& key,
                                    const String16List& value) {
  return Set(key, StringListToListValue(value));
}

bool ScreenContext::GetBoolean(const KeyType& key) const {
  return Get<bool>(key);
}

bool ScreenContext::GetBoolean(const KeyType& key, bool default_value) const {
  return Get(key, default_value);
}

int ScreenContext::GetInteger(const KeyType& key) const {
  return Get<int>(key);
}

int ScreenContext::GetInteger(const KeyType& key, int default_value) const {
  return Get(key, default_value);
}

double ScreenContext::GetDouble(const KeyType& key) const {
  return Get<double>(key);
}

double ScreenContext::GetDouble(const KeyType& key,
                                double default_value) const {
  return Get(key, default_value);
}

std::string ScreenContext::GetString(const KeyType& key) const {
  return Get<std::string>(key);
}

std::string ScreenContext::GetString(const KeyType& key,
                                     const std::string& default_value) const {
  return Get(key, default_value);
}

base::string16 ScreenContext::GetString16(const KeyType& key) const {
  return Get<base::string16>(key);
}

base::string16 ScreenContext::GetString16(
    const KeyType& key,
    const base::string16& default_value) const {
  return Get(key, default_value);
}

StringList ScreenContext::GetStringList(const KeyType& key) const {
  return Get<StringList>(key);
}

StringList ScreenContext::GetStringList(const KeyType& key,
                                        const StringList& default_value) const {
  return Get(key, default_value);
}

String16List ScreenContext::GetString16List(const KeyType& key) const {
  return Get<String16List>(key);
}

String16List ScreenContext::GetString16List(
    const KeyType& key,
    const String16List& default_value) const {
  return Get(key, default_value);
}

void ScreenContext::CopyFrom(ScreenContext& context) {
  std::unique_ptr<base::DictionaryValue> storage(context.storage_.DeepCopy());
  std::unique_ptr<base::DictionaryValue> changes(context.changes_.DeepCopy());
  storage_.Swap(storage.get());
  changes_.Swap(changes.get());
}

bool ScreenContext::HasKey(const KeyType& key) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return storage_.HasKey(key);
}

bool ScreenContext::HasChanges() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return !changes_.empty();
}

void ScreenContext::GetChangesAndReset(base::DictionaryValue* diff) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(diff);
  changes_.Swap(diff);
  changes_.Clear();
}

void ScreenContext::ApplyChanges(const base::DictionaryValue& diff,
                                 std::vector<std::string>* keys) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!HasChanges());
  if (keys) {
    keys->clear();
    keys->reserve(diff.size());
  }

  for (base::DictionaryValue::Iterator it(diff); !it.IsAtEnd(); it.Advance()) {
    Set(it.key(), it.value().DeepCopy());
    if (keys)
      keys->push_back(it.key());
  }
  changes_.Clear();
}

bool ScreenContext::Set(const KeyType& key, base::Value* value) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(value);
  std::unique_ptr<base::Value> new_value(value);

  base::Value* current_value;
  bool in_storage = storage_.Get(key, &current_value);

  // Don't do anything if |storage_| already contains <|key|, |new_value|> pair.
  if (in_storage && new_value->Equals(current_value))
    return false;

  changes_.Set(key, std::make_unique<base::Value>(new_value->Clone()));
  storage_.Set(key, std::move(new_value));
  return true;
}

}  // namespace login
