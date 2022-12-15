// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/system/kiosk_oem_manifest_parser.h"

#include <memory>

#include "base/json/json_file_value_serializer.h"
#include "base/values.h"

namespace chromeos {

namespace {

const char kEnterpriseManaged[] = "enterprise_managed";
const char kAllowReset[] = "can_exit_enrollment";
const char kDeviceRequisition[] = "device_requisition";
const char kKeyboardDrivenOobe[] = "keyboard_driven_oobe";

}  // namespace

KioskOemManifestParser::Manifest::Manifest()
    : enterprise_managed(false),
      can_exit_enrollment(true),
      keyboard_driven_oobe(false) {}

bool KioskOemManifestParser::Load(const base::FilePath& kiosk_oem_file,
                                  KioskOemManifestParser::Manifest* manifest) {
  int error_code = JSONFileValueDeserializer::JSON_NO_ERROR;
  std::string error_msg;
  std::unique_ptr<JSONFileValueDeserializer> deserializer(
      new JSONFileValueDeserializer(kiosk_oem_file));
  std::unique_ptr<base::Value> value =
      deserializer->Deserialize(&error_code, &error_msg);
  if (error_code != JSONFileValueDeserializer::JSON_NO_ERROR || !value ||
      !value->is_dict())
    return false;

  if (auto* v = value->GetDict().FindString(kDeviceRequisition))
    manifest->device_requisition = *v;

  if (absl::optional<bool> v = value->GetDict().FindBool(kKeyboardDrivenOobe)) {
    manifest->keyboard_driven_oobe = *v;
  }

  if (absl::optional<bool> v = value->GetDict().FindBool(kEnterpriseManaged)) {
    manifest->enterprise_managed = *v;
  } else {
    return false;
  }

  if (absl::optional<bool> v = value->GetDict().FindBool(kAllowReset)) {
    manifest->can_exit_enrollment = *v;
  } else {
    return false;
  }

  return true;
}

}  // namespace chromeos
