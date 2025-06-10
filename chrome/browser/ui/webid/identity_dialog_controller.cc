// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webid/identity_dialog_controller.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "build/build_config.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/segmentation_platform/segmentation_platform_service_factory.h"
#include "chrome/browser/ui/browser_finder.h"
#include "components/favicon/content/content_favicon_driver.h"
#include "components/favicon/core/favicon_driver.h"
#include "components/segmentation_platform/public/constants.h"
#include "components/segmentation_platform/public/features.h"
#include "components/segmentation_platform/public/result.h"
#include "components/segmentation_platform/public/segmentation_platform_service.h"

// We add nognchecks on these includes so that Android bots do not fail
// dependency checks.
#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/views/webid/fedcm_account_selection_view_desktop.h"  // nogncheck
#include "components/tabs/public/tab_interface.h"  // nogncheck
#endif

#include "chrome/browser/ui/webid/account_selection_view.h"
#include "chrome/browser/webid/identity_provider_permission_request.h"
#include "components/permissions/permission_request_manager.h"
#include "third_party/blink/public/mojom/webid/federated_auth_request.mojom-shared.h"

IdentityDialogController::IdentityDialogController(
    content::WebContents* rp_web_contents,
    segmentation_platform::SegmentationPlatformService* service,
    optimization_guide::OptimizationGuideDecider* decider)
    : rp_web_contents_(rp_web_contents),
      segmentation_platform_service_(service),
      optimization_guide_decider_(decider) {
  if (!base::FeatureList::IsEnabled(
          segmentation_platform::features::kSegmentationPlatformFedCmUser)) {
    return;
  }

  Profile* profile = Profile::FromBrowserContext(
      rp_web_contents_->GetPrimaryMainFrame()->GetBrowserContext());
  if (profile->IsOffTheRecord()) {
    return;
  }

  if (!segmentation_platform_service_) {
    segmentation_platform_service_ = segmentation_platform::
        SegmentationPlatformServiceFactory::GetForProfile(profile);
  }

  if (!optimization_guide_decider_) {
    optimization_guide_decider_ =
        OptimizationGuideKeyedServiceFactory::GetForProfile(profile);
  }
  optimization_guide_decider_->RegisterOptimizationTypes(
      {optimization_guide::proto::OptimizationType::FEDCM_CLICKTHROUGH_RATE});
}

IdentityDialogController::~IdentityDialogController() = default;

int IdentityDialogController::GetBrandIconMinimumSize(
    blink::mojom::RpMode rp_mode) {
  return AccountSelectionView::GetBrandIconMinimumSize(rp_mode);
}

int IdentityDialogController::GetBrandIconIdealSize(
    blink::mojom::RpMode rp_mode) {
  return AccountSelectionView::GetBrandIconIdealSize(rp_mode);
}

bool IdentityDialogController::ShowAccountsDialog(
    content::RelyingPartyData rp_data,
    const std::vector<IdentityProviderDataPtr>& identity_provider_data,
    const std::vector<IdentityRequestAccountPtr>& accounts,
    blink::mojom::RpMode rp_mode,
    const std::vector<IdentityRequestAccountPtr>& new_accounts,
    AccountSelectionCallback on_selected,
    LoginToIdPCallback on_add_account,
    DismissCallback dismiss_callback,
    AccountsDisplayedCallback accounts_displayed_callback) {
  on_account_selection_ = std::move(on_selected);
  on_login_ = std::move(on_add_account);
  on_dismiss_ = std::move(dismiss_callback);
  on_accounts_displayed_ = std::move(accounts_displayed_callback);
  rp_mode_ = rp_mode;
  if (!TrySetAccountView()) {
    return false;
  }
  favicon::FaviconDriver* favicon_driver =
      favicon::ContentFaviconDriver::FromWebContents(rp_web_contents_);
  // Currently, FaviconIsValid() is never true on Android, and GetFavicon()
  // returns the default favicon, so we will not use this result and instead
  // obtain the favicon from the Java code.
  if (favicon_driver && favicon_driver->FaviconIsValid()) {
    rp_data.rp_icon = favicon_driver->GetFavicon();
  }

  // If widget mode and segmentation platform feature flag is enabled, make the
  // call to segmentation platform service for a UI volume recommendation.
  if (rp_mode == blink::mojom::RpMode::kPassive &&
      base::FeatureList::IsEnabled(
          segmentation_platform::features::kSegmentationPlatformFedCmUser)) {
    RequestUiVolumeRecommendation(base::BindOnce(
        &IdentityDialogController::
            OnRequestUiVolumeRecommendationResultReceived,
        weak_ptr_factory_.GetWeakPtr(), rp_data, identity_provider_data,
        accounts, rp_mode, new_accounts));
    return true;
  }

  return account_view_->Show(rp_data, identity_provider_data, accounts, rp_mode,
                             new_accounts);
}

