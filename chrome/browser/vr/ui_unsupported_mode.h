// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_UI_UNSUPPORTED_MODE_H_
#define CHROME_BROWSER_VR_UI_UNSUPPORTED_MODE_H_

namespace vr {

// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.vr
enum class UiUnsupportedMode : int {
  kUnhandledCodePoint = 0,
  // kCouldNotElideURL = 1,  // Obsolete.
  kUnhandledPageInfo = 2,
  // kURLWithStrongRTLChars = 3,  // Obsolete.
  kVoiceSearchNeedsRecordAudioOsPermission = 4,  // TODO(ddorwin): Android only.
  kGenericUnsupportedFeature = 5,
  kNeedsKeyboardUpdate = 6,
  kSearchEnginePromo = 7,
  // kUnhandledConnectionInfo = 8,  // Obsolete.
  kUnhandledCertificateInfo = 9,
  kUnhandledConnectionSecurityInfo = 10,
  // This must be last.
  kCount,
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_UI_UNSUPPORTED_MODE_H_
