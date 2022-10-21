// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/metrics/histogram_functions.h"
#include "chromeos/ash/components/language/language_packs/language_packs_impl.h"

#include "base/no_destructor.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash::language_packs {

using ::ash::language::mojom::BasePackInfo;
using ::ash::language::mojom::FeatureId;
using ::ash::language::mojom::LanguagePackInfo;
using ::ash::language::mojom::LanguagePacks;
using ::ash::language::mojom::PackState;

namespace {

absl::optional<std::string> ConvertMojoFeatureToPackId(FeatureId mojo_id) {
  switch (mojo_id) {
    case FeatureId::HANDWRITING_RECOGNITION:
      return kHandwritingFeatureId;

    case FeatureId::TTS:
      return kTtsFeatureId;

    // Catch all unknown cases here.
    default:
      return absl::nullopt;
  }
}

PackState GetPackStateFromStatusCode(const PackResult::StatusCode status_code) {
  switch (status_code) {
    case PackResult::NOT_INSTALLED:
      return PackState::NOT_INSTALLED;
    case PackResult::IN_PROGRESS:
      return PackState::INSTALLING;
    case PackResult::INSTALLED:
      return PackState::INSTALLED;
    // Catch all remaining cases as error.
    default:
      return PackState::ERROR;
  }
}

// Called when GetPackState() or InstallPack() functions from Language Packs
// are complete.
void OnOperationComplete(LanguagePacksImpl::GetPackInfoCallback mojo_callback,
                         const PackResult& pack_result) {
  auto info = LanguagePackInfo::New();
  info->pack_state = GetPackStateFromStatusCode(pack_result.pack_state);
  if (pack_result.pack_state == PackResult::INSTALLED) {
    info->path = pack_result.path;
  }

  base::UmaHistogramEnumeration("ChromeOS.LanguagePacks.Mojo.PackStateResponse",
                                info->pack_state);

  std::move(mojo_callback).Run(std::move(info));
}

// Called when InstallBasePack() from Language Packs is complete.
void OnInstallBasePackComplete(
    LanguagePacksImpl::InstallBasePackCallback mojo_callback,
    const PackResult& pack_result) {
  auto info = BasePackInfo::New();
  info->pack_state = GetPackStateFromStatusCode(pack_result.pack_state);
  if (pack_result.pack_state == PackResult::INSTALLED) {
    info->path = pack_result.path;
  }

  base::UmaHistogramEnumeration(
      "ChromeOS.LanguagePacks.Mojo.BasePackStateResponse", info->pack_state);

  std::move(mojo_callback).Run(std::move(info));
}

}  // namespace

LanguagePacksImpl::LanguagePacksImpl() = default;
LanguagePacksImpl::~LanguagePacksImpl() = default;

LanguagePacksImpl& LanguagePacksImpl::GetInstance() {
  static base::NoDestructor<LanguagePacksImpl> impl;
  return *impl;
}

void LanguagePacksImpl::BindReceiver(
    mojo::PendingReceiver<language::mojom::LanguagePacks> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void LanguagePacksImpl::GetPackInfo(FeatureId feature_id,
                                    const std::string& language,
                                    GetPackInfoCallback mojo_callback) {
  base::UmaHistogramEnumeration(
      "ChromeOS.LanguagePacks.Mojo.GetPackInfo.Feature", feature_id);

  LanguagePackManager* lp = LanguagePackManager::GetInstance();
  const absl::optional<std::string> pack_id =
      ConvertMojoFeatureToPackId(feature_id);

  if (pack_id.has_value()) {
    lp->GetPackState(
        pack_id.value(), language,
        base::BindOnce(&OnOperationComplete, std::move(mojo_callback)));
  } else {
    auto info = LanguagePackInfo::New();
    info->pack_state = PackState::ERROR;
    std::move(mojo_callback).Run(std::move(info));
  }
}

void LanguagePacksImpl::InstallPack(FeatureId feature_id,
                                    const std::string& language,
                                    InstallPackCallback mojo_callback) {
  base::UmaHistogramEnumeration(
      "ChromeOS.LanguagePacks.Mojo.InstallPack.Feature", feature_id);

  LanguagePackManager* lp = LanguagePackManager::GetInstance();
  const absl::optional<std::string> pack_id =
      ConvertMojoFeatureToPackId(feature_id);

  if (pack_id.has_value()) {
    lp->InstallPack(
        pack_id.value(), language,
        base::BindOnce(&OnOperationComplete, std::move(mojo_callback)));
  } else {
    auto info = LanguagePackInfo::New();
    info->pack_state = PackState::ERROR;
    std::move(mojo_callback).Run(std::move(info));
  }
}

void LanguagePacksImpl::InstallBasePack(FeatureId feature_id,
                                        InstallBasePackCallback mojo_callback) {
  base::UmaHistogramEnumeration(
      "ChromeOS.LanguagePacks.Mojo.InstallBasePack.Feature", feature_id);

  LanguagePackManager* lp = LanguagePackManager::GetInstance();
  const absl::optional<std::string> pack_id =
      ConvertMojoFeatureToPackId(feature_id);

  if (pack_id.has_value()) {
    lp->InstallBasePack(*pack_id, base::BindOnce(&OnInstallBasePackComplete,
                                                 std::move(mojo_callback)));
  } else {
    auto info = BasePackInfo::New();
    info->pack_state = PackState::ERROR;
    std::move(mojo_callback).Run(std::move(info));
  }
}

void LanguagePacksImpl::UninstallPack(FeatureId feature_id,
                                      const std::string& language,
                                      UninstallPackCallback mojo_callback) {
  LanguagePackManager* lp = LanguagePackManager::GetInstance();
  const absl::optional<std::string> pack_id =
      ConvertMojoFeatureToPackId(feature_id);

  // We ignore the request if the input parameters are incorrect.
  if (pack_id.has_value()) {
    lp->RemovePack(pack_id.value(), language, base::DoNothing());
  }
}

}  // namespace ash::language_packs
