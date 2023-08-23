// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DESKS_STORAGE_CORE_DESK_TEMPLATE_UTIL_H_
#define COMPONENTS_DESKS_STORAGE_CORE_DESK_TEMPLATE_UTIL_H_

#include <memory>
#include <string>

#include "ash/public/cpp/desk_template.h"
#include "base/containers/flat_map.h"
#include "base/uuid.h"

namespace desks_storage::desk_template_util {

ash::DeskTemplate* FindOtherEntryWithName(
    const std::u16string& name,
    const base::Uuid& uuid,
    const base::flat_map<base::Uuid, std::unique_ptr<ash::DeskTemplate>>&
        entries);

bool AreDeskTemplatesEqual(const ash::DeskTemplate* template_one,
                           const ash::DeskTemplate* template_two);

}  // namespace desks_storage::desk_template_util

#endif  // COMPONENTS_DESKS_STORAGE_CORE_DESK_TEMPLATE_UTIL_H_
