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

// This class is native peer of FormData.java, to make autofill::FormData
// available in Java.
class FormDataAndroid {
 public:
  explicit FormDataAndroid(const FormData& form);
  FormDataAndroid(const FormDataAndroid&) = delete;
  FormDataAndroid& operator=(const FormDataAndroid&) = delete;

  virtual ~FormDataAndroid();

  base::android::ScopedJavaLocalRef<jobject> GetJavaPeer(
      const FormStructure* form_structure);

  // Get autofill values from Java side and return FormData.
  const FormData& GetAutofillValues();

  base::android::ScopedJavaLocalRef<jobject> GetNextFormFieldData(JNIEnv* env);

  // Get index of given field, return True and index of focus field if found.
  bool GetFieldIndex(const FormFieldData& field, size_t* index);

  // Get index of given field, return True and index of focus field if
  // similar field is found. This method compares less attributes than
  // GetFieldIndex() does, and should be used when field could be changed
  // dynamically, but the changed has no impact on autofill purpose, e.g. css
  // style change, see FormFieldData::SimilarFieldAs() for details.
  bool GetSimilarFieldIndex(const FormFieldData& field, size_t* index);

  // Return true if this form is similar to the given form.
  bool SimilarFormAs(const FormData& form);

  // Invoked when form field which specified by |index| is charged to new
  // |value|.
  void OnFormFieldDidChange(size_t index, const std::u16string& value);

  // Updates the field types from the |form|.
  void UpdateFieldTypes(const FormStructure& form);

  const FormData& form() { return form_; }

 private:
  // Same as the form passed in from constructor, but FormFieldData's bounds is
  // transformed to viewport coordinates.
  FormData form_;
  std::vector<std::unique_ptr<FormFieldDataAndroid>> fields_;
  JavaObjectWeakGlobalRef java_ref_;
  // keep track of index when popping up fields to Java.
  size_t index_ = 0;
};

}  // namespace autofill

#endif  // COMPONENTS_ANDROID_AUTOFILL_BROWSER_FORM_DATA_ANDROID_H_
