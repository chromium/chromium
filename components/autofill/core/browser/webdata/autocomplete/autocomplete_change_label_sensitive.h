// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOCOMPLETE_AUTOCOMPLETE_CHANGE_LABEL_SENSITIVE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOCOMPLETE_AUTOCOMPLETE_CHANGE_LABEL_SENSITIVE_H_

#include <vector>

#include "components/autofill/core/browser/webdata/autocomplete/autocomplete_entry_label_sensitive.h"

namespace autofill {

class AutocompleteChangeLabelSensitive {
 public:
  enum Type { ADD, UPDATE, REMOVE, EXPIRE };

  AutocompleteChangeLabelSensitive(Type type,
                                   const AutocompleteKeyLabelSensitive& key);
  ~AutocompleteChangeLabelSensitive();

  Type type() const { return type_; }
  const AutocompleteKeyLabelSensitive& key() const { return key_; }

  bool operator==(const AutocompleteChangeLabelSensitive&) const = default;

 private:
  Type type_;
  AutocompleteKeyLabelSensitive key_;
};

using AutocompleteChangeLabelSensitiveList =
    std::vector<AutocompleteChangeLabelSensitive>;

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOCOMPLETE_AUTOCOMPLETE_CHANGE_LABEL_SENSITIVE_H_
