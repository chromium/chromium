// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_TEST_MOCK_UI_BROWSER_INTERFACE_H_
#define CHROME_BROWSER_VR_TEST_MOCK_UI_BROWSER_INTERFACE_H_

#include "chrome/browser/vr/ui_browser_interface.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace vr {

class MockUiBrowserInterface : public UiBrowserInterface {
 public:
  MockUiBrowserInterface();

  MockUiBrowserInterface(const MockUiBrowserInterface&) = delete;
  MockUiBrowserInterface& operator=(const MockUiBrowserInterface&) = delete;

  ~MockUiBrowserInterface() override;

  MOCK_METHOD0(ExitPresent, void());
  MOCK_METHOD2(CloseTab, void(int id, bool incognito));
  MOCK_METHOD0(CloseAllTabs, void());
  MOCK_METHOD0(LoadAssets, void());
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_TEST_MOCK_UI_BROWSER_INTERFACE_H_
