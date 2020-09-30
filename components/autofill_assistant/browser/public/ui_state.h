// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_PUBLIC_UI_STATE_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_PUBLIC_UI_STATE_H_

namespace autofill_assistant {
// Defines if Autofill Assistant UI is shown.
enum class UIState {
  // Autofill Assistant UI is not being shown.
  kNotShown = 0,
  // Autofill Assistant UI is being shown.
  kShown
};
}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_PUBLIC_UI_STATE_H_
