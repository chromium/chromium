// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_PERMISSION_INDICATORS_TAB_DATA_H_
#define COMPONENTS_PERMISSIONS_PERMISSION_INDICATORS_TAB_DATA_H_

#include "base/time/time.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "content/public/browser/web_contents_observer.h"

namespace permissions {

enum class RequestTypeForUma;

class PermissionIndicatorsTabData : public content::WebContentsObserver {
 public:
  // LHS indicators type. Currently only camera and mic are supported.
  enum class IndicatorsType { kMediaStream };

  explicit PermissionIndicatorsTabData(content::WebContents* web_contents);

  ~PermissionIndicatorsTabData() override;

  // Returns `true` if the LHS verbose indicator is allowed in a tab.
  bool IsVerboseIndicatorAllowed(IndicatorsType type) const;
  // Mark that the LHS verbose indicator was already displayed.
  void SetVerboseIndicatorDisplayed(IndicatorsType type);

  // Record when an activity starts.
  void RecordActivity(RequestTypeForUma request_type);

  // Capture the microphone and camera change.
  void OnMediaCaptureChanged(RequestTypeForUma request_type, bool used);

  // content::WebContentsObserver:
  void WebContentsDestroyed() override;
  void PrimaryPageChanged(content::Page& page) override;
  void OnCapabilityTypesChanged(
      content::WebContents::CapabilityType connection_type,
      bool used) override;

 private:
  void ClearData();

  std::map<RequestTypeForUma, std::optional<base::TimeTicks>> last_usage_time_;

  std::optional<url::Origin> origin_;

  std::set<IndicatorsType> displayed_indicators_;
};

}  // namespace permissions

#endif  // COMPONENTS_PERMISSIONS_PERMISSION_INDICATORS_TAB_DATA_H_
