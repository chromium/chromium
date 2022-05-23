// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_TOUCH_TO_FILL_DELEGATE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_TOUCH_TO_FILL_DELEGATE_H_

#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"

namespace autofill {

// Delegate for in-browser Touch To Fill (TTF) surface display and selection.
// TODO(crbug.com/1324900): Consider using more descriptive name.
class TouchToFillDelegate {
 public:
  TouchToFillDelegate();
  TouchToFillDelegate(const TouchToFillDelegate&) = delete;
  TouchToFillDelegate& operator=(const TouchToFillDelegate&) = delete;
  virtual ~TouchToFillDelegate();

  // Checks whether TTF is eligible for the given web form data. On success
  // triggers the corresponding surface and returns |true|.
  virtual bool TryToShowTouchToFill(int query_id,
                                    const FormData& form,
                                    const FormFieldData& field);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_TOUCH_TO_FILL_DELEGATE_H_
