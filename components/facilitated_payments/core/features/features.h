// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FACILITATED_PAYMENTS_CORE_FEATURES_FEATURES_H_
#define COMPONENTS_FACILITATED_PAYMENTS_CORE_FEATURES_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "build/build_config.h"

namespace payments::facilitated {

BASE_DECLARE_FEATURE(kDisableFacilitatedPaymentsMerchantAllowlist);
BASE_DECLARE_FEATURE(kEnableDesktopQrCodeDetection);
// Comma-separated keywords matched against the URL, the document title and the
// `<meta name="description">` content.
extern const base::FeatureParam<std::string> kQrCodeDetectionKeywords;
// Minimum length, in CSS pixels, of the shorter edge of a QR candidate
// `<img>` or `<canvas>`.
extern const base::FeatureParam<int> kQrCodeDetectionMinImageSize;
// Maximum ratio of the longer edge to the shorter edge of a QR candidate.
extern const base::FeatureParam<double> kQrCodeDetectionMaxAspectRatio;
BASE_DECLARE_FEATURE(kEnableEwalletNewAccountLinking);
BASE_DECLARE_FEATURE(kEnableIframeForPix);
#if BUILDFLAG(IS_ANDROID)
BASE_DECLARE_FEATURE(kEnablePixAccountLinking);
#endif  // BUILDFLAG(IS_ANDROID)
BASE_DECLARE_FEATURE(kEnablePixAccountLinkingNative);
extern const base::FeatureParam<int>
    kPixAccountLinkingNativeTriggerDelaySeconds;
#if BUILDFLAG(IS_ANDROID)
extern const base::FeatureParam<std::string>
    kPixAccountLinkingNativePromptVariant;
extern const base::FeatureParam<std::string> kVideoUrlOnPrompt;
#endif  // BUILDFLAG(IS_ANDROID)
BASE_DECLARE_FEATURE(kEnablePixInCct);
BASE_DECLARE_FEATURE(kEnablePixPaymentsInLandscapeMode);
BASE_DECLARE_FEATURE(kEnableStaticQrCodeForPix);
#if BUILDFLAG(IS_ANDROID)
BASE_DECLARE_FEATURE(kEwalletPayments);
#endif  // BUILDFLAG(IS_ANDROID)
BASE_DECLARE_FEATURE(kFacilitatedPaymentsEnableA2APayment);

}  // namespace payments::facilitated

#endif  // COMPONENTS_FACILITATED_PAYMENTS_CORE_FEATURES_FEATURES_H_
