// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_TEST_MOCK_BROWSER_UI_INTERFACE_H_
#define CHROME_BROWSER_VR_TEST_MOCK_BROWSER_UI_INTERFACE_H_

#include "chrome/browser/vr/browser_ui_interface.h"
#include "chrome/browser/vr/model/assets.h"
#include "chrome/browser/vr/model/location_bar_state.h"
#include "chrome/browser/vr/model/omnibox_suggestions.h"
#include "chrome/browser/vr/ui_test_input.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace vr {

class MockBrowserUiInterface : public BrowserUiInterface {
 public:
  MockBrowserUiInterface();

  MockBrowserUiInterface(const MockBrowserUiInterface&) = delete;
  MockBrowserUiInterface& operator=(const MockBrowserUiInterface&) = delete;

  ~MockBrowserUiInterface() override;

  MOCK_METHOD1(SetWebVrMode, void(bool enabled));
  MOCK_METHOD1(SetFullscreen, void(bool enabled));
  MOCK_METHOD1(SetLocationBarState, void(const LocationBarState& state));
  MOCK_METHOD1(SetIncognito, void(bool enabled));
  MOCK_METHOD1(SetLoading, void(bool loading));
  MOCK_METHOD1(SetLoadProgress, void(float progress));
  MOCK_METHOD0(SetIsExiting, void());
  MOCK_METHOD2(SetHistoryButtonsEnabled,
               void(bool can_go_back, bool can_go_forward));
  MOCK_METHOD3(SetCapturingState,
               void(const CapturingStateModel& state,
                    const CapturingStateModel& background_state,
                    const CapturingStateModel& potential_state));
  MOCK_METHOD1(ShowExitVrPrompt, void(UiUnsupportedMode reason));
  MOCK_METHOD1(SetSpeechRecognitionEnabled, void(bool enabled));
  MOCK_METHOD1(SetHasOrCanRequestRecordAudioPermission,
               void(bool has_or_can_request_record_audio_permission));
  MOCK_METHOD1(SetRecognitionResult, void(const std::u16string& result));
  MOCK_METHOD1(OnSpeechRecognitionStateChanged, void(int new_state));
  void SetOmniboxSuggestions(
      std::vector<OmniboxSuggestion> suggestions) override {}
  void OnAssetsLoaded(AssetsLoadStatus status,
                      std::unique_ptr<Assets> assets,
                      const base::Version& component_version) override {}
  MOCK_METHOD0(OnAssetsUnavailable, void());
  MOCK_METHOD0(WaitForAssets, void());
  MOCK_METHOD1(SetRegularTabsOpen, void(bool));
  MOCK_METHOD1(SetIncognitoTabsOpen, void(bool));
  MOCK_METHOD1(SetOverlayTextureEmpty, void(bool));
  MOCK_METHOD1(ShowSoftInput, void(bool));
  MOCK_METHOD4(UpdateWebInputIndices, void(int, int, int, int));
  MOCK_METHOD1(OnSwapContents, void(int));
  MOCK_METHOD2(SetDialogLocation, void(float, float));
  MOCK_METHOD1(SetDialogFloating, void(bool));
  MOCK_METHOD1(ShowPlatformToast, void(const std::u16string&));
  MOCK_METHOD0(CancelPlatformToast, void());
  MOCK_METHOD2(OnContentBoundsChanged, void(int, int));
  MOCK_METHOD0(RemoveAllTabs, void());
  MOCK_METHOD1(PerformKeyboardInputForTesting, void(KeyboardTestInput));
  MOCK_METHOD1(SetVisibleExternalPromptNotification,
               void(ExternalPromptNotificationType));
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_TEST_MOCK_BROWSER_UI_INTERFACE_H_
