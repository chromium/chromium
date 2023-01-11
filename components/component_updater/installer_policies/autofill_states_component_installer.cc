// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/component_updater/installer_policies/autofill_states_component_installer.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/ranges/algorithm.h"
#include "base/values.h"
#include "components/autofill/core/browser/geo/country_data.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/component_updater/component_updater_service.h"
#include "components/prefs/pref_registry_simple.h"

namespace {

// The SHA256 of the SubjectPublicKeyInfo used to sign the extension.
// The extension id is: eeigpngbgcognadeebkilcpcaedhellh
const std::array<uint8_t, 32> kAutofillStatesPublicKeySHA256 = {
    0x44, 0x86, 0xfd, 0x61, 0x62, 0xe6, 0xd0, 0x34, 0x41, 0xa8, 0xb2,
    0xf2, 0x04, 0x37, 0x4b, 0xb7, 0x0b, 0xae, 0x93, 0x12, 0x9d, 0x58,
    0x15, 0xb5, 0xdd, 0x89, 0xf2, 0x98, 0x73, 0xd3, 0x08, 0x97};

// Returns the filenames corresponding to the states data.
std::vector<base::FilePath> AutofillStateFileNames() {
  std::vector<base::FilePath> filenames;
  for (const auto& country_code :
       autofill::CountryDataMap::GetInstance()->country_codes()) {
    filenames.push_back(base::FilePath().AppendASCII(country_code));
  }
  return filenames;
}

}  // namespace

namespace component_updater {

// static
void AutofillStatesComponentInstallerPolicy::RegisterPrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterStringPref(autofill::prefs::kAutofillStatesDataDir, "");
}

AutofillStatesComponentInstallerPolicy::AutofillStatesComponentInstallerPolicy(
    PrefService* pref_service)
    : pref_service_(pref_service) {}

AutofillStatesComponentInstallerPolicy::
    ~AutofillStatesComponentInstallerPolicy() = default;

bool AutofillStatesComponentInstallerPolicy::
    SupportsGroupPolicyEnabledComponentUpdates() const {
  return true;
}

bool AutofillStatesComponentInstallerPolicy::RequiresNetworkEncryption() const {
  return false;
}

update_client::CrxInstaller::Result
AutofillStatesComponentInstallerPolicy::OnCustomInstall(
    const base::Value::Dict& manifest,
    const base::FilePath& install_dir) {
  return update_client::CrxInstaller::Result(update_client::InstallError::NONE);
}

void AutofillStatesComponentInstallerPolicy::OnCustomUninstall() {}

void AutofillStatesComponentInstallerPolicy::ComponentReady(
    const base::Version& version,
    const base::FilePath& install_dir,
    base::Value::Dict manifest) {
  DVLOG(1) << "Component ready, version " << version.GetString() << " in "
           << install_dir.value();

  DCHECK(pref_service_);
  pref_service_->SetFilePath(autofill::prefs::kAutofillStatesDataDir,
                             install_dir);
}

// Called during startup and installation before ComponentReady().
bool AutofillStatesComponentInstallerPolicy::VerifyInstallation(
    const base::Value::Dict& manifest,
    const base::FilePath& install_dir) const {
  // Verify that state files are present.
  return base::ranges::count(
             AutofillStateFileNames(), true, [&](const auto& filename) {
               return base::PathExists(install_dir.Append(filename));
             }) > 0;
}

base::FilePath AutofillStatesComponentInstallerPolicy::GetRelativeInstallDir()
    const {
  return base::FilePath(FILE_PATH_LITERAL("AutofillStates"));
}

void AutofillStatesComponentInstallerPolicy::GetHash(
    std::vector<uint8_t>* hash) const {
  hash->assign(kAutofillStatesPublicKeySHA256.begin(),
               kAutofillStatesPublicKeySHA256.end());
}

std::string AutofillStatesComponentInstallerPolicy::GetName() const {
  return "Autofill States Data";
}

update_client::InstallerAttributes
AutofillStatesComponentInstallerPolicy::GetInstallerAttributes() const {
  return update_client::InstallerAttributes();
}

void RegisterAutofillStatesComponent(ComponentUpdateService* cus,
                                     PrefService* prefs) {
  DVLOG(1) << "Registering Autofill States data component.";
  auto installer = base::MakeRefCounted<ComponentInstaller>(
      std::make_unique<AutofillStatesComponentInstallerPolicy>(prefs));
  installer->Register(cus, base::OnceClosure());
}

}  // namespace component_updater
