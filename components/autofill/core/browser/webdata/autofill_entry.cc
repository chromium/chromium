// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/autofill_entry.h"

#include <string>
#include <tuple>

#include "base/strings/utf_string_conversions.h"

namespace autofill {

AutofillKey::AutofillKey() {}

AutofillKey::AutofillKey(const std::u16string& name,
                         const std::u16string& value)
    : name_(name), value_(value) {}

AutofillKey::AutofillKey(const std::string& name,
                         const std::string& value)
    : name_(base::UTF8ToUTF16(name)),
      value_(base::UTF8ToUTF16(value)) {
}

AutofillKey::AutofillKey(const AutofillKey& key)
    : name_(key.name()),
      value_(key.value()) {
}

AutofillKey::~AutofillKey() {}

bool AutofillKey::operator==(const AutofillKey& key) const {
  return name_ == key.name() && value_ == key.value();
}

bool AutofillKey::operator<(const AutofillKey& key) const {
  return std::tie(name_, value_) < std::tie(key.name(), key.value());
}

AutofillEntry::AutofillEntry() {}

AutofillEntry::AutofillEntry(const AutofillKey& key,
                             const base::Time& date_created,
                             const base::Time& date_last_used)
    : key_(key),
      date_created_(date_created),
      date_last_used_(date_last_used) {}

AutofillEntry::~AutofillEntry() {}

bool AutofillEntry::operator==(const AutofillEntry& entry) const {
  return key() == entry.key() &&
         date_created() == entry.date_created() &&
         date_last_used() == entry.date_last_used();
}

bool AutofillEntry::operator!=(const AutofillEntry& entry) const {
  return !(*this == entry);
}

bool AutofillEntry::operator<(const AutofillEntry& entry) const {
  return key_ < entry.key();
}

}  // namespace autofill
