// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBID_IDENTITY_DIALOG_CONTROLLER_H_
#define CHROME_BROWSER_UI_WEBID_IDENTITY_DIALOG_CONTROLLER_H_

#include <memory>
#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/ui/webid/account_selection_view.h"
#include "chrome/browser/webid/proto/fedcm_clickthrough_rate_metadata.pb.h"
#include "components/segmentation_platform/public/segmentation_platform_service.h"
#include "components/segmentation_platform/public/trigger.h"
#include "content/public/browser/identity_request_dialog_controller.h"
#include "content/public/browser/web_contents.h"
#include "ui/gfx/native_widget_types.h"

using AccountSelectionCallback =
    content::IdentityRequestDialogController::AccountSelectionCallback;
using DismissCallback =
    content::IdentityRequestDialogController::DismissCallback;
using IdentityProviderDataPtr = scoped_refptr<content::IdentityProviderData>;
using IdentityRequestAccountPtr =
    scoped_refptr<content::IdentityRequestAccount>;
using TokenError = content::IdentityCredentialTokenError;

// The IdentityDialogController controls the views that are used across
// browser-mediated federated sign-in flows.
class IdentityDialogController
    : public content::IdentityRequestDialogController,
      public AccountSelectionView::Delegate {
 public:
  explicit IdentityDialogController(
      content::WebContents* rp_web_contents,
      segmentation_platform::SegmentationPlatformService* service = nullptr,
      optimization_guide::OptimizationGuideDecider* decider = nullptr);

  IdentityDialogController(const IdentityDialogController&) = delete;
  IdentityDialogController& operator=(const IdentityDialogController&) = delete;

  ~IdentityDialogController() override;

  // This enum describes the user action taken when the UI shown uses
  // segmentation platform's UI volume recommendation and is used for
  // histograms. Do not remove or modify existing values, but you may add new
  // values at the end. This enum should be kept in sync with FedCmUserAction in
  // tools/metrics/histograms/enums.xml.
  enum class UserAction {
    // kSuccess = 0,  // Deprecated.
    kIgnored = 1,
    kClosed = 2,
    kSuccess = 3,

    kMaxValue = kSuccess
  };

  // content::IdentityRequestDelegate
  int GetBrandIconMinimumSize(blink::mojom::RpMode rp_mode) override;
  int GetBrandIconIdealSize(blink::mojom::RpMode rp_mode) override;

  // content::IdentityRequestDialogController
  bool ShowAccountsDialog(
      content::RelyingPartyData rp_data,
      const std::vector<IdentityProviderDataPtr>& identity_provider_data,
      const std::vector<IdentityRequestAccountPtr>& accounts,
      blink::mojom::RpMode rp_mode,
      const std::vector<IdentityRequestAccountPtr>& new_accounts,
      AccountSelectionCallback on_selected,
      LoginToIdPCallback on_add_account,
      DismissCallback dismiss_callback,
      AccountsDisplayedCallback accounts_displayed_callback) override;
  bool ShowFailureDialog(const content::RelyingPartyData& rp_data,
                         const std::string& idp_for_display,
                         blink::mojom::RpContext rp_context,
                         blink::mojom::RpMode rp_mode,
                         const content::IdentityProviderMetadata& idp_metadata,
                         DismissCallback dismiss_callback,
                         LoginToIdPCallback login_callback) override;
  bool ShowErrorDialog(const content::RelyingPartyData& rp_data,
                       const std::string& idp_for_display,
                       blink::mojom::RpContext rp_context,
                       blink::mojom::RpMode rp_mode,
                       const content::IdentityProviderMetadata& idp_metadata,
                       const std::optional<TokenError>& error,
                       DismissCallback dismiss_callback,
                       MoreDetailsCallback more_details_callback) override;
  bool ShowLoadingDialog(const content::RelyingPartyData& rp_data,
                         const std::string& idp_for_display,
                         blink::mojom::RpContext rp_context,
                         blink::mojom::RpMode rp_mode,
                         DismissCallback dismiss_callback) override;
  bool ShowVerifyingDialog(
      const content::RelyingPartyData& rp_data,
      const IdentityProviderDataPtr& idp_data,
      const IdentityRequestAccountPtr& account,
      Account::SignInMode sign_in_mode,
      blink::mojom::RpMode rp_mode,
      AccountsDisplayedCallback accounts_displayed_callback) override;

  std::string GetTitle() const override;
  std::optional<std::string> GetSubtitle() const override;

  void ShowUrl(LinkType type, const GURL& url) override;
  // Show a modal dialog that loads content from the IdP in a WebView.
  content::WebContents* ShowModalDialog(
      const GURL& url,
      blink::mojom::RpMode rp_mode,
      DismissCallback dismiss_callback) override;
  void CloseModalDialog() override;
  content::WebContents* GetRpWebContents() override;

  // AccountSelectionView::Delegate:
  void OnAccountSelected(
      const GURL& idp_config_url,
      const std::string& account_id,
      const content::IdentityRequestAccount::LoginState& login_state) override;
  void OnDismiss(DismissReason dismiss_reason) override;
  void OnLoginToIdP(const GURL& idp_config_url,
                    const GURL& idp_login_url) override;
  void OnMoreDetails() override;
  void OnAccountsDisplayed() override;
  gfx::NativeView GetNativeView() override;
  content::WebContents* GetWebContents() override;

  // Request the IdP Registration permission.
  void RequestIdPRegistrationPermision(
      const url::Origin& origin,
      base::OnceCallback<void(bool accepted)> callback) override;

  // Allows setting a mock AccountSelectionView for testing purposes.
  void SetAccountSelectionViewForTesting(
      std::unique_ptr<AccountSelectionView> account_view);

  // Requests a UI volume recommendation from |segmentation_platform_service_|.
  void RequestUiVolumeRecommendation(
      segmentation_platform::ClassificationResultCallback callback);

  // Called when |RequestUiVolumeRecommendation| returns a result.
  void OnRequestUiVolumeRecommendationResultReceived(
      const content::RelyingPartyData& rp_data,
      const std::vector<IdentityProviderDataPtr>& identity_provider_data,
      const std::vector<IdentityRequestAccountPtr>& accounts,
      blink::mojom::RpMode rp_mode,
      const std::vector<IdentityRequestAccountPtr>& new_accounts,
      const segmentation_platform::ClassificationResult&
          ui_volume_recommendation);

  // Records the action the user has taken on the UI shown when a UI volume
  // recommendation from |segmentation_platform_service_| is used.
  void CollectTrainingData(UserAction user_action);

 private:
  // Attempts to set `account_view_` if it is not already set -- directly on
  // Android, via TabFeatures on desktop.
  bool TrySetAccountView();

  // Gets the clickthrough rate on the RP aggregated across all users.
  webid::FedCmClickthroughRateMetadata GetFedCmClickthroughRateMetadata();

  std::unique_ptr<AccountSelectionView> account_view_{nullptr};
  AccountSelectionCallback on_account_selection_;
  DismissCallback on_dismiss_;
  LoginToIdPCallback on_login_;
  MoreDetailsCallback on_more_details_;
  AccountsDisplayedCallback on_accounts_displayed_;
  raw_ptr<content::WebContents> rp_web_contents_{nullptr};
  blink::mojom::RpMode rp_mode_;

  // Request ID associated with a |GetClassificationResult| call to
  // |segmentation_platform_service_|. This is nullopt when the
  // |GetClassificationResult| call has not returned a result yet.
  std::optional<segmentation_platform::TrainingRequestId> training_request_id_;

  // Service which returns a recommendation for UI volume.
  raw_ptr<segmentation_platform::SegmentationPlatformService>
      segmentation_platform_service_{nullptr};

  // Optimization guide decider for information about URLs that have recently
  // been navigated to. e.g. Aggregated FedCM clickthrough rate.
  raw_ptr<optimization_guide::OptimizationGuideDecider>
      optimization_guide_decider_{nullptr};

  base::WeakPtrFactory<IdentityDialogController> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_WEBID_IDENTITY_DIALOG_CONTROLLER_H_