bool IdentityDialogController::ShowFailureDialog(
    const content::RelyingPartyData& rp_data,
    const std::string& idp_for_display,
    blink::mojom::RpContext rp_context,
    blink::mojom::RpMode rp_mode,
    const content::IdentityProviderMetadata& idp_metadata,
    DismissCallback dismiss_callback,
    LoginToIdPCallback login_callback) {
  const GURL rp_url = rp_web_contents_->GetLastCommittedURL();
  on_dismiss_ = std::move(dismiss_callback);
  on_login_ = std::move(login_callback);
  if (!TrySetAccountView()) {
    return false;
  }
  // Else:
  //   TODO: If the failure dialog is already being shown, notify user that
  //   sign-in attempt failed.

  return account_view_->ShowFailureDialog(rp_data, idp_for_display, rp_context,
                                          rp_mode, idp_metadata);
}

bool IdentityDialogController::ShowErrorDialog(
    const content::RelyingPartyData& rp_data,
    const std::string& idp_for_display,
    blink::mojom::RpContext rp_context,
    blink::mojom::RpMode rp_mode,
    const content::IdentityProviderMetadata& idp_metadata,
    const std::optional<TokenError>& error,
    DismissCallback dismiss_callback,
    MoreDetailsCallback more_details_callback) {
  on_dismiss_ = std::move(dismiss_callback);
  on_more_details_ = std::move(more_details_callback);
  if (!TrySetAccountView()) {
    return false;
  }
  return account_view_->ShowErrorDialog(rp_data, idp_for_display, rp_context,
                                        rp_mode, idp_metadata, error);
}

bool IdentityDialogController::ShowLoadingDialog(
    const content::RelyingPartyData& rp_data,
    const std::string& idp_for_display,
    blink::mojom::RpContext rp_context,
    blink::mojom::RpMode rp_mode,
    DismissCallback dismiss_callback) {
  on_dismiss_ = std::move(dismiss_callback);
  if (!TrySetAccountView()) {
    return false;
  }
  return account_view_->ShowLoadingDialog(rp_data, idp_for_display, rp_context,
                                          rp_mode);
}

bool IdentityDialogController::ShowVerifyingDialog(
    const content::RelyingPartyData& rp_data,
    const IdentityProviderDataPtr& idp_data,
    const IdentityRequestAccountPtr& account,
    Account::SignInMode sign_in_mode,
    blink::mojom::RpMode rp_mode,
    AccountsDisplayedCallback accounts_displayed_callback) {
  on_accounts_displayed_ = std::move(accounts_displayed_callback);
  rp_mode_ = rp_mode;
  if (!TrySetAccountView()) {
    return false;
  }
  return account_view_->ShowVerifyingDialog(rp_data, idp_data, account,
                                            sign_in_mode, rp_mode);
}

void IdentityDialogController::OnLoginToIdP(const GURL& idp_config_url,
                                            const GURL& idp_login_url) {
  CHECK(on_login_);
  on_login_.Run(idp_config_url, idp_login_url);
}

void IdentityDialogController::OnMoreDetails() {
  CHECK(on_more_details_);
  std::move(on_more_details_).Run();
}

void IdentityDialogController::OnAccountsDisplayed() {
  CHECK(on_accounts_displayed_);
  std::move(on_accounts_displayed_).Run();
}

void IdentityDialogController::OnAccountSelected(
    const GURL& idp_config_url,
    const std::string& account_id,
    const content::IdentityRequestAccount::LoginState& login_state) {
  CHECK(on_account_selection_);

  CollectTrainingData(UserAction::kSuccess);

  // We only allow dismiss after account selection on active modes and not on
  // passive mode.
  // TODO(crbug.com/335886093): Figure out whether users can cancel after
  // selecting an account on active mode modal.
  if (rp_mode_ == blink::mojom::RpMode::kPassive) {
    on_dismiss_.Reset();
  }

  std::move(on_account_selection_)
      .Run(idp_config_url, account_id,
           login_state == content::IdentityRequestAccount::LoginState::kSignIn);
}

void IdentityDialogController::OnDismiss(DismissReason dismiss_reason) {
  // |OnDismiss| can be called after |OnAccountSelected| which sets the callback
  // to null.
  if (!on_dismiss_) {
    return;
  }

  if (dismiss_reason == DismissReason::kCloseButton ||
      dismiss_reason == DismissReason::kSwipe) {
    CollectTrainingData(UserAction::kClosed);
  } else {
    CollectTrainingData(UserAction::kIgnored);
  }

  on_account_selection_.Reset();
  std::move(on_dismiss_).Run(dismiss_reason);

  // Do not access member variables from this point onwards because
  // |on_dismiss_| may have destroyed this object.
}

