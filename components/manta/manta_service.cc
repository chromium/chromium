// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/manta/manta_service.h"

#include <memory>

#include "base/containers/contains.h"
#include "base/containers/fixed_flat_set.h"
#include "base/memory/scoped_refptr.h"
#include "build/chromeos_buildflags.h"
#include "components/account_id/account_id.h"
#include "components/manta/anchovy/anchovy_provider.h"
#include "components/manta/provider_params.h"
#include "components/manta/sparky/system_info_delegate.h"
#include "components/signin/public/identity_manager/account_capabilities.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/tribool.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chromeos/constants/chromeos_features.h"  // nogncheck
#include "components/manta/mahi_provider.h"
#include "components/manta/orca_provider.h"
#include "components/manta/scanner_provider.h"
#include "components/manta/snapper_provider.h"
#include "components/manta/sparky/sparky_provider.h"
#include "components/manta/walrus_provider.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace manta {

namespace {

FeatureSupportStatus ConvertToMantaFeatureSupportStatus(signin::Tribool value) {
  switch (value) {
    case signin::Tribool::kUnknown:
      return FeatureSupportStatus::kUnknown;
    case signin::Tribool::kTrue:
      return FeatureSupportStatus::kSupported;
    case signin::Tribool::kFalse:
      return FeatureSupportStatus::kUnsupported;
  }
}

#if BUILDFLAG(IS_CHROMEOS_ASH)

constexpr auto kAllowedLanguagesForAddingLocaleToRequest =
    base::MakeFixedFlatSet<std::string_view>({"de", "en", "en-GB", "fr", "ja"});

bool ShouldIncludeLocaleInRequest(std::string_view locale) {
  return chromeos::features::IsOrcaUseL10nStringsEnabled() ||
         (chromeos::features::IsOrcaInternationalizeEnabled() &&
          base::Contains(kAllowedLanguagesForAddingLocaleToRequest, locale));
}

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace

MantaService::MantaService(
    scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory,
    signin::IdentityManager* identity_manager,
    bool is_demo_mode,
    bool is_otr_profile,
    const std::string& chrome_version,
    const version_info::Channel chrome_channel,
    const std::string& locale)
    : shared_url_loader_factory_(shared_url_loader_factory),
      identity_manager_(identity_manager),
      is_demo_mode_(is_demo_mode),
      is_otr_profile_(is_otr_profile),
      chrome_version_(chrome_version),
      chrome_channel_(chrome_channel),
      locale_(locale) {}

MantaService::~MantaService() = default;

FeatureSupportStatus MantaService::SupportsOrca() {
  if (is_demo_mode_) {
    return FeatureSupportStatus::kSupported;
  }
  return CanAccessMantaFeaturesWithoutMinorRestrictions();
}

FeatureSupportStatus
MantaService::CanAccessMantaFeaturesWithoutMinorRestrictions() {
  if (identity_manager_ == nullptr) {
    return FeatureSupportStatus::kUnknown;
  }

  const auto account_id =
      identity_manager_->GetPrimaryAccountId(signin::ConsentLevel::kSignin);

  if (account_id.empty()) {
    return FeatureSupportStatus::kUnknown;
  }

  const AccountInfo extended_account_info =
      identity_manager_->FindExtendedAccountInfoByAccountId(account_id);

  // Fetches and uses the shared account capability for manta service.
  return ConvertToMantaFeatureSupportStatus(
      extended_account_info.capabilities.can_use_manta_service());
}

std::unique_ptr<AnchovyProvider> MantaService::CreateAnchovyProvider() {
  // Anchovy Provider supports API Key Requests for OTR profiles and doesn't
  // requires a valid identity_manager.
  const ProviderParams provider_params = {/*use_api_key=*/is_otr_profile_,
                                          chrome_version_, chrome_channel_,
                                          locale_};

  return std::make_unique<AnchovyProvider>(shared_url_loader_factory_,
                                           identity_manager_, provider_params);
}

#if BUILDFLAG(IS_CHROMEOS_ASH)

std::unique_ptr<OrcaProvider> MantaService::CreateOrcaProvider() {
  if (!identity_manager_) {
    return nullptr;
  }
  const ProviderParams provider_params = {
      /*use_api_key=*/is_demo_mode_, chrome_version_, chrome_channel_,
      /*locale=*/
      ShouldIncludeLocaleInRequest(locale_) ? locale_ : std::string()};
  return std::make_unique<OrcaProvider>(shared_url_loader_factory_,
                                        identity_manager_, provider_params);
}

std::unique_ptr<ScannerProvider> MantaService::CreateScannerProvider() {
  if (!identity_manager_) {
    return nullptr;
  }
  const ProviderParams provider_params = {/*use_api_key=*/is_demo_mode_,
                                          chrome_version_, chrome_channel_};
  return std::make_unique<ScannerProvider>(shared_url_loader_factory_,
                                           identity_manager_, provider_params);
}

std::unique_ptr<SnapperProvider> MantaService::CreateSnapperProvider() {
  if (!identity_manager_) {
    return nullptr;
  }
  const ProviderParams provider_params = {/*use_api_key=*/is_demo_mode_,
                                          chrome_version_, chrome_channel_};
  return std::make_unique<SnapperProvider>(shared_url_loader_factory_,
                                           identity_manager_, provider_params);
}

std::unique_ptr<MahiProvider> MantaService::CreateMahiProvider() {
  if (!identity_manager_) {
    return nullptr;
  }
  const ProviderParams provider_params = {/*use_api_key=*/is_demo_mode_,
                                          chrome_version_, chrome_channel_};
  return std::make_unique<MahiProvider>(shared_url_loader_factory_,
                                        identity_manager_, provider_params);
}

std::unique_ptr<SparkyProvider> MantaService::CreateSparkyProvider(
    std::unique_ptr<SparkyDelegate> sparky_delegate,
    std::unique_ptr<SystemInfoDelegate> system_info_delegate) {
  if (!identity_manager_ || !sparky_delegate || !system_info_delegate) {
    return nullptr;
  }
  const ProviderParams provider_params = {/*use_api_key=*/is_demo_mode_,
                                          chrome_version_, chrome_channel_};
  return std::make_unique<SparkyProvider>(
      shared_url_loader_factory_, identity_manager_, provider_params,
      std::move(sparky_delegate), std::move(system_info_delegate));
}

std::unique_ptr<WalrusProvider> MantaService::CreateWalrusProvider() {
  if (!identity_manager_) {
    return nullptr;
  }
  const ProviderParams provider_params = {/*use_api_key=*/is_demo_mode_,
                                          chrome_version_, chrome_channel_};
  return std::make_unique<WalrusProvider>(shared_url_loader_factory_,
                                          identity_manager_, provider_params);
}

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

void MantaService::Shutdown() {
  identity_manager_ = nullptr;
}

}  // namespace manta
