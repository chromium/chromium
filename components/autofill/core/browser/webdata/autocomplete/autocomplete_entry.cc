// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/autocomplete/autocomplete_entry.h"

#include <string>

#include "base/strings/utf_string_conversions.h"

namespace autofill {

AutocompleteKey::AutocompleteKey() = default;

AutocompleteKey::AutocompleteKey(const std::u16string& name,
                         const std::u16string& value)
    : name_(name), value_(value) {}

AutocompleteKey::AutocompleteKey(const std::string& name,
                         const std::string& value)
    : name_(base::UTF8ToUTF16(name)),
      value_(base::UTF8ToUTF16(value)) {
}

AutocompleteKey::AutocompleteKey(const AutocompleteKey& key)
    : name_(key.name()),
      value_(key.value()) {
}

AutocompleteKey::~AutocompleteKey() = default;

AutocompleteEntry::AutocompleteEntry() = default;

AutocompleteEntry::AutocompleteEntry(const AutocompleteKey& key,
                                     base::Time date_created,
                                     base::Time date_last_used)
    : key_(key), date_created_(date_created), date_last_used_(date_last_used) {}

AutocompleteEntry::~AutocompleteEntry() = default;

}  // namespace autofill