std::string IdentityDialogController::GetTitle() const {
  return account_view_->GetTitle();
}

std::optional<std::string> IdentityDialogController::GetSubtitle() const {
  return account_view_->GetSubtitle();
}

gfx::NativeView IdentityDialogController::GetNativeView() {
  return rp_web_contents_->GetNativeView();
}

content::WebContents* IdentityDialogController::GetWebContents() {
  return rp_web_contents_;
}

void IdentityDialogController::ShowUrl(LinkType type, const GURL& url) {
  if (!account_view_) {
    return;
  }
  account_view_->ShowUrl(type, url);
}

content::WebContents* IdentityDialogController::ShowModalDialog(
    const GURL& url,
    blink::mojom::RpMode rp_mode,
    DismissCallback dismiss_callback) {
  on_dismiss_ = std::move(dismiss_callback);
  if (!TrySetAccountView()) {
    return nullptr;
  }

  return account_view_->ShowModalDialog(url, rp_mode);
}

void IdentityDialogController::CloseModalDialog() {
#if BUILDFLAG(IS_ANDROID)
  // On Android, this method is invoked on the modal dialog controller,
  // which means we may need to initialize the |account_view|.
  if (!account_view_) {
    account_view_ = AccountSelectionView::Create(this);
  }
#endif  // BUILDFLAG(IS_ANDROID)
  CHECK(account_view_);
  account_view_->CloseModalDialog();
}

content::WebContents* IdentityDialogController::GetRpWebContents() {
#if BUILDFLAG(IS_ANDROID)
  // On Android, this method is invoked on the modal dialog controller,
  // which means we may need to initialize the |account_view|.
  if (!account_view_) {
    account_view_ = AccountSelectionView::Create(this);
  }
#endif  // BUILDFLAG(IS_ANDROID)
  CHECK(account_view_);
  return account_view_->GetRpWebContents();
}

void IdentityDialogController::RequestIdPRegistrationPermision(
    const url::Origin& origin,
    base::OnceCallback<void(bool accepted)> callback) {
  permissions::PermissionRequestManager* permission_request_manager =
      permissions::PermissionRequestManager::FromWebContents(rp_web_contents_);

  permission_request_manager->AddRequest(
      rp_web_contents_->GetPrimaryMainFrame(),
      std::make_unique<IdentityProviderPermissionRequest>(origin,
                                                          std::move(callback)));
}

void IdentityDialogController::SetAccountSelectionViewForTesting(
    std::unique_ptr<AccountSelectionView> account_view) {
  account_view_ = std::move(account_view);
}

bool IdentityDialogController::TrySetAccountView() {
  if (account_view_) {
    return true;
  }
#if BUILDFLAG(IS_ANDROID)
  account_view_ = AccountSelectionView::Create(this);
#else
  tabs::TabInterface* tab =
      tabs::TabInterface::MaybeGetFromContents(rp_web_contents_);
  // FedCM is supported in general web content, but not in chrome UI. Of the
  // BrowserWindow types, devtools show Chrome UI and the rest show general web
  // content.
  if (!tab || tab->GetBrowserWindowInterface()->GetType() ==
                  BrowserWindowInterface::Type::TYPE_DEVTOOLS) {
    return false;
  }
  account_view_ = std::make_unique<webid::FedCmAccountSelectionView>(this, tab);
#endif
  return true;
}

