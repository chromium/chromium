// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/json/json_string_value_serializer.h"
#include "components/account_id/account_id.h"
#include "components/desks_storage/core/desk_template_conversion.h"
#include "components/desks_storage/core/desk_template_util.h"
#include "components/desks_storage/core/desk_test_util.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/sync/protocol/workspace_desk_specifics.pb.h"
#include "testing/libfuzzer/proto/lpm_interface.h"

namespace desks_storage {

// Attempts to parse proto from fuzz data, if succeeds verifies semantic
// equivalency between the various formats.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  // parse string into value.
  std::string str = std::string(reinterpret_cast<const char*>(data), size);
  std::string error_message;
  int error_code;
  std::unique_ptr<base::Value> desk_template_value =
      JSONStringValueDeserializer(str).Deserialize(&error_code, &error_message);

  if (desk_template_value == nullptr) {
    return 0;
  }

  // Init dependencies for conversion.
  AccountId account_id;
  std::unique_ptr<apps::AppRegistryCache> apps_cache =
      std::make_unique<apps::AppRegistryCache>();
  desk_test_util::PopulateAppRegistryCache(account_id, apps_cache.get());

  desk_template_conversion::ParseSavedDeskResult desk_template_result =
      desk_template_conversion::ParseDeskTemplateFromBaseValue(
          *desk_template_value, ash::DeskTemplateSource::kUser);

  if (!desk_template_result.has_value()) {
    return 0;
  }

  std::unique_ptr<ash::DeskTemplate> desk_template =
      std::move(desk_template_result.value());

  // Round trip for sync protos.
  sync_pb::WorkspaceDeskSpecifics template_to_proto =
      desk_template_conversion::ToSyncProto(desk_template.get(),
                                            apps_cache.get());
  std::unique_ptr<ash::DeskTemplate> proto_to_template =
      desk_template_conversion::FromSyncProto(template_to_proto);

  CHECK(desk_template_util::AreDeskTemplatesEqual(desk_template.get(),
                                                  proto_to_template.get()));

  // Round trip for JSON format.
  base::Value template_to_value =
      desk_template_conversion::SerializeDeskTemplateAsBaseValue(
          desk_template.get(), apps_cache.get());
  desk_template_conversion::ParseSavedDeskResult parse_result =
      desk_template_conversion::ParseDeskTemplateFromBaseValue(
          template_to_value, desk_template->source());

  CHECK(parse_result.has_value());
  CHECK(desk_template_util::AreDeskTemplatesEqual(desk_template.get(),
                                                  parse_result.value().get()));

  return 0;
}

}  // namespace desks_storage
