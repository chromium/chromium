// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/android_autofill/browser/form_field_data_android.h"

#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include "components/android_autofill/browser/android_autofill_bridge_factory.h"
#include "components/android_autofill/browser/form_field_data_android_bridge.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/autofill/core/common/form_field_data.h"

namespace autofill {

using base::android::ScopedJavaLocalRef;

FormFieldDataAndroid::FieldTypes::FieldTypes() = default;

FormFieldDataAndroid::FieldTypes::FieldTypes(AutofillType type)
    : heuristic_type(type),
      server_type(type),
      computed_type(type),
      server_predictions({std::move(type)}) {}

FormFieldDataAndroid::FieldTypes::FieldTypes(
    AutofillType heuristic_type,
    AutofillType server_type,
    AutofillType computed_type,
    std::vector<AutofillType> server_predictions)
    : heuristic_type(std::move(heuristic_type)),
      server_type(std::move(server_type)),
      computed_type(std::move(computed_type)),
      server_predictions(std::move(server_predictions)) {}

FormFieldDataAndroid::FieldTypes::FieldTypes(FieldTypes&&) = default;

FormFieldDataAndroid::FieldTypes& FormFieldDataAndroid::FieldTypes::operator=(
    FieldTypes&&) = default;

FormFieldDataAndroid::FieldTypes::~FieldTypes() = default;

bool FormFieldDataAndroid::FieldTypes::operator==(
    const AutofillType& type) const {
  std::string_view target = type.ToStringView();
  return heuristic_type.ToStringView() == target &&
         server_type.ToStringView() == target &&
         computed_type.ToStringView() == target &&
         server_predictions.size() == 1 &&
         server_predictions[0].ToStringView() == target;
}

FormFieldDataAndroid::FormFieldDataAndroid(FormFieldData* field)
    : bridge_(AndroidAutofillBridgeFactory::GetInstance()
                  .CreateFormFieldDataAndroidBridge()),
      field_(*field) {
  field_types_.heuristic_type = AutofillType(UNKNOWN_TYPE);
}

FormFieldDataAndroid::~FormFieldDataAndroid() = default;

ScopedJavaLocalRef<jobject> FormFieldDataAndroid::GetJavaPeer() {
  return bridge_->GetOrCreateJavaPeer(*field_, field_types_);
}

void FormFieldDataAndroid::UpdateFromJava() {
  bridge_->UpdateFieldFromJava(*field_);
}

void FormFieldDataAndroid::OnFormFieldDidChange(std::u16string_view value) {
  field_->set_value(std::u16string(value));
  field_->set_is_autofilled(false);
  bridge_->UpdateValue(value);
}

void FormFieldDataAndroid::OnFormFieldVisibilityDidChange(
    const FormFieldData& field) {
  field_->set_is_focusable(field.is_focusable());
  field_->set_role(field.role());
  CHECK_EQ(field_->IsFocusable(), field.IsFocusable());
  bridge_->UpdateVisible(field_->IsFocusable());
}

bool FormFieldDataAndroid::SimilarFieldAs(const FormFieldData& field) const {
  auto SimilarityTuple = [](const FormFieldData& f) {
    return std::tuple_cat(
        std::tie(f.host_frame(), f.name(), f.name_attribute(),
                 f.id_attribute()),
        std::make_tuple(f.renderer_id(), f.form_control_type(),
                        IsCheckable(f.check_status())));
  };

  // For Android Autofill, labels are considered similar if they meet one of the
  // following two conditions:
  // 1. The labels have the same value.
  // 2. The labels were inferred from the same type of source and that source
  //    was not `LabelSource::kLabelTag`.
  auto LabelsAreSimilar = [](const FormFieldData& f1, const FormFieldData& f2) {
    return f1.label() == f2.label() ||
           (f1.label_source() != FormFieldData::LabelSource::kLabelTag &&
            f1.label_source() == f2.label_source());
  };

  return SimilarityTuple(*field_) == SimilarityTuple(field) &&
         LabelsAreSimilar(*field_, field);
}

void FormFieldDataAndroid::UpdateAutofillTypes(FieldTypes field_types) {
  field_types_ = std::move(field_types);
  bridge_->UpdateFieldTypes(field_types_);
}

}  // namespace autofill
