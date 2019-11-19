// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/system/kiosk_oem_manifest_parser.h"

#include <memory>

#include "base/json/json_file_value_serializer.h"
#include "base/strings/stringprintf.h"
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
  base::DictionaryValue* dict = NULL;
  if (error_code != JSONFileValueDeserializer::JSON_NO_ERROR || !value.get() ||
      !value->GetAsDictionary(&dict)) {
    return false;
  }

  dict->GetString(kDeviceRequisition, &manifest->device_requisition);
  dict->GetBoolean(kKeyboardDrivenOobe, &manifest->keyboard_driven_oobe);
  if (!dict->GetBoolean(kEnterpriseManaged, &manifest->enterprise_managed) ||
      !dict->GetBoolean(kAllowReset, &manifest->can_exit_enrollment)) {
    return false;
  }

  return true;
}

}  // namespace chromeos
