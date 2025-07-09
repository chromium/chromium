// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/autocomplete/autocomplete_change_label_sensitive.h"

namespace autofill {

AutocompleteChangeLabelSensitive::AutocompleteChangeLabelSensitive(
    Type type,
    const AutocompleteKeyLabelSensitive& key)
    : type_(type), key_(key) {}

AutocompleteChangeLabelSensitive::~AutocompleteChangeLabelSensitive() = default;

}  // namespace autofill
