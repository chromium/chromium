// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ANDROID_AUTOFILL_BROWSER_FORM_DATA_ANDROID_H_
#define COMPONENTS_ANDROID_AUTOFILL_BROWSER_FORM_DATA_ANDROID_H_

#include <memory>
#include <string_view>
#include <vector>

#include "base/android/scoped_java_ref.h"
#include "base/types/strong_alias.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/unique_ids.h"

namespace autofill {

class FormDataAndroidBridge;
class FormFieldDataAndroid;
class FormStructure;

using SessionId = base::StrongAlias<class SessionIdTag, int>;

// This class is the native peer of `FormData.java` to make
// `autofill::FormData` available in Java.
class FormDataAndroid {
 public:
  FormDataAndroid(const FormData& form, SessionId session_id);
  FormDataAndroid(const FormDataAndroid&) = delete;
  FormDataAndroid& operator=(const FormDataAndroid&) = delete;

  virtual ~FormDataAndroid();

  base::android::ScopedJavaLocalRef<jobject> GetJavaPeer();

  // Updates `form_` with state from Java side.
  void UpdateFromJava();

  // Gets the index of a given field. It returns `true` and sets the `index` if
  // `field` is found.
  bool GetFieldIndex(const FormFieldData& field, size_t* index);

  // Gets the index of a given field. It returns `true` and sets the `index` if
  // a similar field is found. This method compares fewer attributes than
  // `GetFieldIndex()` does, and should be used when the field could be changed
  // dynamically, but the change has no impact on autofill purpose. Examples are
  // CSS style changes - see `FormFieldDataAndroid::SimilarFieldAs()` for
  // details.
  bool GetSimilarFieldIndex(const FormFieldData& field, size_t* index);

  // Returns true if this form is similar to the given form.
  // `SimilarFormAs` checks `FormData` members that are unlikely to have been
  // changed by direct user input. If they differ, the form has changed enough
  // (e.g. by adding or removing fields) to warrant starting a new Autofill
  // session.
  bool SimilarFormAs(const FormData& form) const;

  // Is invoked when the form field specified by `index` is changed to a new
  // `value`.
  void OnFormFieldDidChange(size_t index, std::u16string_view value);

  // Updates the field types from the `form`.
  void UpdateFieldTypes(const FormStructure& form);

  // Updates the field types. Note that this method sets *all* types (computed
  // type, server type, heuristic type) to a single type. This is intended to be
  // used for password forms in which the `password_manager::FormDataParser`
  // predictions overrule Autofill's predictions.
  void UpdateFieldTypes(
      const base::flat_map<FieldGlobalId, AutofillType>& types);

  // Updates the visibility (focusability in Autofill terms) of the fields and
  // returns the indices of the fields that were changed. Assumes that the forms
  // are similar.
  std::vector<int> UpdateFieldVisibilities(const FormData& form);

  const FormData& form() const { return form_; }

  SessionId session_id() const { return session_id_; }

 private:
  friend class FormDataAndroidTestApi;

  // Returns whether the fields of `this` are similar to the fields of `form`.
  // Returns `false` if the number of fields differs.
  bool SimilarFieldsAs(const FormData& form) const;

  // The session id of this form. It is used to generate virtual view ids for
  // the `ViewStructure` shared with the Android AutofillManager framework.
  const SessionId session_id_;

  // A copy of the form passed in through the constructor.
  FormData form_;
  std::vector<std::unique_ptr<FormFieldDataAndroid>> fields_;

  // The bridge for C++ <-> Java communication.
  std::unique_ptr<FormDataAndroidBridge> bridge_;
};

}  // namespace autofill

#endif  // COMPONENTS_ANDROID_AUTOFILL_BROWSER_FORM_DATA_ANDROID_H_
