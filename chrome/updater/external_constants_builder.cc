// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/external_constants_builder.h"

#include <iterator>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/files/file_util.h"
#include "base/json/json_file_value_serializer.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/external_constants_default.h"
#include "chrome/updater/external_constants_override.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/util/util.h"
#include "components/crx_file/crx_verifier.h"
#include "url/gurl.h"

namespace updater {

namespace {

std::vector<std::string> StringVectorFromGURLVector(
    const std::vector<GURL>& gurls) {
  std::vector<std::string> ret;
  ret.reserve(gurls.size());

  base::ranges::transform(gurls, std::back_inserter(ret), [](const GURL& gurl) {
    return gurl.possibly_invalid_spec();
  });

  return ret;
}

}  // namespace

ExternalConstantsBuilder::~ExternalConstantsBuilder() {
  LOG_IF(WARNING, !written_)
      << "An ExternalConstantsBuilder with " << overrides_.size()
      << " entries is being " << "discarded without being written to a file.";
}

ExternalConstantsBuilder& ExternalConstantsBuilder::SetUpdateURL(
    const std::vector<std::string>& urls) {
  base::Value::List url_list;
  url_list.reserve(urls.size());
  for (const std::string& url_string : urls) {
    url_list.Append(url_string);
  }
  overrides_.Set(kDevOverrideKeyUrl, std::move(url_list));
  return *this;
}

ExternalConstantsBuilder& ExternalConstantsBuilder::ClearUpdateURL() {
  overrides_.Remove(kDevOverrideKeyUrl);
  return *this;
}

ExternalConstantsBuilder& ExternalConstantsBuilder::SetCrashUploadURL(
    const std::string& url) {
  overrides_.Set(kDevOverrideKeyCrashUploadUrl, url);
  return *this;
}

ExternalConstantsBuilder& ExternalConstantsBuilder::ClearCrashUploadURL() {
  overrides_.Remove(kDevOverrideKeyCrashUploadUrl);
  return *this;
}

ExternalConstantsBuilder& ExternalConstantsBuilder::SetDeviceManagementURL(
    const std::string& url) {
  overrides_.Set(kDevOverrideKeyDeviceManagementUrl, url);
  return *this;
}

ExternalConstantsBuilder& ExternalConstantsBuilder::ClearDeviceManagementURL() {
  overrides_.Remove(kDevOverrideKeyDeviceManagementUrl);
  return *this;
}

ExternalConstantsBuilder& ExternalConstantsBuilder::SetAppLogoURL(
    const std::string& url) {
  overrides_.Set(kDevOverrideKeyAppLogoUrl, url);
  return *this;
}

ExternalConstantsBuilder& ExternalConstantsBuilder::ClearAppLogoURL() {
  overrides_.Remove(kDevOverrideKeyAppLogoUrl);
  return *this;
}

ExternalConstantsBuilder& ExternalConstantsBuilder::SetUseCUP(bool use_cup) {
  overrides_.Set(kDevOverrideKeyUseCUP, use_cup);
  return *this;
}

ExternalConstantsBuilder& ExternalConstantsBuilder::ClearUseCUP() {
  overrides_.Remove(kDevOverrideKeyUseCUP);
  return *this;
}

ExternalConstantsBuilder& ExternalConstantsBuilder::SetInitialDelay(
    base::TimeDelta initial_delay) {
  overrides_.Set(kDevOverrideKeyInitialDelay, initial_delay.InSecondsF());
  return *this;
}

ExternalConstantsBuilder& ExternalConstantsBuilder::ClearInitialDelay() {
  overrides_.Remove(kDevOverrideKeyInitialDelay);
  return *this;
}

ExternalConstantsBuilder& ExternalConstantsBuilder::SetServerKeepAliveTime(
    base::TimeDelta server_keep_alive_time) {
  overrides_.Set(kDevOverrideKeyServerKeepAliveSeconds,
                 base::checked_cast<int>(server_keep_alive_time.InSeconds()));
  return *this;
}

ExternalConstantsBuilder&
ExternalConstantsBuilder::ClearServerKeepAliveSeconds() {
  overrides_.Remove(kDevOverrideKeyServerKeepAliveSeconds);
  return *this;
}

ExternalConstantsBuilder& ExternalConstantsBuilder::SetCrxVerifierFormat(
    crx_file::VerifierFormat crx_verifier_format) {
  overrides_.Set(kDevOverrideKeyCrxVerifierFormat,
                 static_cast<int>(crx_verifier_format));
  return *this;
}

ExternalConstantsBuilder& ExternalConstantsBuilder::ClearCrxVerifierFormat() {
  overrides_.Remove(kDevOverrideKeyCrxVerifierFormat);
  return *this;
}

ExternalConstantsBuilder& ExternalConstantsBuilder::SetGroupPolicies(
    const base::Value::Dict& group_policies) {
  overrides_.Set(kDevOverrideKeyGroupPolicies, group_policies.Clone());
  return *this;
}

ExternalConstantsBuilder& ExternalConstantsBuilder::ClearGroupPolicies() {
  overrides_.Remove(kDevOverrideKeyGroupPolicies);
  return *this;
}

ExternalConstantsBuilder& ExternalConstantsBuilder::SetOverinstallTimeout(
    base::TimeDelta overinstall_timeout) {
  overrides_.Set(kDevOverrideKeyOverinstallTimeout,
                 static_cast<int>(overinstall_timeout.InSeconds()));
  return *this;
}

ExternalConstantsBuilder& ExternalConstantsBuilder::ClearOverinstallTimeout() {
  overrides_.Remove(kDevOverrideKeyOverinstallTimeout);
  return *this;
}

ExternalConstantsBuilder& ExternalConstantsBuilder::SetIdleCheckPeriod(
    base::TimeDelta idle_check_period) {
  overrides_.Set(kDevOverrideKeyIdleCheckPeriodSeconds,
                 static_cast<int>(idle_check_period.InSeconds()));
  return *this;
}

ExternalConstantsBuilder& ExternalConstantsBuilder::ClearIdleCheckPeriod() {
  overrides_.Remove(kDevOverrideKeyIdleCheckPeriodSeconds);
  return *this;
}

ExternalConstantsBuilder& ExternalConstantsBuilder::SetMachineManaged(
    const std::optional<bool>& is_managed_device) {
  if (is_managed_device.has_value()) {
    overrides_.Set(kDevOverrideKeyManagedDevice, is_managed_device.value());
  }

  return *this;
}

ExternalConstantsBuilder& ExternalConstantsBuilder::ClearMachineManaged() {
  overrides_.Remove(kDevOverrideKeyManagedDevice);
  return *this;
}

ExternalConstantsBuilder& ExternalConstantsBuilder::SetEnableDiffUpdates(
    bool enable_diffs) {
  overrides_.Set(kDevOverrideKeyEnableDiffUpdates, enable_diffs);
  return *this;
}

ExternalConstantsBuilder& ExternalConstantsBuilder::ClearEnableDiffUpdates() {
  overrides_.Remove(kDevOverrideKeyEnableDiffUpdates);
  return *this;
}

ExternalConstantsBuilder& ExternalConstantsBuilder::SetCecaConnectionTimeout(
    base::TimeDelta ceca_connection_timeout) {
  overrides_.Set(kDevOverrideKeyCecaConnectionTimeout,
                 static_cast<int>(ceca_connection_timeout.InSeconds()));
  return *this;
}

ExternalConstantsBuilder&
ExternalConstantsBuilder::ClearCecaConnectionTimeout() {
  overrides_.Remove(kDevOverrideKeyCecaConnectionTimeout);
  return *this;
}

bool ExternalConstantsBuilder::Overwrite() {
  const std::optional<base::FilePath> override_path =
      GetOverrideFilePath(GetUpdaterScope());
  if (!override_path) {
    LOG(ERROR) << "Can't find base directory; can't save constant overrides.";
    return false;
  }
  if (!base::CreateDirectory(override_path->DirName())) {
    LOG(ERROR) << "Can't create " << override_path->value();
    return false;
  }

  bool ok = JSONFileValueSerializer(*override_path).Serialize(overrides_);
  written_ = written_ || ok;
  return ok;
}

bool ExternalConstantsBuilder::Modify() {
  scoped_refptr<ExternalConstantsOverrider> verifier =
      ExternalConstantsOverrider::FromDefaultJSONFile(
          CreateDefaultExternalConstants());
  if (!verifier) {
    return Overwrite();
  }

  if (!overrides_.contains(kDevOverrideKeyUrl)) {
    SetUpdateURL(StringVectorFromGURLVector(verifier->UpdateURL()));
  }
  if (!overrides_.contains(kDevOverrideKeyCrashUploadUrl)) {
    SetCrashUploadURL(verifier->CrashUploadURL().possibly_invalid_spec());
  }
  if (!overrides_.contains(kDevOverrideKeyDeviceManagementUrl)) {
    SetDeviceManagementURL(
        verifier->DeviceManagementURL().possibly_invalid_spec());
  }
  if (!overrides_.contains(kDevOverrideKeyAppLogoUrl)) {
    SetAppLogoURL(verifier->AppLogoURL().possibly_invalid_spec());
  }
  if (!overrides_.contains(kDevOverrideKeyUseCUP)) {
    SetUseCUP(verifier->UseCUP());
  }
  if (!overrides_.contains(kDevOverrideKeyInitialDelay)) {
    SetInitialDelay(verifier->InitialDelay());
  }
  if (!overrides_.contains(kDevOverrideKeyServerKeepAliveSeconds)) {
    SetServerKeepAliveTime(verifier->ServerKeepAliveTime());
  }
  if (!overrides_.contains(kDevOverrideKeyCrxVerifierFormat)) {
    SetCrxVerifierFormat(verifier->CrxVerifierFormat());
  }
  if (!overrides_.contains(kDevOverrideKeyGroupPolicies)) {
    SetGroupPolicies(verifier->GroupPolicies());
  }
  if (!overrides_.contains(kDevOverrideKeyOverinstallTimeout)) {
    SetOverinstallTimeout(verifier->OverinstallTimeout());
  }
  if (!overrides_.contains(kDevOverrideKeyIdleCheckPeriodSeconds)) {
    SetIdleCheckPeriod(verifier->IdleCheckPeriod());
  }
  if (!overrides_.contains(kDevOverrideKeyManagedDevice)) {
    SetMachineManaged(verifier->IsMachineManaged());
  }
  if (!overrides_.contains(kDevOverrideKeyEnableDiffUpdates)) {
    SetEnableDiffUpdates(verifier->EnableDiffUpdates());
  }
  if (!overrides_.contains(kDevOverrideKeyCecaConnectionTimeout)) {
    SetCecaConnectionTimeout(verifier->CecaConnectionTimeout());
  }

  return Overwrite();
}

}  // namespace updater
