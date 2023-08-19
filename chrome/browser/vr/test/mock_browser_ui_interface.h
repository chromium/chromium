// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_TEST_MOCK_BROWSER_UI_INTERFACE_H_
#define CHROME_BROWSER_VR_TEST_MOCK_BROWSER_UI_INTERFACE_H_

#include "chrome/browser/vr/browser_ui_interface.h"
#include "chrome/browser/vr/ui_test_input.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace vr {

class MockBrowserUiInterface : public BrowserUiInterface {
 public:
  MockBrowserUiInterface();

  MockBrowserUiInterface(const MockBrowserUiInterface&) = delete;
  MockBrowserUiInterface& operator=(const MockBrowserUiInterface&) = delete;

  ~MockBrowserUiInterface() override;

  MOCK_METHOD3(SetCapturingState,
               void(const CapturingStateModel& state,
                    const CapturingStateModel& background_state,
                    const CapturingStateModel& potential_state));
  MOCK_METHOD1(SetVisibleExternalPromptNotification,
               void(ExternalPromptNotificationType));
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_TEST_MOCK_BROWSER_UI_INTERFACE_H_