void IdentityDialogController::RequestUiVolumeRecommendation(
    segmentation_platform::ClassificationResultCallback callback) {
  if (!segmentation_platform_service_) {
    segmentation_platform::ClassificationResult result(
        segmentation_platform::PredictionStatus::kFailed);
    std::move(callback).Run(result);
    return;
  }

  segmentation_platform::PredictionOptions prediction_options;
  prediction_options.on_demand_execution = true;
  scoped_refptr<segmentation_platform::InputContext> input_context =
      base::MakeRefCounted<segmentation_platform::InputContext>();
  webid::FedCmClickthroughRateMetadata metadata =
      GetFedCmClickthroughRateMetadata();
  input_context->metadata_args.emplace(
      segmentation_platform::kFedCmHost,
      segmentation_platform::processing::ProcessedValue(
          rp_web_contents_->GetLastCommittedURL().host()));
  input_context->metadata_args.emplace(
      segmentation_platform::kFedCmUrl,
      segmentation_platform::processing::ProcessedValue(
          rp_web_contents_->GetLastCommittedURL()));
  input_context->metadata_args.emplace(
      segmentation_platform::kFedCmPerPageLoadClickthroughRate,
      segmentation_platform::processing::ProcessedValue(
          metadata.per_page_load_clickthrough_rate()));
  input_context->metadata_args.emplace(
      segmentation_platform::kFedCmPerClientClickthroughRate,
      segmentation_platform::processing::ProcessedValue(
          metadata.per_client_clickthrough_rate()));
  input_context->metadata_args.emplace(
      segmentation_platform::kFedCmPerImpressionClickthroughRate,
      segmentation_platform::processing::ProcessedValue(
          metadata.per_impression_clickthrough_rate()));
  input_context->metadata_args.emplace(
      segmentation_platform::kFedCmLikelyToSignin,
      segmentation_platform::processing::ProcessedValue(
          metadata.likely_to_signin()));
  input_context->metadata_args.emplace(
      segmentation_platform::kFedCmLikelyInsufficientData,
      segmentation_platform::processing::ProcessedValue(
          metadata.likely_insufficient_data()));
  segmentation_platform_service_->GetClassificationResult(
      segmentation_platform::kFedCmUserKey, prediction_options, input_context,
      std::move(callback));
}

void IdentityDialogController::OnRequestUiVolumeRecommendationResultReceived(
    const content::RelyingPartyData& rp_data,
    const std::vector<IdentityProviderDataPtr>& identity_provider_data,
    const std::vector<IdentityRequestAccountPtr>& accounts,
    blink::mojom::RpMode rp_mode,
    const std::vector<IdentityRequestAccountPtr>& new_accounts,
    const segmentation_platform::ClassificationResult&
        ui_volume_recommendation) {
  training_request_id_ = ui_volume_recommendation.request_id;

  // Default to showing loud UI if the prediction fails for any reason.
  if (ui_volume_recommendation.status !=
          segmentation_platform::PredictionStatus::kSucceeded ||
      ui_volume_recommendation.ordered_labels[0] == "FedCmUserLoud") {
    account_view_->Show(rp_data, identity_provider_data, accounts, rp_mode,
                        new_accounts);
    return;
  }

  // TODO(crbug.com/380416872): Integrate with quiet UI. Until then, dismiss the
  // UI.
  OnDismiss(DismissReason::kSuppressed);
}

void IdentityDialogController::CollectTrainingData(UserAction user_action) {
  if (!training_request_id_.has_value() || !segmentation_platform_service_) {
    return;
  }

  ukm::SourceId source_id =
      (rp_web_contents_ && rp_web_contents_->GetPrimaryMainFrame())
          ? rp_web_contents_->GetPrimaryMainFrame()->GetPageUkmSourceId()
          : ukm::kInvalidSourceId;

  segmentation_platform::TrainingLabels training_labels;
  base::UmaHistogramEnumeration("Blink.FedCm.SegmentationPlatform.UserAction",
                                user_action);
  training_labels.output_metric =
      std::make_pair("Blink.FedCm.SegmentationPlatform.UserAction",
                     static_cast<base::HistogramBase::Sample32>(user_action));

  segmentation_platform_service_->CollectTrainingData(
      segmentation_platform::proto::SegmentId::
          OPTIMIZATION_TARGET_SEGMENTATION_FEDCM_USER,
      *training_request_id_, source_id, training_labels, base::DoNothing());
  training_request_id_ = std::nullopt;
  segmentation_platform_service_ = nullptr;
}

webid::FedCmClickthroughRateMetadata
IdentityDialogController::GetFedCmClickthroughRateMetadata() {
  if (!optimization_guide_decider_) {
    return webid::FedCmClickthroughRateMetadata();
  }

  optimization_guide::OptimizationMetadata opt_guide_metadata;
  auto opt_guide_has_hint = optimization_guide_decider_->CanApplyOptimization(
      rp_web_contents_->GetPrimaryMainFrame()->GetLastCommittedURL(),
      optimization_guide::proto::OptimizationType::FEDCM_CLICKTHROUGH_RATE,
      &opt_guide_metadata);
  if (opt_guide_has_hint !=
          optimization_guide::OptimizationGuideDecision::kTrue ||
      !opt_guide_metadata.any_metadata().has_value()) {
    return webid::FedCmClickthroughRateMetadata();
  }

  std::optional<webid::FedCmClickthroughRateMetadata> parsed_metadata =
      optimization_guide::ParsedAnyMetadata<
          webid::FedCmClickthroughRateMetadata>(
          opt_guide_metadata.any_metadata().value());
  if (!parsed_metadata.has_value()) {
    return webid::FedCmClickthroughRateMetadata();
  }
  return parsed_metadata.value();
}
