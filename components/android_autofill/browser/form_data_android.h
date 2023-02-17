// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ANDROID_AUTOFILL_BROWSER_FORM_DATA_ANDROID_H_
#define COMPONENTS_ANDROID_AUTOFILL_BROWSER_FORM_DATA_ANDROID_H_

#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "components/autofill/core/common/form_data.h"

namespace autofill {

class FormFieldDataAndroid;
class FormStructure;

// This class is the native peer of `FormData.java` to make
// `autofill::FormData` available in Java.
class FormDataAndroid {
 public:
  explicit FormDataAndroid(const FormData& form);
  FormDataAndroid(const FormDataAndroid&) = delete;
  FormDataAndroid& operator=(const FormDataAndroid&) = delete;

  virtual ~FormDataAndroid();

  base::android::ScopedJavaLocalRef<jobject> GetJavaPeer(
      const FormStructure* form_structure);

  // Updates `form_` with state from Java side.
  void UpdateFromJava();

  base::android::ScopedJavaLocalRef<jobject> GetNextFormFieldData(JNIEnv* env);

  // Gets index of given field. It returns `true` and sets the `index` if
  // `field` is found.
  bool GetFieldIndex(const FormFieldData& field, size_t* index);

  // Gets index of given field. It returns `true` and sets the `index` if a
  // similar field is found. This method compares fewer attributes than
  // `GetFieldIndex()` does, and should be used when the field could be changed
  // dynamically, but the change has no impact on autofill purpose. Examples are
  // CSS style changes - see `FormFieldData::SimilarFieldAs()` for details.
  bool GetSimilarFieldIndex(const FormFieldData& field, size_t* index);

  // Returns true if this form is similar to the given form.
  bool SimilarFormAs(const FormData& form) const;

  // Is invoked when the form field specified by `index` is changed to a new
  // `value`.
  void OnFormFieldDidChange(size_t index, const std::u16string& value);

  // Updates the field types from the `form`.
  void UpdateFieldTypes(const FormStructure& form);

  const FormData& form() const { return form_; }

 private:
  // Same as the form passed in from constructor, but FormFieldData's bounds are
  // transformed to viewport coordinates.
  FormData form_;
  std::vector<std::unique_ptr<FormFieldDataAndroid>> fields_;
  JavaObjectWeakGlobalRef java_ref_;
};

}  // namespace autofill

#endif  // COMPONENTS_ANDROID_AUTOFILL_BROWSER_FORM_DATA_ANDROID_H_
