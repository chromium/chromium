// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TEST_UTIL_H_
#define CHROME_BROWSER_UI_TABS_TEST_UTIL_H_

namespace tabs {

// Keeping an instance of this class alive prevents initialization of
// TabFeatures. This is done by replacing the factory for TabFeatures, and thus
// is not compatible with other code that also replaces the factory. This should
// be called from unit tests that want to test the functionality of tabs outside
// the regular production context of a `class Browser` and `class BrowserView`.
class PreventTabFeatureInitialization {
 public:
  PreventTabFeatureInitialization();
  ~PreventTabFeatureInitialization();
};

}  // namespace tabs

#endif  // CHROME_BROWSER_UI_TABS_TEST_UTIL_H_
