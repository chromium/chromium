// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/component_updater/installer_policies/amount_extraction_heuristic_regexes_component_installer.h"

#include <optional>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/task/thread_pool.h"
#include "base/values.h"
#include "components/autofill/core/browser/metrics/payments/amount_extraction_metrics.h"
#include "components/autofill/core/browser/payments/amount_extraction_heuristic_regexes.h"

namespace component_updater {
namespace {

const base::FilePath::CharType
    kAmountExtractionHeuristicRegexesBinaryPbFileName[] =
        FILE_PATH_LITERAL("heuristic_regexes.binarypb");

// The SHA256 of the SubjectPublicKeyInfo used to sign the extension.
// The extension id is: hajigopbbjhghbfimgkfmpenfkclmohk
constexpr std::array<uint8_t, 32>
    kAmountExtractionHeuristicRegexesPublicKeySHA256 = {
        0x70, 0x98, 0x6e, 0xf1, 0x19, 0x76, 0x71, 0x58, 0xc6, 0xa5, 0xcf,
        0x4d, 0x5a, 0x2b, 0xce, 0x7a, 0x37, 0x44, 0x42, 0x77, 0x6c, 0x2d,
        0x2e, 0xa2, 0x9c, 0x08, 0x7a, 0x2c, 0x0f, 0x07, 0xe5, 0x89};

base::FilePath GetInstalledPath(const base::FilePath& base) {
  return base.Append(kAmountExtractionHeuristicRegexesBinaryPbFileName);
}

static std::optional<std::string> LoadAmountExtractionHeuristicRegexesFromDisk(
    const base::FilePath& pb_path) {
  std::string binary_pb;
  if (!base::ReadFileToString(pb_path, &binary_pb)) {
    autofill::autofill_metrics::LogAmountExtractionComponentInstallationResult(
        autofill::autofill_metrics::
            AmountExtractionComponentInstallationResult::
                kReadingBinaryFileFailed);
    return std::nullopt;
  }

  return binary_pb;
}

static void PopulateAmountExtractionHeuristicRegexesData(
    const base::FilePath& pb_path,
    std::optional<std::string> binary_pb) {
  if (!binary_pb || binary_pb.value().empty()) {
    autofill::autofill_metrics::LogAmountExtractionComponentInstallationResult(
        autofill::autofill_metrics::
            AmountExtractionComponentInstallationResult::kEmptyBinaryFile);
    return;
  }

  autofill::payments::AmountExtractionHeuristicRegexes::GetInstance()
      .PopulateStringFromComponent(std::move(*binary_pb));
}

}  // namespace

AmountExtractionHeuristicRegexesInstallerPolicy::
    AmountExtractionHeuristicRegexesInstallerPolicy() = default;

AmountExtractionHeuristicRegexesInstallerPolicy::
    ~AmountExtractionHeuristicRegexesInstallerPolicy() = default;

bool AmountExtractionHeuristicRegexesInstallerPolicy::
    SupportsGroupPolicyEnabledComponentUpdates() const {
  return true;
}

bool AmountExtractionHeuristicRegexesInstallerPolicy::
    RequiresNetworkEncryption() const {
  return false;
}

update_client::CrxInstaller::Result
AmountExtractionHeuristicRegexesInstallerPolicy::OnCustomInstall(
    const base::Value::Dict& /* manifest */,
    const base::FilePath& /* install_dir */) {
  return update_client::CrxInstaller::Result(0);  // Nothing custom here.
}

void AmountExtractionHeuristicRegexesInstallerPolicy::OnCustomUninstall() {}

void AmountExtractionHeuristicRegexesInstallerPolicy::ComponentReady(
    const base::Version& version,
    const base::FilePath& install_dir,
    base::Value::Dict /* manifest */) {
  const base::FilePath pb_path = GetInstalledPath(install_dir);

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&LoadAmountExtractionHeuristicRegexesFromDisk, pb_path),
      base::BindOnce(&PopulateAmountExtractionHeuristicRegexesData, pb_path));
}

// Called during startup and installation before ComponentReady().
bool AmountExtractionHeuristicRegexesInstallerPolicy::VerifyInstallation(
    const base::Value::Dict& /* manifest */,
    const base::FilePath& install_dir) const {
  bool installation_result = base::PathExists(GetInstalledPath(install_dir));
  if (!installation_result) {
    autofill::autofill_metrics::LogAmountExtractionComponentInstallationResult(
        autofill::autofill_metrics::
            AmountExtractionComponentInstallationResult::
                kInvalidInstallationPath);
  }
  return installation_result;
}

base::FilePath
AmountExtractionHeuristicRegexesInstallerPolicy::GetRelativeInstallDir() const {
  return base::FilePath(FILE_PATH_LITERAL("AmountExtractionHeuristicRegexes"));
}

void AmountExtractionHeuristicRegexesInstallerPolicy::GetHash(
    std::vector<uint8_t>* hash) const {
  hash->assign(std::begin(kAmountExtractionHeuristicRegexesPublicKeySHA256),
               std::end(kAmountExtractionHeuristicRegexesPublicKeySHA256));
}

std::string AmountExtractionHeuristicRegexesInstallerPolicy::GetName() const {
  return "Amount Extraction Heuristic Regexes";
}

update_client::InstallerAttributes
AmountExtractionHeuristicRegexesInstallerPolicy::GetInstallerAttributes()
    const {
  return update_client::InstallerAttributes();
}

void RegisterAmountExtractionHeuristicRegexesComponent(
    ComponentUpdateService* cus) {
  auto installer = base::MakeRefCounted<ComponentInstaller>(
      std::make_unique<AmountExtractionHeuristicRegexesInstallerPolicy>());
  installer->Register(cus, base::OnceClosure());
}

}  // namespace component_updater
