// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DESKS_STORAGE_CORE_DESK_TEMPLATE_CONVERSION_H_
#define COMPONENTS_DESKS_STORAGE_CORE_DESK_TEMPLATE_CONVERSION_H_

#include "base/time/time.h"
#include "base/values.h"

namespace ash {
class DeskTemplate;
}

namespace apps {
class AppRegistryCache;
}

namespace desks_storage {

// DeskTemplateConversion contains helper functions for converting between
// the various representations of desk template storage. These include
// ChromeSync format, DeskTemplate objects, and JSON representation.

namespace desk_template_conversion {

// Converts a time field from sync protobufs to a time object.
base::Time ProtoTimeToTime(int64_t proto_time);

// Converts a time object to the format used in sync protobufs
// (Microseconds since the Windows epoch).
int64_t TimeToProtoTime(const base::Time& t);

// Converts a JSON desk template to an ash desk template.
// The returned desk template will have source set to
// |ash::DeskTemplateSource::kPolicy|.
std::unique_ptr<ash::DeskTemplate> ParseDeskTemplateFromPolicy(
    const base::Value& policyJson);

base::Value SerializeDeskTemplateAsPolicy(
    const ash::DeskTemplate* desk_template,
    apps::AppRegistryCache* app_cache);

}  // namespace desk_template_conversion

}  // namespace desks_storage

#endif  // COMPONENTS_DESKS_STORAGE_CORE_DESK_TEMPLATE_CONVERSION_H_