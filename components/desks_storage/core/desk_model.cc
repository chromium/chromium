// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/desks_storage/core/desk_model.h"

#include "ash/public/cpp/desk_template.h"
#include "components/desks_storage/core/desk_model_observer.h"

namespace desks_storage {

DeskModel::DeskModel() = default;

DeskModel::~DeskModel() = default;

GetAllEntriesResult::GetAllEntriesResult(
    GetAllEntriesStatus status,
    std::vector<ash::DeskTemplate*> entries)
    : status(status), entries(std::move(entries)) {}

GetAllEntriesResult::~GetAllEntriesResult() = default;

GetEntryByUuidResult::GetEntryByUuidResult(
    GetEntryByUuidStatus status,
    std::unique_ptr<ash::DeskTemplate> entry)
    : status(status), entry(std::move(entry)) {}

GetEntryByUuidResult::~GetEntryByUuidResult() = default;

void DeskModel::AddObserver(DeskModelObserver* observer) {
  DCHECK(observer);
  observers_.AddObserver(observer);
}

void DeskModel::RemoveObserver(DeskModelObserver* observer) {
  observers_.RemoveObserver(observer);
}

}  // namespace desks_storage