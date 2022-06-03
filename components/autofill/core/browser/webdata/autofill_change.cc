// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/autofill_change.h"


namespace autofill {

AutofillChange::AutofillChange(Type type, const AutofillKey& key)
    : GenericAutofillChange<AutofillKey>(type, key) {
}

AutofillChange::~AutofillChange() {
}

}  // namespace autofill
