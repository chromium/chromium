// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/desks_storage/core/desk_model.h"

#include "ash/public/cpp/desk_template.h"
#include "base/guid.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
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

DeskModel::GetAllEntriesResult::GetAllEntriesResult(
    GetAllEntriesStatus status,
    std::vector<const ash::DeskTemplate*> entries)
    : status(status), entries(std::move(entries)) {}
DeskModel::GetAllEntriesResult::~GetAllEntriesResult() = default;

void DeskModel::AddObserver(DeskModelObserver* observer) {
  DCHECK(observer);
  observers_.AddObserver(observer);
}

void DeskModel::RemoveObserver(DeskModelObserver* observer) {
  observers_.RemoveObserver(observer);
}

void DeskModel::GetTemplateJson(const std::string& uuid,
                                apps::AppRegistryCache* app_cache,
                                GetTemplateJsonCallback callback) {
  GetEntryByUUID(
      uuid,
      base::BindOnce(&DeskModel::HandleTemplateConversionToPolicyJson,
                     base::Unretained(this), std::move(callback), app_cache));
}

void DeskModel::SetPolicyDeskTemplates(const std::string& policy_json) {
  policy_entries_.clear();

  base::StringPiece raw_json = base::StringPiece(policy_json);
  auto parsed_list = base::JSONReader::ReadAndReturnValueWithError(raw_json);
  if (!parsed_list.has_value())
    return;

  if (!parsed_list->is_list()) {
    LOG(WARNING) << "Expected JSON list in admin templates policy.";
    return;
  }

  for (auto& desk_template : parsed_list->GetListDeprecated()) {
    std::unique_ptr<ash::DeskTemplate> dt =
        desk_template_conversion::ParseDeskTemplateFromSource(
            desk_template, ash::DeskTemplateSource::kPolicy);
    if (dt) {
      policy_entries_.push_back(std::move(dt));
    } else {
      LOG(WARNING) << "Failed to parse admin template from JSON: "
                   << desk_template;
    }
  }
}

void DeskModel::RemovePolicyDeskTemplates() {
  policy_entries_.clear();
}

std::unique_ptr<ash::DeskTemplate> DeskModel::GetAdminDeskTemplateByUUID(
    const std::string& uuid_str) const {
  const base::GUID uuid = base::GUID::ParseCaseInsensitive(uuid_str);

  for (const std::unique_ptr<ash::DeskTemplate>& policy_entry :
       policy_entries_) {
    if (policy_entry->uuid() == uuid)
      return policy_entry->Clone();
  }

  return nullptr;
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
  base::Value template_list(base::Value::Type::LIST);
  template_list.Append(desk_template_conversion::SerializeDeskTemplateAsPolicy(
      entry.get(), app_cache));

  const bool conversion_success =
      base::JSONWriter::Write(template_list, &raw_json);

  if (conversion_success)
    std::move(callback).Run(GetTemplateJsonStatus::kOk, raw_json);
  else
    std::move(callback).Run(GetTemplateJsonStatus::kFailure, "");
}

}  // namespace desks_storage
