// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/touch_to_fill_delegate.h"

namespace autofill {

TouchToFillDelegate::TouchToFillDelegate() = default;
TouchToFillDelegate::~TouchToFillDelegate() = default;

bool TouchToFillDelegate::TryToShowTouchToFill(int query_id,
                                               const FormData& form,
                                               const FormFieldData& field) {
  // TODO(crbug.com/1247698): Add eligibility checks and trigger TTF.
  return false;
}

}  // namespace autofill
