// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_COMPOSEBOX_VARIATIONS_COMPOSEBOX_FIELDTRIAL_H_
#define CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_COMPOSEBOX_VARIATIONS_COMPOSEBOX_FIELDTRIAL_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "chrome/browser/profiles/profile.h"
#include "components/omnibox/common/omnibox_feature_configs.h"
#include "third_party/omnibox_proto/ntp_composebox_config.pb.h"

class Profile;

namespace ntp_composebox {

inline constexpr char kConfigParamParseSuccessHistogram[] =
    "NewTabPage.Composebox.ConfigParseSuccess";

// If overridden to false, disables the feature (kill switch). If true, enables
// the feature.
BASE_DECLARE_FEATURE(kNtpComposebox);

// The serialized base64 encoded `omnibox::NTPComposeboxConfig`.
extern const base::FeatureParam<std::string> kConfigParam;
// Whether to send the lns_surface parameter.
// TODO(crbug.com/430070871): Remove this flag once the server supports the
// `lns_surface` parameter.
extern const base::FeatureParam<bool> kSendLnsSurfaceParam;
// Whether to show zps suggestions under the composebox.
extern const base::FeatureParam<bool> kShowComposeboxZps;
// Whether to show typed suggestions under the composebox.
extern const base::FeatureParam<bool> kShowComposeboxTypedSuggest;
// Whether to show the + entrypoint and contextual input menu in the realbox and
// composebox.
extern const base::FeatureParam<bool> kShowContextMenu;
// The maximum number of tab suggestions to show in the composebox context menu.
extern const base::FeatureParam<int> kContextMenuMaxTabSuggestions;

bool IsNtpComposeboxEnabled(Profile* profile);

class FeatureConfig : public omnibox_feature_configs::Config<FeatureConfig> {
 public:
  // The configuration proto for the feature.
  omnibox::NTPComposeboxConfig config;

  FeatureConfig(const FeatureConfig&);
  FeatureConfig(FeatureConfig&&);
  FeatureConfig& operator=(const FeatureConfig&);
  FeatureConfig& operator=(FeatureConfig&&);
  ~FeatureConfig();

 private:
  friend class omnibox_feature_configs::Config<FeatureConfig>;
  friend class omnibox_feature_configs::ScopedConfigForTesting<FeatureConfig>;
  FeatureConfig();
};

using ScopedFeatureConfigForTesting =
    omnibox_feature_configs::ScopedConfigForTesting<FeatureConfig>;

}  // namespace ntp_composebox

#endif  // CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_COMPOSEBOX_VARIATIONS_COMPOSEBOX_FIELDTRIAL_H_
