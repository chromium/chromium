// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ANDROID_AUTOFILL_BROWSER_ANDROID_AUTOFILL_PROVIDER_BRIDGE_IMPL_H_
#define COMPONENTS_ANDROID_AUTOFILL_BROWSER_ANDROID_AUTOFILL_PROVIDER_BRIDGE_IMPL_H_

#include <jni.h>

#include <optional>
#include <string>

#include "base/android/jni_weak_ref.h"
#include "base/containers/span.h"
#include "base/memory/raw_ref.h"
#include "components/android_autofill/browser/android_autofill_provider_bridge.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom.h"

namespace autofill {

class AndroidAutofillProviderBridgeImpl : public AndroidAutofillProviderBridge {
 public:
  explicit AndroidAutofillProviderBridgeImpl(Delegate* delegate);
  ~AndroidAutofillProviderBridgeImpl() override;

  // AndroidAutofillProviderBridge:
  void AttachToJavaAutofillProvider(
      JNIEnv* env,
      const base::android::JavaRef<jobject>& jcaller) override;
  void SendPrefillRequest(FormDataAndroid& form) override;
  void StartAutofillSession(FormDataAndroid& form,
                            const FieldInfo& field,
                            bool has_server_predictions) override;
  void OnServerPredictionsAvailable() override;
  void ShowDatalistPopup(base::span<const SelectOption> options,
                         bool is_rtl) override;
  void HideDatalistPopup() override;
  void OnFocusChanged(const std::optional<FieldInfo>& field) override;
  void OnFormFieldDidChange(const FieldInfo& field) override;
  void OnFormFieldVisibilitiesDidChange(base::span<const int> indices) override;
  void OnTextFieldDidScroll(const FieldInfo& field) override;
  void OnFormSubmitted(mojom::SubmissionSource submission_source) override;
  void OnDidFillAutofillFormData() override;
  void CancelSession() override;
  void Reset() override;

  // Called by Java:

  // Resets the weak reference to its Java counterpart. Invoked when the
  // WebContents associated with the Java `AutofillProvider` is changed or the
  // Java `AutofillProvider` is destroyed. It indicates that the Java Peer is
  // about to be destroyed.
  void DetachFromJavaAutofillProvider(JNIEnv* env);

  // Informs the `Delegate` that the linked form should be sent to the renderer
  // for filling. Invoked when the user has accepted Autofill.
  void OnAutofillAvailable(JNIEnv* env);

  // Informs the `Delegate` that the datalist `value` should be accepted in the
  // renderer. Invoked when the user has accepted a datalist entry.
  void OnAcceptDataListSuggestion(JNIEnv* env, std::u16string value);

  // Informs the `Delegate` that the `WebContents`' native view should set the
  // anchor rect for `anchor_view` to the specified bounds. Invoked when opening
  // a datalist popup.
  void SetAnchorViewRect(JNIEnv* env,
                         jobject anchor_view,
                         jfloat x,
                         jfloat y,
                         jfloat width,
                         jfloat height);

  // Informs the `Delegate` of the outcome of an attempt to show a bottom sheet.
  // `is_shown` indicates whether the bottom sheet was shown and
  // `provided_autofill_structure` describes whether an Autofill ViewStructure
  // was provided to the Autofill framework prior to showing the bottom sheet.
  void OnShowBottomSheetResult(JNIEnv* env,
                               jboolean is_shown,
                               jboolean provided_autofill_structure);

 private:
  // The delegate of the bridge.
  raw_ref<Delegate> delegate_;

  // The Java counterpart of `this`.
  JavaObjectWeakGlobalRef java_ref_;
};

}  // namespace autofill

#endif  // COMPONENTS_ANDROID_AUTOFILL_BROWSER_ANDROID_AUTOFILL_PROVIDER_BRIDGE_IMPL_H_
