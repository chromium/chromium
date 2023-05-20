// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DESKS_STORAGE_CORE_ADMIN_TEMPLATE_MODEL_H_
#define COMPONENTS_DESKS_STORAGE_CORE_ADMIN_TEMPLATE_MODEL_H_

#include "ash/public/cpp/desk_template.h"
#include "components/desks_storage/core/desk_model.h"

namespace desks_storage {

// This virtual class is a reduced interface for a DeskModel that is used in
// admin templates to provide a list of said templates as well as an ability to
// update them.
class AdminTemplateModel {
 public:
  virtual ~AdminTemplateModel() = default;

  // Attempts to update the entry based on `uuid`.  If `uuid` is not present
  // in the underlying model, the entry is discarded.
  virtual void UpdateEntry(std::unique_ptr<ash::DeskTemplate> entry) = 0;

  // Semantically equivalent to DeskModel::GetEntryByUUID()
  virtual DeskModel::GetEntryByUuidResult GetEntryByUUID(
      const base::Uuid& uuid) = 0;

  // Semantically equivalent to DeskModel::GetAllEntries()
  virtual DeskModel::GetAllEntriesResult GetAllEntries() = 0;
};

}  // namespace desks_storage

#endif  // COMPONENTS_DESKS_STORAGE_CORE_ADMIN_TEMPLATE_MODEL_H_
