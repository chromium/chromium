// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/facilitated_payments/core/browser/facilitated_payments_client.h"

#include <memory>
#include <string_view>
#include <utility>

#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/data_model/payments/bank_account.h"
#include "components/autofill/core/browser/data_model/payments/ewallet.h"
#include "components/facilitated_payments/core/browser/facilitated_payments_app_info_list.h"
#include "components/facilitated_payments/core/utils/facilitated_payments_ui_utils.h"
#include "url/origin.h"

#if BUILDFLAG(IS_ANDROID)
#include "components/facilitated_payments/core/browser/pix_account_linking_manager.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace payments::facilitated {

#if BUILDFLAG(IS_ANDROID)
FacilitatedPaymentsClient::FacilitatedPaymentsClient()
    : pix_account_linking_manager_(
          std::make_unique<PixAccountLinkingManager>(/* client= */ this)) {}
#else
FacilitatedPaymentsClient::FacilitatedPaymentsClient() = default;
#endif  // BUILDFLAG(IS_ANDROID)

FacilitatedPaymentsClient::~FacilitatedPaymentsClient() = default;

#if BUILDFLAG(IS_ANDROID)
void FacilitatedPaymentsClient::SetPixAccountLinkingManagerForTesting(
    std::unique_ptr<PixAccountLinkingManager> pix_account_linking_manager) {
  pix_account_linking_manager_ = std::move(pix_account_linking_manager);
}
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace payments::facilitated
