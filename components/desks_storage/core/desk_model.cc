// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/desks_storage/core/desk_model.h"

#include "ash/public/cpp/desk_template.h"
#include "base/json/json_reader.h"
#include "components/desks_storage/core/desk_model_observer.h"
#include "components/desks_storage/core/desk_template_conversion.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace desks_storage {

DeskModel::DeskModel() = default;

DeskModel::~DeskModel() {
  for (DeskModelObserver& observer : observers_)
    observer.OnDeskModelDestroying();
}

void DeskModel::AddObserver(DeskModelObserver* observer) {
  DCHECK(observer);
  observers_.AddObserver(observer);
}

void DeskModel::RemoveObserver(DeskModelObserver* observer) {
  observers_.RemoveObserver(observer);
}

void DeskModel::SetPolicyDeskTemplates(const std::string& policyJson) {
  policy_entries_.clear();

  base::StringPiece raw_json = base::StringPiece(policyJson);
  base::JSONReader::ValueWithError parsed_list =
      base::JSONReader::ReadAndReturnValueWithError(raw_json);
  if (!parsed_list.value || !parsed_list.value->is_list())
    return;

  for (auto& desk_template : parsed_list.value->GetList()) {
    std::unique_ptr<ash::DeskTemplate> dt =
        desk_template_conversion::ParseDeskTemplateFromPolicy(desk_template);
    if (dt)
      policy_entries_.push_back(std::move(dt));
  }
}

}  // namespace desks_storage
