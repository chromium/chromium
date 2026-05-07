// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/facilitated_payments/core/features/features.h"

#include "base/metrics/field_trial_params.h"

namespace payments::facilitated {

// When enabled, Chrome will offer to pay with accounts supporting Pix to users
// using their devices in landscape mode. Chrome always offers to pay with Pix
// accounts for users using their devices in portrait mode.
BASE_FEATURE(kEnablePixPaymentsInLandscapeMode,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Use the Rust implementation of the Pix code validator.
BASE_FEATURE(kUseRustPixCodeValidator, base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_ANDROID)
// When enabled, the check for matching the main frame domain with the
// allowlisted domains will be disabled.
BASE_FEATURE(kDisableFacilitatedPaymentsMerchantAllowlist,
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, Chrome will prompt users without linked Pix accounts to link
// their Pix accounts to Google Wallet.
BASE_FEATURE(kEnablePixAccountLinking, base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, Chrome will offer a native Pix account linking flow for
// users on Chrome, removing the Wallet app dependency.
BASE_FEATURE(kEnablePixAccountLinkingNative, base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<std::string> kPixAccountLinkingNativePromptVariant{
    &kEnablePixAccountLinkingNative, "prompt_variant", "VariationA"};

const base::FeatureParam<int> kPixAccountLinkingNativeTriggerDelaySeconds{
    &kEnablePixAccountLinkingNative, "trigger_delay_seconds", 3};

// TODO: Replace with a public YouTube link for production to guarantee access.
const base::FeatureParam<std::string> kVideoUrlOnPrompt{
    &kEnablePixAccountLinkingNative, "video_url_on_prompt",
    "https://support.google.com/wallet/answer/14616353?hl=en"};

// When enabled, static qr code will be supported for pix pay flow.
BASE_FEATURE(kEnableStaticQrCodeForPix, base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, pix pay flow will be triggered when users click the copy button
// within iframe.
BASE_FEATURE(kEnableIframeForPix, base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, Pix code detection will be supported in Chrome Custom Tabs.
BASE_FEATURE(kEnablePixInCct, base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, Chrome will offer to pay with eWallet accounts if a payment
// link is detected.
BASE_FEATURE(kEwalletPayments, base::FEATURE_ENABLED_BY_DEFAULT);

// When enabled, Chrome will offer an app list when a supported payment link is
// detected. Users can choose the payment app they want to
// use and be redirected to the chosen app to complete the payment flow.
BASE_FEATURE(kFacilitatedPaymentsEnableA2APayment,
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace payments::facilitated
