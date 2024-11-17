// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_PERMISSION_INDICATORS_TAB_DATA_H_
#define COMPONENTS_PERMISSIONS_PERMISSION_INDICATORS_TAB_DATA_H_

#include "base/time/time.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace permissions {

class PermissionIndicatorsTabData : public content::WebContentsObserver {
 public:
  // LHS indicators type. Currently only camera and mic are supported.
  enum class IndicatorsType { kMediaStream };

  // This is the key for recording indicator usage within different frequencies
  // metrics.
  // LINT.IfChange(Duration)
  enum Duration {
    kFourSeconds = 0,
    kTenSeconds = 1,
    kOneMinute = 2,
    kFiveMinutes = 3,
    kTenMinutes = 4,
    kOneHour = 5,
    kMoreThanOneHour = 6,

    // Always keep this at the end.
    kMaxValue = kMoreThanOneHour,
  };
  // LINT.ThenChange(/tools/metrics/histograms/metadata/permissions/enums.xml:Duration)

  explicit PermissionIndicatorsTabData(content::WebContents* web_contents);

  ~PermissionIndicatorsTabData() override;

  // Returns `true` if the LHS verbose indicator is allowed in a tab.
  bool IsVerboseIndicatorAllowed(IndicatorsType type) const;
  // Mark that the LHS verbose indicator was already displayed.
  void SetVerboseIndicatorDisplayed(IndicatorsType type);

  // Record the times geolocation service started.
  void RecordStartGeolocationService();

  // content::WebContentsObserver:
  void WebContentsDestroyed() override;
  void PrimaryPageChanged(content::Page& page) override;
  void OnCapabilityTypesChanged(
      content::WebContents::CapabilityType connection_type,
      bool used) override;

 private:
  void ClearData();

  std::optional<base::TimeTicks> geolocation_last_usage_time_;

  std::optional<url::Origin> origin_;

  std::set<IndicatorsType> displayed_indicators_;
};

}  // namespace permissions

#endif  // COMPONENTS_PERMISSIONS_PERMISSION_INDICATORS_TAB_DATA_H_
