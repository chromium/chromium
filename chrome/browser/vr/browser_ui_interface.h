// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_BROWSER_UI_INTERFACE_H_
#define CHROME_BROWSER_VR_BROWSER_UI_INTERFACE_H_

#include <memory>

#include "chrome/browser/vr/assets_load_status.h"
#include "chrome/browser/vr/model/capturing_state_model.h"
#include "chrome/browser/vr/ui_unsupported_mode.h"
#include "chrome/browser/vr/vr_export.h"
#include "components/security_state/core/security_state.h"

namespace base {
class Version;
}  // namespace base

namespace vr {

struct Assets;
struct KeyboardTestInput;
struct OmniboxSuggestions;
struct ToolbarState;

// The browser communicates state changes to the VR UI via this interface.
// A GL thread would also implement this interface to provide a convenient way
// to call these methods from the main thread.
class VR_EXPORT BrowserUiInterface {
 public:
  virtual ~BrowserUiInterface() {}

  virtual void SetWebVrMode(bool enabled) = 0;
  virtual void SetFullscreen(bool enabled) = 0;
  virtual void SetToolbarState(const ToolbarState& state) = 0;
  virtual void SetIncognito(bool enabled) = 0;
  virtual void SetLoading(bool loading) = 0;
  virtual void SetLoadProgress(float progress) = 0;
  virtual void SetHistoryButtonsEnabled(bool can_go_back,
                                        bool can_go_forward) = 0;
  virtual void SetCapturingState(
      const CapturingStateModel& active_capturing,
      const CapturingStateModel& background_capturing,
      const CapturingStateModel& potential_capturing) = 0;
  virtual void ShowExitVrPrompt(UiUnsupportedMode reason) = 0;
  virtual void SetSpeechRecognitionEnabled(bool enabled) = 0;
  virtual void SetRecognitionResult(const base::string16& result) = 0;
  virtual void OnSpeechRecognitionStateChanged(int new_state) = 0;
  virtual void SetOmniboxSuggestions(
      std::unique_ptr<OmniboxSuggestions> suggestions) = 0;
  virtual void OnAssetsLoaded(AssetsLoadStatus status,
                              std::unique_ptr<Assets> assets,
                              const base::Version& component_version) = 0;
  virtual void OnAssetsUnavailable() = 0;
  virtual void WaitForAssets() = 0;
  virtual void SetOverlayTextureEmpty(bool empty) = 0;
  virtual void ShowSoftInput(bool show) = 0;
  virtual void UpdateWebInputIndices(int selection_start,
                                     int selection_end,
                                     int composition_start,
                                     int composition_end) = 0;
  virtual void OnSwapContents(int new_content_id) = 0;
  virtual void SetDialogLocation(float x, float y) = 0;
  virtual void SetDialogFloating(bool floating) = 0;
  virtual void ShowPlatformToast(const base::string16& text) = 0;
  virtual void CancelPlatformToast() = 0;
  virtual void OnContentBoundsChanged(int width, int height) = 0;
  virtual void AddOrUpdateTab(int id,
                              bool incognito,
                              const base::string16& title) = 0;
  virtual void RemoveTab(int id, bool incognito) = 0;
  virtual void RemoveAllTabs() = 0;
  virtual void PerformKeyboardInputForTesting(
      KeyboardTestInput keyboard_input) = 0;
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_BROWSER_UI_INTERFACE_H_
