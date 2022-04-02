// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/metrics/histogram_functions.h"
#include "chromeos/language/language_packs/language_packs_impl.h"

#include "base/no_destructor.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace chromeos::language_packs {

using ::chromeos::language::mojom::FeatureId;
using ::chromeos::language::mojom::LanguagePackInfo;
using ::chromeos::language::mojom::LanguagePacks;
using ::chromeos::language::mojom::PackState;

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

// Called when GetPackState() or InstallPack() functions from Language Packs
// are complete.
void OnOperationComplete(LanguagePacksImpl::GetPackInfoCallback mojo_callback,
                         const PackResult& pack_result) {
  auto info = LanguagePackInfo::New();
  switch (pack_result.pack_state) {
    case PackResult::NOT_INSTALLED:
      info->pack_state = PackState::NOT_INSTALLED;
      break;
    case PackResult::IN_PROGRESS:
      info->pack_state = PackState::INSTALLING;
      break;
    case PackResult::INSTALLED:
      info->pack_state = PackState::INSTALLED;
      info->path = pack_result.path;
      break;

    // Catch all remaining cases as error.
    default:
      info->pack_state = PackState::ERROR;
      break;
  }

  base::UmaHistogramEnumeration("ChromeOS.LanguagePacks.Mojo.PackStateResponse",
                                info->pack_state);

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

}  // namespace chromeos::language_packs
