// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ANDROID_AUTOFILL_BROWSER_FORM_FIELD_DATA_ANDROID_H_
#define COMPONENTS_ANDROID_AUTOFILL_BROWSER_FORM_FIELD_DATA_ANDROID_H_

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ref.h"
#include "components/autofill/core/browser/autofill_type.h"

namespace autofill {

struct FormFieldData;
class FormFieldDataAndroidBridge;

// This class is the native peer of FormFieldData.java. Its intention is
// making relevant aspects of `FormFieldData` accessible to Java.
class FormFieldDataAndroid {
 public:
  // A helper struct that bundles are type predictions available to
  // `FormFieldDataAndroid`.
  struct FieldTypes {
    FieldTypes();
    FieldTypes(AutofillType heuristic_type,
               AutofillType server_type,
               AutofillType computed_type,
               std::vector<AutofillType> server_predictions);
    FieldTypes(FieldTypes&&);
    FieldTypes& operator=(FieldTypes&&);
    ~FieldTypes();

    AutofillType heuristic_type;
    AutofillType server_type;
    AutofillType computed_type;
    std::vector<AutofillType> server_predictions;
  };

  explicit FormFieldDataAndroid(FormFieldData* field);
  FormFieldDataAndroid(const FormFieldDataAndroid&) = delete;
  FormFieldDataAndroid& operator=(const FormFieldDataAndroid&) = delete;

  virtual ~FormFieldDataAndroid();

  base::android::ScopedJavaLocalRef<jobject> GetJavaPeer();
  void UpdateFromJava();
  void OnFormFieldDidChange(std::u16string_view value);

  // Updates the visibility and propagates the changes to Java. Note that
  // inside the `android_autofill` component visibility means focusability of
  // an `autofill::FormFieldData`. On the native side, this method therefore
  // updates the two fields that determine focusability, namely `role` and
  // `is_focusable`.
  void OnFormFieldVisibilityDidChange(const FormFieldData& field);

  bool SimilarFieldAs(const FormFieldData& field) const;
  void UpdateAutofillTypes(FieldTypes field_types);

 private:
  // The C++ <-> Java bridge.
  std::unique_ptr<FormFieldDataAndroidBridge> bridge_;

  // The field type predictions for this field.
  FieldTypes field_types_;

  // A raw reference to the underlying `FormFieldData` object. It is owned by
  // `this`' parent.
  const raw_ref<FormFieldData> field_;
};

}  // namespace autofill

#endif  // COMPONENTS_ANDROID_AUTOFILL_BROWSER_FORM_FIELD_DATA_ANDROID_H_
