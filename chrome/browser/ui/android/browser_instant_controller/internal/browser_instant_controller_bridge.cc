// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/browser_instant_controller/internal/browser_instant_controller_bridge.h"

#include <jni.h>

#include <memory>

#include "base/check_deref.h"
#include "base/feature_list.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/ui/android/browser_instant_controller/internal/jni/BrowserInstantControllerBridgeImpl_jni.h"
#include "chrome/browser/ui/browser_instant_controller.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "components/search/search.h"
#include "third_party/jni_zero/jni_zero.h"

namespace {

// Returns true if the NTP tabs of a window of `profile` have to be reloaded
// when the default search engine changes.
//
// This is the Android counterpart of the gating in
// `BrowserWindowFeatures::Init()` on desktop, which only requires the Instant
// Extended API. On Desktop Android the content of an NTP additionally only
// depends on the default search engine when the WebUI NTP is enabled and
// third-party search engines get their own NTP (`kUseWebUiNtp3PDSE`);
// otherwise every NTP is the same native page. Incognito NTPs never depend on
// the default search engine.
bool ShouldCreateBrowserInstantController(Profile* profile) {
  return !profile->IsOffTheRecord() &&
         search::IsWebUiNtpEnabledForDesktopAndroid() &&
         base::FeatureList::IsEnabled(chrome::android::kUseWebUiNtp3PDSE) &&
         search::IsInstantExtendedAPIEnabled();
}

}  // namespace

// Implements Java `BrowserInstantControllerBridgeImpl.Natives#create`.
static jni_zero::JniUniquePtr<BrowserInstantControllerBridge>
JNI_BrowserInstantControllerBridgeImpl_Create(
    JNIEnv* env,
    BrowserWindowInterface* browser_window) {
  return jni_zero::MakeUnique<BrowserInstantControllerBridge>(browser_window);
}

BrowserInstantControllerBridge::BrowserInstantControllerBridge(
    BrowserWindowInterface* browser_window) {
  Profile* profile = CHECK_DEREF(browser_window).GetProfile();
  if (ShouldCreateBrowserInstantController(profile)) {
    instant_controller_ =
        std::make_unique<BrowserInstantController>(*browser_window);
  }
}

BrowserInstantControllerBridge::~BrowserInstantControllerBridge() = default;

DEFINE_JNI(BrowserInstantControllerBridgeImpl)
