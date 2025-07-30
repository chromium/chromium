// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_COMPOSEBOX_VARIATIONS_COMPOSEBOX_FIELDTRIAL_H_
#define CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_COMPOSEBOX_VARIATIONS_COMPOSEBOX_FIELDTRIAL_H_

#include "base/metrics/field_trial_params.h"
#include "chrome/browser/browser_process.h"
#include "components/omnibox/common/omnibox_feature_configs.h"
#include "third_party/omnibox_proto/ntp_composebox_config.pb.h"

namespace ntp_composebox {

inline constexpr char kConfigParamParseSuccessHistogram[] =
    "NewTabPage.Composebox.ConfigParseSuccess";

BASE_DECLARE_FEATURE(kNtpSearchboxComposeEntrypoint);
BASE_DECLARE_FEATURE(kNtpSearchboxComposeEntrypointEnglishUS);

bool IsNtpSearchboxComposeEntrypointEnabled(BrowserProcess* browser_process);

BASE_DECLARE_FEATURE(kNtpComposebox);
// The serialized base64 encoded `omnibox::NTPComposeboxConfig`.
extern const base::FeatureParam<std::string> kConfigParam;
// Whether to send the lns_surface parameter.
// TODO(crbug.com/430070871): Remove this flag once the server supports the
// `lns_surface` parameter.
extern const base::FeatureParam<bool> kSendLnsSurfaceParam;
// Whether to show zps suggestions under the composebox.
extern const base::FeatureParam<bool> kShowComposeboxZps;

struct FeatureConfig : omnibox_feature_configs::Config<FeatureConfig> {
  // Whether the feature is enabled.
  bool enabled = false;
  // The configuration proto for the feature.
  omnibox::NTPComposeboxConfig config;

 private:
  friend class omnibox_feature_configs::Config<FeatureConfig>;
  friend class omnibox_feature_configs::ScopedConfigForTesting<FeatureConfig>;
  FeatureConfig();
};

using ScopedFeatureConfigForTesting =
    omnibox_feature_configs::ScopedConfigForTesting<FeatureConfig>;

}  // namespace ntp_composebox

#endif  // CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_COMPOSEBOX_VARIATIONS_COMPOSEBOX_FIELDTRIAL_H_
