// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_EXTENSIONS_WINDOWING_INTERNAL_WINDOW_CONTROLLER_LIST_OBSERVER_FOR_TESTING_H_
#define CHROME_BROWSER_UI_ANDROID_EXTENSIONS_WINDOWING_INTERNAL_WINDOW_CONTROLLER_LIST_OBSERVER_FOR_TESTING_H_

#include "base/memory/raw_ref.h"
#include "chrome/browser/extensions/window_controller_list_observer.h"

namespace extensions {
class WindowController;
}  // namespace extensions

// A singleton |WindowControllerListObserver| for tests to observe window
// events received by extension internals.
//
// The reasons we make this a singleton are:
// (1) Tests can observe events received by all windows; and
// (2) Allow the observer to exist before window creation and after window
// destruction so that we can observe those events.
//
// This will help Java integration tests more than native unit tests:
//
// For native unit tests, it's more straightforward to use
// |extensions::MockWindowControllerListObserver|.
//
// For Java integration tests (chrome_public_test_apk), we don't have a
// native "test only" library (.so) in the APK that can let Java tests set up
// and observe native objects, so we need to create a fake
// |extensions::WindowControllerListObserver| in prod code, where we can't
// use mocks.
class WindowControllerListObserverForTesting final
    : public extensions::WindowControllerListObserver {
 public:
  // Returns the singleton instance.
  static WindowControllerListObserverForTesting* GetInstance();

  // Implements |WindowControllerListObserver|.
  void OnWindowControllerAdded(
      extensions::WindowController* window_controller) override;
  void OnWindowControllerRemoved(
      extensions::WindowController* window_controller) override;
  void OnWindowBoundsChanged(
      extensions::WindowController* window_controller) override;
  void OnWindowFocusChanged(extensions::WindowController* window_controller,
                            bool has_focus) override;
};

// Events to be relayed by |WindowControllerListObserverForTesting| so that
// tests can verify the events.
//
// GENERATED_JAVA_ENUM_PACKAGE: (
//   org.chromium.chrome.browser.ui.extensions.windowing)
enum class ExtensionInternalWindowEventForTesting {
  UNKNOWN,
  BOUNDS_CHANGED,
  FOCUS_OBTAINED,
  FOCUS_LOST,
  CREATED,
  REMOVED
};

#endif  // CHROME_BROWSER_UI_ANDROID_EXTENSIONS_WINDOWING_INTERNAL_WINDOW_CONTROLLER_LIST_OBSERVER_FOR_TESTING_H_
