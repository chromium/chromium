// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(crbug.com/346507576): After initial testing period autofill_change.cc
// should be edited to not include AutocompleteChange. This file should be
// renamed to autocomplete_change.cc

#include "components/autofill/core/browser/webdata/autocomplete/autocomplete_change_label_sensitive.h"

namespace autofill {

AutocompleteChangeLabelSensitive::AutocompleteChangeLabelSensitive(
    Type type,
    const AutocompleteKeyLabelSensitive& key)
    : type_(type), key_(key) {}

AutocompleteChangeLabelSensitive::~AutocompleteChangeLabelSensitive() = default;

}  // namespace autofill
