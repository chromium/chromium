// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_MODEL_UI_MODE_H_
#define CHROME_BROWSER_VR_MODEL_UI_MODE_H_

namespace vr {

enum UiMode {
  // Opaque modes. These modes should hide previous opaque UiMode.
  kModeBrowsing,
  kModeFullscreen,
  kModeWebVr,
  kModeVoiceSearch,
  kModeEditingOmnibox,

  // Translucent modes. These modes should NOT hide previous opaque UiMode.
  // This is useful for modal style UiMode which should not hide kModeBrowsing
  // for example.
  kModeRepositionWindow,
  kModeModalPrompt,
  kModeVoiceSearchListening,
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_MODEL_UI_MODE_H_
