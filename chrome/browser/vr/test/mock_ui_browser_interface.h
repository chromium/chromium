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
  MOCK_METHOD0(ExitFullscreen, void());
  MOCK_METHOD2(Navigate, void(GURL gurl, NavigationMethod method));
  MOCK_METHOD0(NavigateBack, void());
  MOCK_METHOD0(NavigateForward, void());
  MOCK_METHOD0(ReloadTab, void());
  MOCK_METHOD1(OpenNewTab, void(bool));
  MOCK_METHOD0(OpenBookmarks, void());
  MOCK_METHOD0(OpenRecentTabs, void());
  MOCK_METHOD0(OpenHistory, void());
  MOCK_METHOD0(OpenDownloads, void());
  MOCK_METHOD0(OpenShare, void());
  MOCK_METHOD0(OpenSettings, void());
  MOCK_METHOD2(CloseTab, void(int id, bool incognito));
  MOCK_METHOD0(CloseAllTabs, void());
  MOCK_METHOD0(CloseAllIncognitoTabs, void());
  MOCK_METHOD0(OpenFeedback, void());
  MOCK_METHOD0(CloseHostedDialog, void());
  MOCK_METHOD1(OnUnsupportedMode, void(UiUnsupportedMode mode));
  MOCK_METHOD2(OnExitVrPromptResult,
               void(ExitVrPromptChoice choice, UiUnsupportedMode reason));
  MOCK_METHOD1(OnContentScreenBoundsChanged, void(const gfx::SizeF& bounds));
  MOCK_METHOD1(SetVoiceSearchActive, void(bool active));
  MOCK_METHOD1(StartAutocomplete, void(const AutocompleteRequest& request));
  MOCK_METHOD0(StopAutocomplete, void());
  MOCK_METHOD0(ShowPageInfo, void());
  MOCK_METHOD0(LoadAssets, void());
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_TEST_MOCK_UI_BROWSER_INTERFACE_H_
