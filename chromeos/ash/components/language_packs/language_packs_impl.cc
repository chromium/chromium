// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "chromeos/ash/components/language_packs/language_packs_impl.h"

#include "base/no_destructor.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash::language_packs {

using ::ash::language::mojom::BasePackInfo;
using ::ash::language::mojom::ErrorCode;
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
    case PackResult::StatusCode::kNotInstalled:
      return PackState::NOT_INSTALLED;
    case PackResult::StatusCode::kInProgress:
      return PackState::INSTALLING;
    case PackResult::StatusCode::kInstalled:
      return PackState::INSTALLED;
    // Catch all remaining cases as error.
    default:
      // TODO: b/294162606 - Deprecate this value and use UNKNOWN instead.
      return PackState::ERROR;
  }
}

ErrorCode GetMojoErrorFromPackError(const PackResult::ErrorCode pack_error) {
  // This conversion is exhaustive. We don't use a default: case so that we can
  // catch missing values at compile time.
  switch (pack_error) {
    case PackResult::ErrorCode::kNone:
      return ErrorCode::kNone;
    case PackResult::ErrorCode::kOther:
      return ErrorCode::kOther;
    case PackResult::ErrorCode::kWrongId:
      return ErrorCode::kWrongId;
    case PackResult::ErrorCode::kNeedReboot:
      return ErrorCode::kNeedReboot;
    case PackResult::ErrorCode::kAllocation:
      return ErrorCode::kAllocation;
  }
}

// Called when GetPackState() or InstallPack() functions from Language Packs
// are complete.
void OnOperationComplete(LanguagePacksImpl::GetPackInfoCallback mojo_callback,
                         const PackResult& pack_result) {
  auto info = LanguagePackInfo::New();
  info->pack_state = GetPackStateFromStatusCode(pack_result.pack_state);
  info->error = GetMojoErrorFromPackError(pack_result.operation_error);
  if (pack_result.pack_state == PackResult::StatusCode::kInstalled) {
    info->path = pack_result.path;
  }

  std::move(mojo_callback).Run(std::move(info));
}

// Called when InstallBasePack() from Language Packs is complete.
void OnInstallBasePackComplete(
    LanguagePacksImpl::InstallBasePackCallback mojo_callback,
    const PackResult& pack_result) {
  auto info = BasePackInfo::New();
  info->pack_state = GetPackStateFromStatusCode(pack_result.pack_state);
  info->error = GetMojoErrorFromPackError(pack_result.operation_error);
  if (pack_result.pack_state == PackResult::StatusCode::kInstalled) {
    info->path = pack_result.path;
  }

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
