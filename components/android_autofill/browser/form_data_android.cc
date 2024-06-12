// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/android_autofill/browser/form_data_android.h"

#include <memory>
#include <string_view>
#include <tuple>

#include "base/containers/flat_map.h"
#include "components/android_autofill/browser/android_autofill_bridge_factory.h"
#include "components/android_autofill/browser/form_data_android_bridge.h"
#include "components/android_autofill/browser/form_field_data_android.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/unique_ids.h"

namespace autofill {

FormDataAndroid::FormDataAndroid(const FormData& form, SessionId session_id)
    : session_id_(session_id),
      form_(form),
      bridge_(AndroidAutofillBridgeFactory::GetInstance()
                  .CreateFormDataAndroidBridge()) {
  fields_.reserve(form_.fields().size());
  for (FormFieldData& field : form_.mutable_fields(/*pass_key=*/{})) {
    fields_.push_back(std::make_unique<FormFieldDataAndroid>(&field));
  }
}

FormDataAndroid::~FormDataAndroid() = default;

base::android::ScopedJavaLocalRef<jobject> FormDataAndroid::GetJavaPeer() {
  return bridge_->GetOrCreateJavaPeer(form_, session_id_, fields_);
}

void FormDataAndroid::UpdateFromJava() {
  for (std::unique_ptr<FormFieldDataAndroid>& field : fields_)
    field->UpdateFromJava();
}

void FormDataAndroid::OnFormFieldDidChange(size_t index,
                                           std::u16string_view value) {
  fields_[index]->OnFormFieldDidChange(value);
}

bool FormDataAndroid::GetFieldIndex(const FormFieldData& field, size_t* index) {
  for (size_t i = 0; i < form_.fields().size(); ++i) {
    if (form_.fields()[i].SameFieldAs(field)) {
      *index = i;
      return true;
    }
  }
  return false;
}

bool FormDataAndroid::GetSimilarFieldIndex(const FormFieldData& field,
                                           size_t* index) {
  for (size_t i = 0; i < form_.fields().size(); ++i) {
    if (fields_[i]->SimilarFieldAs(field)) {
      *index = i;
      return true;
    }
  }
  return false;
}

bool FormDataAndroid::SimilarFieldsAs(const FormData& form) const {
  if (fields_.size() != form.fields().size()) {
    return false;
  }
  for (size_t i = 0; i < fields_.size(); ++i) {
    if (!fields_[i]->SimilarFieldAs(form.fields()[i])) {
      return false;
    }
  }
  return true;
}

bool FormDataAndroid::SimilarFormAs(const FormData& form) const {
  // Note that comparing unique renderer ids alone is not a strict enough check,
  // since these remain constant even if the page has dynamically modified its
  // fields to have different labels, form control types, etc.
  auto SimilarityTuple = [](const FormData& f) {
    return std::tie(f.host_frame(), f.renderer_id(), f.name(), f.id_attribute(),
                    f.name_attribute(), f.url(), f.action());
  };
  return SimilarityTuple(form_) == SimilarityTuple(form) &&
         SimilarFieldsAs(form);
}

void FormDataAndroid::UpdateFieldTypes(const FormStructure& form_structure) {
  // Map FieldGlobalId's to their respective AutofillField, this way we can
  // quickly ignore below FormFieldDataAndroid's with no matching AutofillField.
  auto autofill_fields = base::MakeFlatMap<FieldGlobalId, const AutofillField*>(
      form_structure, {}, [](const auto& field) {
        return std::make_pair(field->global_id(), field.get());
      });
  for (auto& form_field_data_android : fields_) {
    if (auto it = autofill_fields.find(form_field_data_android->global_id());
        it != autofill_fields.end()) {
      const AutofillField* autofill_field = it->second;
      std::vector<AutofillType> server_predictions;
      for (const auto& prediction : autofill_field->server_predictions()) {
        server_predictions.emplace_back(
            ToSafeFieldType(prediction.type(), NO_SERVER_DATA));
      }
      form_field_data_android->UpdateAutofillTypes(
          FormFieldDataAndroid::FieldTypes(
              AutofillType(autofill_field->heuristic_type()),
              AutofillType(autofill_field->server_type()),
              autofill_field->ComputedType(), std::move(server_predictions)));
    }
  }
}

void FormDataAndroid::UpdateFieldTypes(
    const base::flat_map<FieldGlobalId, AutofillType>& types) {
  for (const std::unique_ptr<FormFieldDataAndroid>& field : fields_) {
    auto it = types.find(field->global_id());
    if (it == types.end()) {
      continue;
    }

    const AutofillType& new_type = it->second;
    if (field->field_types() != new_type) {
      field->UpdateAutofillTypes(FormFieldDataAndroid::FieldTypes(new_type));
    }
  }
}

std::vector<int> FormDataAndroid::UpdateFieldVisibilities(
    const FormData& form) {
  CHECK_EQ(form_.fields().size(), form.fields().size());
  CHECK_EQ(form_.fields().size(), fields_.size());

  // We rarely expect to find any difference in visibility - therefore do not
  // reserve space in the vector.
  std::vector<int> indices;
  for (size_t i = 0; i < form_.fields().size(); ++i) {
    if (form_.fields()[i].IsFocusable() != form.fields()[i].IsFocusable()) {
      fields_[i]->OnFormFieldVisibilityDidChange(form.fields()[i]);
      indices.push_back(i);
    }
  }
  return indices;
}

}  // namespace autofill
