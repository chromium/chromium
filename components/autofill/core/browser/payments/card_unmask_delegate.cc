// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/card_unmask_delegate.h"

namespace autofill {

CardUnmaskDelegate::UserProvidedUnmaskDetails::UserProvidedUnmaskDetails() =
    default;

CardUnmaskDelegate::UserProvidedUnmaskDetails::UserProvidedUnmaskDetails(
    const UserProvidedUnmaskDetails& other) = default;

CardUnmaskDelegate::UserProvidedUnmaskDetails::~UserProvidedUnmaskDetails() =
    default;

}  // namespace autofill
