// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/desks_storage/core/desk_model.h"

#include <string_view>

#include "ash/public/cpp/desk_template.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/uuid.h"
#include "components/desks_storage/core/desk_model_observer.h"
#include "components/desks_storage/core/desk_template_conversion.h"

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

DeskModel::GetAllEntriesResult::GetAllEntriesResult(
    GetAllEntriesStatus status,
    std::vector<raw_ptr<const ash::DeskTemplate, VectorExperimental>> entries)
    : status(status), entries(std::move(entries)) {}
DeskModel::GetAllEntriesResult::GetAllEntriesResult(
    GetAllEntriesResult& other) = default;
DeskModel::GetAllEntriesResult::~GetAllEntriesResult() = default;

DeskModel::GetEntryByUuidResult::GetEntryByUuidResult(
    GetEntryByUuidStatus status,
    std::unique_ptr<ash::DeskTemplate> entry)
    : status(status), entry(std::move(entry)) {}
DeskModel::GetEntryByUuidResult::~GetEntryByUuidResult() = default;

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

void DeskModel::GetTemplateJson(const base::Uuid& uuid,
                                apps::AppRegistryCache* app_cache,
                                GetTemplateJsonCallback callback) {
  auto result = GetEntryByUUID(uuid);
  HandleTemplateConversionToPolicyJson(std::move(callback), app_cache,
                                       result.status, std::move(result.entry));
}

void DeskModel::SetPolicyDeskTemplates(const std::string& policy_json) {
  policy_entries_.clear();

  std::string_view raw_json = std::string_view(policy_json);
  auto parsed_list = base::JSONReader::ReadAndReturnValueWithError(raw_json);
  if (!parsed_list.has_value())
    return;

  if (!parsed_list->is_list()) {
    LOG(WARNING) << "Expected JSON list in admin templates policy.";
    return;
  }

  for (auto& desk_template : parsed_list->GetList()) {
    auto dt = desk_template_conversion::ParseDeskTemplateFromBaseValue(
        desk_template, ash::DeskTemplateSource::kPolicy);
    if (dt.has_value()) {
      policy_entries_.push_back(std::move(dt.value()));
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
    const base::Uuid& uuid) const {
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
                            {});
    return;
  }

  base::Value::List template_list;
  template_list.Append(
      desk_template_conversion::SerializeDeskTemplateAsBaseValue(entry.get(),
                                                                 app_cache));

  std::move(callback).Run(GetTemplateJsonStatus::kOk,
                          base::Value(std::move(template_list)));
}

}  // namespace desks_storage
