// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdint>
#include <memory>

#include "base/check.h"
#include "chrome/browser/ui/bookmarks/controllers/android/android_bookmark_bar_injector.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "components/browser_apis/ui_controllers/bookmark_bar/bookmark_bar_api.mojom-jni.h"
#include "components/browser_apis/ui_controllers/bookmark_bar/bookmark_bar_ui_controller.h"
#include "mojo/public/cpp/bindings/mojo_jni_bridge.h"
#include "ui/events/android/event_flags_android.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/bookmarks/android/jni_headers/BookmarkBarControllerFeature_jni.h"

namespace {

using ControllerBridge =
    mojo::MojoJniBridge<bookmark_bar_api::BookmarkBarUIController>;

}  // namespace

// Returns a bridge that owns `browser`'s new controller. `browser` outlives it:
// `ChromeAndroidTask` removes the owning Java feature first.
static int64_t JNI_BookmarkBarControllerFeature_CreateController(
    JNIEnv* env,
    BrowserWindowInterface* browser) {
  CHECK(browser);
  mojo::internal::MojoJniBridgeBase* bridge = new ControllerBridge(
      AndroidBookmarkBarInjector::CreateBookmarkBarUIController(browser)
          .release());
  return reinterpret_cast<int64_t>(bridge);
}

static void JNI_BookmarkBarControllerFeature_DestroyController(
    JNIEnv* env,
    int64_t controller_bridge) {
  std::unique_ptr<ControllerBridge> bridge(static_cast<ControllerBridge*>(
      reinterpret_cast<mojo::internal::MojoJniBridgeBase*>(controller_bridge)));
  std::unique_ptr<bookmark_bar_api::BookmarkBarUIController> controller(
      bridge->stub());
  // The bridge keeps a `raw_ptr` to the controller, so delete it first.
  bridge.reset();
}

static int32_t JNI_BookmarkBarControllerFeature_ToEventFlags(
    JNIEnv* env,
    int32_t meta_state,
    int32_t button_state) {
  return ui::EventFlagsFromAndroidMetaState(meta_state) |
         ui::EventFlagsFromAndroidButtonState(button_state);
}

DEFINE_JNI(BookmarkBarControllerFeature)
