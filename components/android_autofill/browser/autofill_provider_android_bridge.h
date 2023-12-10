// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ANDROID_AUTOFILL_BROWSER_AUTOFILL_PROVIDER_ANDROID_BRIDGE_H_
#define COMPONENTS_ANDROID_AUTOFILL_BROWSER_AUTOFILL_PROVIDER_ANDROID_BRIDGE_H_

#include <jni.h>

#include <string>

#include "base/containers/span.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace base::android {
template <typename T>
class JavaRef;
}  // namespace base::android

namespace gfx {
class RectF;
}  // namespace gfx

namespace autofill {

class FormDataAndroid;

// Interface for the C++ <-> Android bridge between `AutofillProviderAndroid`
// and Java `AutofillProvider`.
class AutofillProviderAndroidBridge {
 public:
  virtual ~AutofillProviderAndroidBridge() = default;

  // The delegate interface that is to be implemented by the owner of this
  // bridge. It is used to forward calls from Java to C++.
  class Delegate {
   public:
    virtual ~Delegate() = default;

    virtual void OnAutofillAvailable() = 0;
    virtual void OnAcceptDatalistSuggestion(const std::u16string& value) = 0;
    virtual void SetAnchorViewRect(
        const base::android::JavaRef<jobject>& anchor,
        const gfx::RectF& bounds) = 0;
    virtual void OnShowBottomSheetResult(bool is_shown,
                                         bool provided_autofill_structure) = 0;
  };

  // A helper struct to reference a field in a form.
  struct FieldInfo {
    // The index of the field in the array of fields of the form.
    size_t index;
    // The bounds of the field.
    gfx::RectF bounds;
  };

  // Attaches the bridge to its Java counterpart.
  virtual void AttachToJavaAutofillProvider(
      JNIEnv* env,
      const base::android::JavaRef<jobject>& jcaller) = 0;

  // Sends a prefill request to the Android Autofill framework.
  virtual void SendPrefillRequest(FormDataAndroid& form) = 0;

  // Starts a new Autofill session for `form` and `field`.
  virtual void StartAutofillSession(FormDataAndroid& form,
                                    const FieldInfo& field,
                                    bool has_server_predictions) = 0;

  // Informs the Java side that the server prediction request is completed.
  virtual void OnServerPredictionQueryDone(bool success) = 0;

  // Shows a Datalist popup.
  virtual void ShowDatalistPopup(
      base::span<const autofill::SelectOption> options,
      bool is_rtl) = 0;

  // Hides the Datalist popup, if any is showing.
  virtual void HideDatalistPopup() = 0;

  // Informs the Java side that a focus change has happened to `field`.
  virtual void OnFocusChanged(const absl::optional<FieldInfo>& field) = 0;

  // Informs the Java side that the `field` has changed.
  virtual void OnFormFieldDidChange(const FieldInfo& field) = 0;

  // Informs the Java side that the visibility of the fields with `indices` has
  // changed.
  virtual void OnFormFieldVisibilitiesDidChange(
      base::span<const int> indices) = 0;

  // Informs the Java side that `field` has new `bounds`.
  // TODO(crbug.com/1478934): Make naming consistent across events, e.g.,
  // `OnFormFieldDidScroll`.
  // TODO(crbug.com/1478934): Combine with `OnFormFieldDidChange`?
  virtual void OnTextFieldDidScroll(const FieldInfo& field) = 0;

  // Informs the Java side that the form was submitted.
  virtual void OnFormSubmitted(mojom::SubmissionSource submission_source) = 0;

  // Informs the Java side that the form was autofilled.
  virtual void OnDidFillAutofillFormData() = 0;

  // Resets the Java instance.
  virtual void Reset() = 0;
};

}  // namespace autofill

#endif  // COMPONENTS_ANDROID_AUTOFILL_BROWSER_AUTOFILL_PROVIDER_ANDROID_BRIDGE_H_
