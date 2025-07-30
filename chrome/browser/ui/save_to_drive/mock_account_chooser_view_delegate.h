// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SAVE_TO_DRIVE_MOCK_ACCOUNT_CHOOSER_VIEW_DELEGATE_H_
#define CHROME_BROWSER_UI_SAVE_TO_DRIVE_MOCK_ACCOUNT_CHOOSER_VIEW_DELEGATE_H_

#include "chrome/browser/ui/save_to_drive/account_chooser_view_delegate.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace save_to_drive {
class MockAccountChooserViewDelegate : public AccountChooserViewDelegate {
 public:
  MockAccountChooserViewDelegate();
  ~MockAccountChooserViewDelegate() override;
  MOCK_METHOD(void, OnSaveButtonClicked, (), (override));
  MOCK_METHOD(void,
              OnUserClosedDialog,
              (int32_t widget_closed_reason),
              (override));
  MOCK_METHOD(void, OnAccountSelected, (const AccountInfo& account), (override));
};
}  // namespace save_to_drive
#endif  // CHROME_BROWSER_UI_SAVE_TO_DRIVE_MOCK_ACCOUNT_CHOOSER_VIEW_DELEGATE_H_
