// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_PLUS_ADDRESSES_PLUS_ADDRESS_CREATION_VIEW_ANDROID_H_
#define CHROME_BROWSER_UI_ANDROID_PLUS_ADDRESSES_PLUS_ADDRESS_CREATION_VIEW_ANDROID_H_

#include <jni.h>

#include "components/plus_addresses/plus_address_types.h"
#include "ui/gfx/native_widget_types.h"

class TabModel;

namespace plus_addresses {

class PlusAddressCreationController;

// A class intended as a thin wrapper around a Java object, which calls out to
// the `PlusAddressCreationControllerAndroid`. This shields the controller from
// JNI complications, allowing a consistent interface for clients (e.g.,
// autofill). Note that it is likely that either the controller will morph to do
// what this class does now, or a similar wrapper will be created for desktop,
// with a single controller implementation.
class PlusAddressCreationViewAndroid {
 public:
  explicit PlusAddressCreationViewAndroid(
      base::WeakPtr<PlusAddressCreationController> controller);
  ~PlusAddressCreationViewAndroid();

  void ShowInit(gfx::NativeView native_view,
                TabModel* tab_model,
                const std::string& primary_email_address,
                bool refresh_supported,
                bool has_accepted_notice);
  void OnRefreshClicked(JNIEnv* env,
                        const base::android::JavaParamRef<jobject>& obj);
  void OnConfirmRequested(JNIEnv* env,
                          const base::android::JavaParamRef<jobject>& obj);
  void OnCanceled(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);
  void PromptDismissed(JNIEnv* env,
                       const base::android::JavaParamRef<jobject>& obj);

  // Updates the bottomsheet to either show an error message or show the
  // plus address in the bottomsheet and enable the OK button.
  void ShowReserveResult(const PlusProfileOrError& maybe_plus_profile);
  // Either shows an error message on the bottomsheet or closes the bottomsheet.
  void ShowConfirmResult(const PlusProfileOrError& maybe_plus_profile);
  // Hides the refresh icon in case no more plus address refreshes are available
  // to the user.
  void HideRefreshButton();

 private:
  // Returns either the fully initialized java counterpart of this bridge or
  // a is_null() reference if the creation failed. By using this method, the
  // bridge will try to recreate the java object if it failed previously (e.g.
  // because there was no native window available).
  base::android::ScopedJavaGlobalRef<jobject> GetOrCreateJavaObject(
      gfx::NativeView native_view,
      TabModel* tab_model);

  // The corresponding java object.
  base::android::ScopedJavaGlobalRef<jobject> java_object_;
  base::WeakPtr<PlusAddressCreationController> controller_;
};

}  // namespace plus_addresses

#endif  // CHROME_BROWSER_UI_ANDROID_PLUS_ADDRESSES_PLUS_ADDRESS_CREATION_VIEW_ANDROID_H_
