// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_UI_BROWSER_INTERFACE_H_
#define CHROME_BROWSER_VR_UI_BROWSER_INTERFACE_H_

#include "chrome/browser/vr/exit_vr_prompt_choice.h"
#include "chrome/browser/vr/model/omnibox_suggestions.h"
#include "chrome/browser/vr/ui_unsupported_mode.h"
#include "chrome/browser/vr/vr_base_export.h"
#include "ui/gfx/geometry/size_f.h"
#include "url/gurl.h"

namespace vr {

// A actions which can trigger the navigate function.
enum NavigationMethod {
  kOmniboxUrlEntry,
  kOmniboxSuggestionSelected,
  kVoiceSearch,
};

// An interface for the VR UI to communicate with VrShell. Many of the functions
// in this interface are proxies to methods on VrShell.
class VR_BASE_EXPORT UiBrowserInterface {
 public:
  virtual ~UiBrowserInterface() = default;

  virtual void ExitPresent() = 0;
  virtual void ExitFullscreen() = 0;
  virtual void Navigate(GURL gurl, NavigationMethod method) = 0;
  virtual void NavigateBack() = 0;
  virtual void NavigateForward() = 0;
  virtual void ReloadTab() = 0;
  virtual void OpenNewTab(bool incognito) = 0;
  virtual void OpenBookmarks() = 0;
  virtual void OpenRecentTabs() = 0;
  virtual void OpenHistory() = 0;
  virtual void OpenDownloads() = 0;
  virtual void OpenShare() = 0;
  virtual void OpenSettings() = 0;
  virtual void CloseAllIncognitoTabs() = 0;
  virtual void OpenFeedback() = 0;
  virtual void CloseHostedDialog() = 0;
  virtual void OnUnsupportedMode(UiUnsupportedMode mode) = 0;
  virtual void OnExitVrPromptResult(ExitVrPromptChoice choice,
                                    UiUnsupportedMode reason) = 0;
  virtual void OnContentScreenBoundsChanged(const gfx::SizeF& bounds) = 0;
  virtual void SetVoiceSearchActive(bool active) = 0;
  virtual void StartAutocomplete(const AutocompleteRequest& request) = 0;
  virtual void StopAutocomplete() = 0;
  virtual void ShowPageInfo() = 0;
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_UI_BROWSER_INTERFACE_H_
