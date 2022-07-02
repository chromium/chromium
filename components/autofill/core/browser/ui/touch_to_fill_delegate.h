// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_UI_TOUCH_TO_FILL_DELEGATE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_UI_TOUCH_TO_FILL_DELEGATE_H_

namespace autofill {

// An interface for interaction with the corresponding UI controller.
class TouchToFillDelegate {
 public:
  // TODO(crbug.com/1247698): Define the API.
  virtual ~TouchToFillDelegate() = default;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_UI_TOUCH_TO_FILL_DELEGATE_H_
