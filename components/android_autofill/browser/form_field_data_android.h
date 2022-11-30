// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ANDROID_AUTOFILL_BROWSER_FORM_FIELD_DATA_ANDROID_H_
#define COMPONENTS_ANDROID_AUTOFILL_BROWSER_FORM_FIELD_DATA_ANDROID_H_

#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/common/form_field_data.h"

namespace autofill {

// This class is native peer of FormFieldData.java, makes
// autofill::FormFieldData available in Java.
class FormFieldDataAndroid {
 public:
  explicit FormFieldDataAndroid(FormFieldData* field);
  FormFieldDataAndroid(const FormFieldDataAndroid&) = delete;
  FormFieldDataAndroid& operator=(const FormFieldDataAndroid&) = delete;

  virtual ~FormFieldDataAndroid();

  base::android::ScopedJavaLocalRef<jobject> GetJavaPeer();
  void GetValue();
  void OnFormFieldDidChange(const std::u16string& value);
  bool SimilarFieldAs(const FormFieldData& field) const;
  void UpdateAutofillTypes(const AutofillType& heuristic_type,
                           const AutofillType& server_type,
                           const AutofillType& computed_type,
                           const std::vector<AutofillType>& server_predictions);

 private:
  AutofillType heuristic_type_;
  AutofillType server_type_;
  AutofillType computed_type_;
  std::vector<AutofillType> server_predictions_;

  // Not owned.
  raw_ptr<FormFieldData> field_ptr_;
  JavaObjectWeakGlobalRef java_ref_;
};

}  // namespace autofill

#endif  // COMPONENTS_ANDROID_AUTOFILL_BROWSER_FORM_FIELD_DATA_ANDROID_H_
