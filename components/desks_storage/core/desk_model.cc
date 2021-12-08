// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/desks_storage/core/desk_model.h"

#include "ash/public/cpp/desk_template.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "components/desks_storage/core/desk_model_observer.h"
#include "components/desks_storage/core/desk_template_conversion.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace desks_storage {

namespace {
DeskModel::GetTemplateJsonStatus ConvertGetEntryStatusToTemplateJsonStatus(
    const DeskModel::GetEntryByUuidStatus status) {
  switch (status) {
    case DeskModel::GetEntryByUuidStatus::kOk:
      return DeskModel::GetTemplateJsonStatus::kOk;
    case DeskModel::GetEntryByUuidStatus::kFailure:
      return DeskModel::GetTemplateJsonStatus::kFailure;
    case DeskModel::GetEntryByUuidStatus::kNotFound:
      return DeskModel::GetTemplateJsonStatus::kNotFound;
    case DeskModel::GetEntryByUuidStatus::kInvalidUuid:
      return DeskModel::GetTemplateJsonStatus::kInvalidUuid;
  }
}

}  // namespace

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

void DeskModel::GetTemplateJson(const std::string& uuid,
                                apps::AppRegistryCache* app_cache,
                                GetTemplateJsonCallback callback) {
  GetEntryByUUID(
      uuid,
      base::BindOnce(&DeskModel::HandleTemplateConversionToPolicyJson,
                     base::Unretained(this), std::move(callback), app_cache));
}

void DeskModel::HandleTemplateConversionToPolicyJson(
    GetTemplateJsonCallback callback,
    apps::AppRegistryCache* app_cache,
    GetEntryByUuidStatus status,
    std::unique_ptr<ash::DeskTemplate> entry) {
  if (status != GetEntryByUuidStatus::kOk) {
    std::move(callback).Run(ConvertGetEntryStatusToTemplateJsonStatus(status),
                            "");
    return;
  }

  std::string raw_json;
  bool conversion_success = base::JSONWriter::Write(
      desk_template_conversion::SerializeDeskTemplateAsPolicy(entry.get(),
                                                              app_cache),
      &raw_json);

  if (conversion_success)
    std::move(callback).Run(GetTemplateJsonStatus::kOk, raw_json);
  else
    std::move(callback).Run(GetTemplateJsonStatus::kFailure, "");
}

}  // namespace desks_storage
