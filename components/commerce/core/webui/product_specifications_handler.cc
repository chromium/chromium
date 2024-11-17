// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/core/webui/product_specifications_handler.h"

#include "base/strings/utf_string_conversions.h"
#include "components/commerce/core/commerce_constants.h"
#include "components/commerce/core/commerce_utils.h"
#include "components/commerce/core/mojom/shared.mojom.h"
#include "components/commerce/core/pref_names.h"
#include "components/commerce/core/webui/webui_utils.h"
#include "components/prefs/pref_service.h"

namespace commerce {

ProductSpecificationsHandler::ProductSpecificationsHandler(
    mojo::PendingRemote<product_specifications::mojom::Page> remote_page,
    mojo::PendingReceiver<
        product_specifications::mojom::ProductSpecificationsHandler> receiver,
    std::unique_ptr<Delegate> delegate,
    history::HistoryService* history_service,
    PrefService* pref_service,
    ProductSpecificationsService* product_specs_service)
    : remote_page_(std::move(remote_page)),
      receiver_(this, std::move(receiver)),
      delegate_(std::move(delegate)),
      history_service_(history_service),
      pref_service_(pref_service) {
  if (product_specs_service) {
    scoped_product_specs_observer_.Observe(product_specs_service);
  }
}

ProductSpecificationsHandler::~ProductSpecificationsHandler() = default;

void ProductSpecificationsHandler::GetPageTitleFromHistory(
    const GURL& url,
    GetPageTitleFromHistoryCallback callback) {
  if (!history_service_) {
    std::move(callback).Run("");
    return;
  }

  history_service_->QueryURL(
      url, false,
      base::BindOnce(
          [](GetPageTitleFromHistoryCallback callback,
             history::QueryURLResult result) {
            std::move(callback).Run(
                result.success ? base::UTF16ToUTF8(result.row.title()) : "");
          },
          std::move(callback)),
      &cancelable_task_tracker_);
}

void ProductSpecificationsHandler::ShowProductSpecificationsSetForUuid(
    const base::Uuid& uuid,
    bool in_new_tab) {
  if (!delegate_) {
    return;
  }

  delegate_->ShowProductSpecificationsSetForUuid(uuid, in_new_tab);
}

void ProductSpecificationsHandler::SetAcceptedDisclosureVersion(
    product_specifications::mojom::DisclosureVersion version) {
  if (!pref_service_) {
    return;
  }

  pref_service_->SetInteger(kProductSpecificationsAcceptedDisclosureVersion,
                            static_cast<int>(version));
}

void ProductSpecificationsHandler::MaybeShowDisclosure(
    const std::vector<GURL>& urls,
    const std::string& name,
    const std::string& set_id,
    MaybeShowDisclosureCallback callback) {
  bool show = (pref_service_->GetInteger(
                   kProductSpecificationsAcceptedDisclosureVersion) ==
               static_cast<int>(
                   product_specifications::mojom::DisclosureVersion::kUnknown));
  if (show) {
    delegate_->ShowDisclosureDialog(urls, name, set_id);
  }
  std::move(callback).Run(show);
}

void ProductSpecificationsHandler::DeclineDisclosure() {
  if (!pref_service_) {
    return;
  }

  int current_gap_time = pref_service_->GetInteger(
      commerce::kProductSpecificationsEntryPointShowIntervalInDays);
  // Double the gap time for every dismiss, starting from one day.
  if (current_gap_time == 0) {
    current_gap_time = 1;
  } else {
    current_gap_time = std::min(2 * current_gap_time,
                                kProductSpecMaxEntryPointTriggeringInterval);
  }

  pref_service_->SetInteger(
      commerce::kProductSpecificationsEntryPointShowIntervalInDays,
      current_gap_time);
  pref_service_->SetTime(
      commerce::kProductSpecificationsEntryPointLastDismissedTime,
      base::Time::Now());
}

void ProductSpecificationsHandler::ShowSyncSetupFlow() {
  if (delegate_) {
    delegate_->ShowSyncSetupFlow();
  }
}

void ProductSpecificationsHandler::OnProductSpecificationsSetAdded(
    const ProductSpecificationsSet& set) {
  remote_page_->OnProductSpecificationsSetAdded(ProductSpecsSetToMojo(set));
}

void ProductSpecificationsHandler::OnProductSpecificationsSetUpdate(
    const ProductSpecificationsSet& before,
    const ProductSpecificationsSet& set) {
  remote_page_->OnProductSpecificationsSetUpdated(ProductSpecsSetToMojo(set));
}

void ProductSpecificationsHandler::OnProductSpecificationsSetRemoved(
    const ProductSpecificationsSet& set) {
  remote_page_->OnProductSpecificationsSetRemoved(set.uuid());
}

}  // namespace commerce
