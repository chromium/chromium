// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/component_updater/installer_policies/on_device_head_suggest_component_installer.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "base/files/file_util.h"
#include "base/functional/callback.h"
#include "base/metrics/field_trial_params.h"
#include "base/path_service.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "components/component_updater/component_installer.h"
#include "components/component_updater/component_updater_service.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/browser/on_device_model_update_listener.h"

namespace component_updater {

namespace {
// CRX hash. The extension id is: obedbbhbpmojnkanicioggnmelmoomoc.
const uint8_t kOnDeviceHeadSuggestPublicKeySHA256[32] = {
    0xe1, 0x43, 0x11, 0x71, 0xfc, 0xe9, 0xda, 0x0d, 0x82, 0x8e, 0x66,
    0xdc, 0x4b, 0xce, 0xec, 0xe2, 0xa3, 0xb0, 0x47, 0x00, 0x3d, 0xb8,
    0xcf, 0x8e, 0x0f, 0x4a, 0x73, 0xa1, 0x89, 0x1f, 0x5f, 0x38};

// Get the normalized locale from a given raw locale, i.e capitalizes all
// letters and removes all hyphens and underscores in the locale string, e.g.
// "en-US" -> "ENUS". If param "ForceModelLocaleConstraint" is set, append it
// to the normalized locale as suffix.
std::string GetNormalizedLocale(const std::string& raw_locale) {
  std::string locale, locale_constraint;
  // Both incognito and non-incognito will use a same model so it's okay to
  // fetch the param from either feature.
  if (OmniboxFieldTrial::IsOnDeviceHeadSuggestEnabledForIncognito()) {
    locale_constraint =
        OmniboxFieldTrial::OnDeviceHeadModelLocaleConstraint(true);
  } else if (OmniboxFieldTrial::IsOnDeviceHeadSuggestEnabledForNonIncognito()) {
    locale_constraint =
        OmniboxFieldTrial::OnDeviceHeadModelLocaleConstraint(false);
  }

  locale = raw_locale;
  for (const auto c : "-_") {
    locale.erase(std::remove(locale.begin(), locale.end(), c), locale.end());
  }

  base::ranges::transform(locale, locale.begin(),
                          [](char c) { return base::ToUpperASCII(c); });

  if (!locale_constraint.empty()) {
    locale += locale_constraint;
  }

  VLOG(1) << "On Device Head Component will fetch model for locale: " << locale;

  return locale;
}

}  // namespace

OnDeviceHeadSuggestInstallerPolicy::OnDeviceHeadSuggestInstallerPolicy(
    const std::string& locale)
    : accept_locale_(GetNormalizedLocale(locale)) {}

OnDeviceHeadSuggestInstallerPolicy::~OnDeviceHeadSuggestInstallerPolicy() =
    default;

bool OnDeviceHeadSuggestInstallerPolicy::VerifyInstallation(
    const base::Value::Dict& manifest,
    const base::FilePath& install_dir) const {
  const std::string* name = manifest.FindString("name");

  if (!name || *name != ("OnDeviceHeadSuggest" + accept_locale_)) {
    return false;
  }

  bool is_successful = base::PathExists(install_dir);
  VLOG(1) << "On Device head model "
          << (is_successful ? "is successfully" : "cannot be")
          << " installed at directory: " << install_dir.value();

  return is_successful;
}

bool OnDeviceHeadSuggestInstallerPolicy::
    SupportsGroupPolicyEnabledComponentUpdates() const {
  return true;
}

bool OnDeviceHeadSuggestInstallerPolicy::RequiresNetworkEncryption() const {
  return false;
}

update_client::CrxInstaller::Result
OnDeviceHeadSuggestInstallerPolicy::OnCustomInstall(
    const base::Value::Dict& manifest,
    const base::FilePath& install_dir) {
  return update_client::CrxInstaller::Result(0);  // Nothing custom here.
}

void OnDeviceHeadSuggestInstallerPolicy::OnCustomUninstall() {}

void OnDeviceHeadSuggestInstallerPolicy::ComponentReady(
    const base::Version& version,
    const base::FilePath& install_dir,
    base::Value::Dict manifest) {
  auto* listener = OnDeviceModelUpdateListener::GetInstance();
  if (listener) {
    listener->OnHeadModelUpdate(install_dir);
  }
}

base::FilePath OnDeviceHeadSuggestInstallerPolicy::GetRelativeInstallDir()
    const {
  return base::FilePath(FILE_PATH_LITERAL("OnDeviceHeadSuggestModel"));
}

void OnDeviceHeadSuggestInstallerPolicy::GetHash(
    std::vector<uint8_t>* hash) const {
  hash->assign(std::begin(kOnDeviceHeadSuggestPublicKeySHA256),
               std::end(kOnDeviceHeadSuggestPublicKeySHA256));
}

std::string OnDeviceHeadSuggestInstallerPolicy::GetName() const {
  return "OnDeviceHeadSuggest";
}

update_client::InstallerAttributes
OnDeviceHeadSuggestInstallerPolicy::GetInstallerAttributes() const {
  return {{"accept_locale", accept_locale_}};
}

void RegisterOnDeviceHeadSuggestComponent(ComponentUpdateService* cus,
                                          const std::string& locale) {
  // Ideally we should only check if the feature is enabled for non-incognito or
  // incognito, but whether the browser is currently on incognito or not is not
  // available yet during component registration on iOS platform.
  if (OmniboxFieldTrial::IsOnDeviceHeadSuggestEnabledForLocale(locale)) {
    auto installer = base::MakeRefCounted<ComponentInstaller>(
        std::make_unique<OnDeviceHeadSuggestInstallerPolicy>(locale));
    installer->Register(cus, base::OnceClosure());
  }
}

}  // namespace component_updater
