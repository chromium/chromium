// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_PLUS_ADDRESSES_ALL_PLUS_ADDRESSES_BOTTOM_SHEET_VIEW_H_
#define CHROME_BROWSER_UI_ANDROID_PLUS_ADDRESSES_ALL_PLUS_ADDRESSES_BOTTOM_SHEET_VIEW_H_

#include "base/android/scoped_java_ref.h"
#include "base/containers/span.h"
#include "base/functional/callback.h"

namespace plus_addresses {

class AllPlusAddressesBottomSheetController;
struct PlusProfile;

// This bridge is used for communication between the all plus addresses bottom
// sheet controller and the Android frontend.
class AllPlusAddressesBottomSheetView final {
 public:
  explicit AllPlusAddressesBottomSheetView(
      AllPlusAddressesBottomSheetController* controller);
  ~AllPlusAddressesBottomSheetView();
  void Show(base::span<const PlusProfile> profiles);

  // Invoked in case the user chooses an entry from the plus address list
  // presented to them.
  void OnPlusAddressSelected(JNIEnv* env, const std::string& plus_address);

  // Called from Java bridge when user dismisses the BottomSheet.
  // Redirects the call to the controller.
  void OnDismissed(JNIEnv* env);

 private:
  // Returns either the fully initialized java counterpart of this bridge or
  // a is_null() reference if the creation failed. By using this method, the
  // bridge will try to recreate the java object if it failed previously (e.g.
  // because there was no native window available).
  base::android::ScopedJavaGlobalRef<jobject> GetOrCreateJavaObject();

  base::android::ScopedJavaGlobalRef<jobject> java_object_internal_;
  const raw_ref<AllPlusAddressesBottomSheetController> controller_;
};

}  // namespace plus_addresses

#endif  // CHROME_BROWSER_UI_ANDROID_PLUS_ADDRESSES_ALL_PLUS_ADDRESSES_BOTTOM_SHEET_VIEW_H_
