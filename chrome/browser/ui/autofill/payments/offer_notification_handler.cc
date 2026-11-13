// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/offer_notification_handler.h"

#include <string>

#include "base/check.h"
#include "base/check_deref.h"
#include "base/feature_list.h"
#include "build/build_config.h"
#include "components/autofill/content/browser/content_autofill_client.h"
#include "components/autofill/core/browser/data_manager/personal_data_manager.h"
#include "components/autofill/core/browser/data_model/payments/autofill_offer_data.h"
#include "components/autofill/core/browser/payments/autofill_offer_manager.h"
#include "components/autofill/core/browser/payments/offer_notification_options.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/visibility.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/autofill/payments/offer_notification_bubble_controller_impl.h"
#endif

namespace autofill {

namespace {

bool IsOfferValid(const AutofillOfferData* offer) {
  if (!offer) {
    return false;
  }

  if (offer->GetMerchantOrigins().empty()) {
    return false;
  }

  if (offer->GetOfferType() == AutofillOfferData::OfferType::UNKNOWN) {
    return false;
  }

  if (!offer->GetOfferDetailsUrl().is_valid() ||
      offer->GetOfferShortTitle().empty()) {
    return false;
  }

  return true;
}

}  // namespace

OfferNotificationHandler::OfferNotificationHandler(
    ContentAutofillClient& client)
    : content::WebContentsObserver(&client.GetWebContents()), client_(client) {
  // Only needed to update the offer notification, which is otherwise not
  // shown.
  if (base::FeatureList::IsEnabled(
          features::kAutofillEnableWalletDirectOffers) &&
      base::FeatureList::IsEnabled(
          features::kAutofillEnableWalletDirectOffersNotificationBubble)) {
    payments_data_manager_observation_.Observe(
        &client_->GetPersonalDataManager().payments_data_manager());
  }
}

OfferNotificationHandler::~OfferNotificationHandler() = default;

void OfferNotificationHandler::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (navigation_handle->HasCommitted() &&
      !navigation_handle->IsSameDocument() &&
      navigation_handle->IsInPrimaryMainFrame()) {
    UpdateOfferNotificationVisibility();
  }
}

void OfferNotificationHandler::OnVisibilityChanged(
    content::Visibility visibility) {
  // `UpdateOfferNotificationVisibility()` does nothing unless the tab is
  // visible.
  UpdateOfferNotificationVisibility();
}

void OfferNotificationHandler::OnPaymentsDataChanged() {
  // This is sent for any change to the payments data, e.g. to cards or to
  // offers for other sites. Leave a showing bubble alone, since re-evaluating
  // would collapse it to its icon.
  if (IsOfferNotificationBubbleShowing()) {
    return;
  }
  UpdateOfferNotificationVisibility();
}

bool OfferNotificationHandler::IsOfferNotificationBubbleShowing() const {
#if BUILDFLAG(IS_ANDROID)
  return false;
#else
  OfferNotificationBubbleControllerImpl* controller =
      OfferNotificationBubbleControllerImpl::FromWebContents(web_contents());
  return controller && controller->IsShowingBubble();
#endif
}

void OfferNotificationHandler::UpdateOfferNotificationVisibility() {
  if (!base::FeatureList::IsEnabled(
          features::kAutofillEnableWalletDirectOffers) ||
      !base::FeatureList::IsEnabled(
          features::kAutofillEnableWalletDirectOffersNotificationBubble)) {
    return;
  }

  // Hidden tabs don't set up the notification, so that they cannot consume the
  // once-per-offer automatic show on behalf of the tab the user is looking at.
  // An occluded tab counts as hidden, since the user cannot see a notification
  // shown on it either. Re-evaluated in `OnVisibilityChanged()` once the tab is
  // shown.
  if (web_contents()->GetVisibility() != content::Visibility::VISIBLE) {
    return;
  }

  const GURL& url =
      web_contents()->GetPrimaryMainFrame()->GetLastCommittedURL();

  if (ValidOfferExistsForUrl(url)) {
    // TODO(crbug.com/40179715): GetOfferForUrl needs to know whether to give
    //   precedence to card-linked offers or promo code offers. Eventually,
    //   promo code offers should take precedence if a bubble is shown.
    //   Currently, if a url has both types of offers and the promo code offer
    //   is selected, no bubble will end up being shown (due to not yet being
    //   implemented).
    AutofillOfferManager& offer_manager = GetOfferManager();
    const AutofillOfferData* const offer = offer_manager.GetOfferForUrl(url);
    CHECK(IsOfferValid(offer));
    const std::string& offer_id = offer->GetOfferId();
    bool offer_id_has_shown_before =
        offer_manager.HasShownNotification(offer_id);
    client_->GetPaymentsAutofillClient()->UpdateOfferNotification(
        *offer,
        {
            .notification_has_been_shown = offer_id_has_shown_before,
            .show_notification_automatically = !offer_id_has_shown_before,
        });
    offer_manager.MarkNotificationShown(offer_id);
  } else {
    client_->GetPaymentsAutofillClient()->DismissOfferNotification();
  }
}

bool OfferNotificationHandler::ValidOfferExistsForUrl(const GURL& url) const {
  AutofillOfferManager& offer_manager = GetOfferManager();
  return offer_manager.IsUrlEligible(url) &&
         IsOfferValid(offer_manager.GetOfferForUrl(url));
}

AutofillOfferManager& OfferNotificationHandler::GetOfferManager() const {
  return CHECK_DEREF(
      client_->GetPaymentsAutofillClient()->GetAutofillOfferManager());
}

}  // namespace autofill
