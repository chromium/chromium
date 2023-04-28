// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/json/json_file_value_serializer.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/notreached.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/external_constants.h"
#include "chrome/updater/external_constants_default.h"
#include "chrome/updater/external_constants_override.h"
#include "chrome/updater/updater_branding.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/updater_version.h"
#include "chrome/updater/util/util.h"
#include "components/crx_file/crx_verifier.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_MAC)
#include "base/mac/foundation_util.h"
#elif BUILDFLAG(IS_WIN)
#include "base/path_service.h"
#endif

namespace {

// Developer override file name, relative to app data directory.
const char kDevOverrideFileName[] = "overrides.json";

std::vector<GURL> GURLVectorFromStringList(
    const base::Value::List& update_url_list) {
  std::vector<GURL> ret;
  ret.reserve(update_url_list.size());
  for (const base::Value& url : update_url_list) {
    CHECK(url.is_string()) << "Non-string Value in update URL list";
    ret.push_back(GURL(url.GetString()));
  }
  return ret;
}

}  // anonymous namespace

namespace updater {

absl::optional<base::FilePath> GetOverrideFilePath(UpdaterScope scope) {
  absl::optional<base::FilePath> base = GetInstallDirectory(scope);
  if (!base) {
    return absl::nullopt;
  }
  return base->DirName().AppendASCII(kDevOverrideFileName);
}

ExternalConstantsOverrider::ExternalConstantsOverrider(
    base::Value::Dict override_values,
    scoped_refptr<ExternalConstants> next_provider)
    : ExternalConstants(std::move(next_provider)),
      override_values_(std::move(override_values)) {}

ExternalConstantsOverrider::~ExternalConstantsOverrider() = default;

std::vector<GURL> ExternalConstantsOverrider::UpdateURL() const {
  if (!override_values_.contains(kDevOverrideKeyUrl)) {
    return next_provider_->UpdateURL();
  }
  const base::Value* update_url_value =
      override_values_.Find(kDevOverrideKeyUrl);
  switch (update_url_value->type()) {
    case base::Value::Type::STRING:
      return {GURL(update_url_value->GetString())};
    case base::Value::Type::LIST:
      return GURLVectorFromStringList(update_url_value->GetList());
    default:
      LOG(FATAL) << "Unexpected type of override[" << kDevOverrideKeyUrl
                 << "]: " << base::Value::GetTypeName(update_url_value->type());
      NOTREACHED();
  }
  NOTREACHED();
  return {};
}

GURL ExternalConstantsOverrider::CrashUploadURL() const {
  if (!override_values_.contains(kDevOverrideKeyCrashUploadUrl)) {
    return next_provider_->CrashUploadURL();
  }
  const base::Value* crash_upload_url_value =
      override_values_.Find(kDevOverrideKeyCrashUploadUrl);
  CHECK(crash_upload_url_value->is_string())
      << "Unexpected type of override[" << kDevOverrideKeyCrashUploadUrl
      << "]: " << base::Value::GetTypeName(crash_upload_url_value->type());
  return {GURL(crash_upload_url_value->GetString())};
}

GURL ExternalConstantsOverrider::DeviceManagementURL() const {
  if (!override_values_.contains(kDevOverrideKeyDeviceManagementUrl)) {
    return next_provider_->DeviceManagementURL();
  }
  const base::Value* device_management_url_value =
      override_values_.Find(kDevOverrideKeyDeviceManagementUrl);
  CHECK(device_management_url_value->is_string())
      << "Unexpected type of override[" << kDevOverrideKeyDeviceManagementUrl
      << "]: " << base::Value::GetTypeName(device_management_url_value->type());
  return {GURL(device_management_url_value->GetString())};
}

bool ExternalConstantsOverrider::UseCUP() const {
  if (!override_values_.contains(kDevOverrideKeyUseCUP)) {
    return next_provider_->UseCUP();
  }
  const base::Value* use_cup_value =
      override_values_.Find(kDevOverrideKeyUseCUP);
  CHECK(use_cup_value->is_bool())
      << "Unexpected type of override[" << kDevOverrideKeyUseCUP
      << "]: " << base::Value::GetTypeName(use_cup_value->type());

  return use_cup_value->GetBool();
}

base::TimeDelta ExternalConstantsOverrider::InitialDelay() const {
  if (!override_values_.contains(kDevOverrideKeyInitialDelay)) {
    return next_provider_->InitialDelay();
  }

  const base::Value* initial_delay_value =
      override_values_.Find(kDevOverrideKeyInitialDelay);
  CHECK(initial_delay_value->is_double())
      << "Unexpected type of override[" << kDevOverrideKeyInitialDelay
      << "]: " << base::Value::GetTypeName(initial_delay_value->type());
  return base::Seconds(initial_delay_value->GetDouble());
}

base::TimeDelta ExternalConstantsOverrider::ServerKeepAliveTime() const {
  if (!override_values_.contains(kDevOverrideKeyServerKeepAliveSeconds)) {
    return next_provider_->ServerKeepAliveTime();
  }

  const base::Value* server_keep_alive_seconds_value =
      override_values_.Find(kDevOverrideKeyServerKeepAliveSeconds);
  CHECK(server_keep_alive_seconds_value->is_int())
      << "Unexpected type of override[" << kDevOverrideKeyServerKeepAliveSeconds
      << "]: "
      << base::Value::GetTypeName(server_keep_alive_seconds_value->type());
  return base::Seconds(server_keep_alive_seconds_value->GetInt());
}

crx_file::VerifierFormat ExternalConstantsOverrider::CrxVerifierFormat() const {
  if (!override_values_.contains(kDevOverrideKeyCrxVerifierFormat)) {
    return next_provider_->CrxVerifierFormat();
  }

  const base::Value* crx_format_verifier_value =
      override_values_.Find(kDevOverrideKeyCrxVerifierFormat);
  CHECK(crx_format_verifier_value->is_int())
      << "Unexpected type of override[" << kDevOverrideKeyCrxVerifierFormat
      << "]: " << base::Value::GetTypeName(crx_format_verifier_value->type());
  return static_cast<crx_file::VerifierFormat>(
      crx_format_verifier_value->GetInt());
}

base::Value::Dict ExternalConstantsOverrider::GroupPolicies() const {
  if (!override_values_.contains(kDevOverrideKeyGroupPolicies)) {
    return next_provider_->GroupPolicies();
  }

  const base::Value* group_policies_value =
      override_values_.Find(kDevOverrideKeyGroupPolicies);
  CHECK(group_policies_value->is_dict())
      << "Unexpected type of override[" << kDevOverrideKeyGroupPolicies
      << "]: " << base::Value::GetTypeName(group_policies_value->type());
  return group_policies_value->GetDict().Clone();
}

base::TimeDelta ExternalConstantsOverrider::OverinstallTimeout() const {
  if (!override_values_.contains(kDevOverrideKeyOverinstallTimeout)) {
    return next_provider_->OverinstallTimeout();
  }

  const base::Value* value =
      override_values_.Find(kDevOverrideKeyOverinstallTimeout);
  CHECK(value->is_int()) << "Unexpected type of override["
                         << kDevOverrideKeyOverinstallTimeout
                         << "]: " << base::Value::GetTypeName(value->type());
  return base::Seconds(value->GetInt());
}

// static
scoped_refptr<ExternalConstantsOverrider>
ExternalConstantsOverrider::FromDefaultJSONFile(
    scoped_refptr<ExternalConstants> next_provider) {
  const absl::optional<base::FilePath> override_file_path =
      GetOverrideFilePath(GetUpdaterScope());
  if (!override_file_path) {
    LOG(ERROR) << "Cannot find override file path.";
    return nullptr;
  }

  JSONFileValueDeserializer parser(*override_file_path,
                                   base::JSON_ALLOW_TRAILING_COMMAS);
  int error_code = 0;
  std::string error_message;
  std::unique_ptr<base::Value> parsed_value(
      parser.Deserialize(&error_code, &error_message));
  if (error_code || !parsed_value) {
    VLOG(2) << "Could not parse " << override_file_path << ": error "
            << error_code << ": " << error_message;
    return nullptr;
  }

  if (!parsed_value->is_dict()) {
    LOG(ERROR) << "Invalid data in " << override_file_path << ": not a dict";
    return nullptr;
  }

  return base::MakeRefCounted<ExternalConstantsOverrider>(
      std::move(*parsed_value).TakeDict(), next_provider);
}

// Declared in external_constants.h. This implementation of the function is
// used only if external_constants_override is linked into the binary.
scoped_refptr<ExternalConstants> CreateExternalConstants() {
  scoped_refptr<ExternalConstants> overrider =
      ExternalConstantsOverrider::FromDefaultJSONFile(
          CreateDefaultExternalConstants());
  return overrider ? overrider : CreateDefaultExternalConstants();
}

}  // namespace updater
