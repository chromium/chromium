// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/autofill_change.h"


namespace autofill {

AutocompleteChange::AutocompleteChange(Type type, const AutocompleteKey& key)
    : type_(type), key_(key) {}

AutocompleteChange::~AutocompleteChange() = default;

}  // namespace autofill
