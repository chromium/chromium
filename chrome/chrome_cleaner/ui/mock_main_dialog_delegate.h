// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_UI_MOCK_MAIN_DIALOG_DELEGATE_H_
#define CHROME_CHROME_CLEANER_UI_MOCK_MAIN_DIALOG_DELEGATE_H_

#include "chrome/chrome_cleaner/ui/main_dialog_api.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chrome_cleaner {

class MockMainDialogDelegate : public MainDialogDelegate {
 public:
  MockMainDialogDelegate();
  ~MockMainDialogDelegate() override;

  MOCK_METHOD1(AcceptedCleanup, void(bool confirmed));
  MOCK_METHOD0(OnClose, void());
};

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_UI_MOCK_MAIN_DIALOG_DELEGATE_H_
