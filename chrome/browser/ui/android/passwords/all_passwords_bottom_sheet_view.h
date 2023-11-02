// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_PASSWORDS_ALL_PASSWORDS_BOTTOM_SHEET_VIEW_H_
#define CHROME_BROWSER_UI_ANDROID_PASSWORDS_ALL_PASSWORDS_BOTTOM_SHEET_VIEW_H_

#include "components/autofill/core/common/mojom/autofill_types.mojom-forward.h"

namespace password_manager {
struct PasswordForm;
}  // namespace password_manager

// This interface is used for communicating between the all
// passwords sheet controller and the Android frontend.
class AllPasswordsBottomSheetView {
 public:
  AllPasswordsBottomSheetView() = default;
  AllPasswordsBottomSheetView(const AllPasswordsBottomSheetView&) = delete;
  AllPasswordsBottomSheetView& operator=(const AllPasswordsBottomSheetView&) =
      delete;
  virtual ~AllPasswordsBottomSheetView() = default;

  // Instructs All Passwords Sheet to show the provided |credentials| to the
  // user.
  virtual void Show(
      const std::vector<std::unique_ptr<password_manager::PasswordForm>>&
          credentials,
      autofill::mojom::FocusedFieldType focused_field_type) = 0;
};

#endif  // CHROME_BROWSER_UI_ANDROID_PASSWORDS_ALL_PASSWORDS_BOTTOM_SHEET_VIEW_H_
