// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_BROWSER_REPORTING_BROWSER_LAUNCH_BROWSER_LAUNCH_EVENT_UPLOADER_H_
#define COMPONENTS_ENTERPRISE_BROWSER_REPORTING_BROWSER_LAUNCH_BROWSER_LAUNCH_EVENT_UPLOADER_H_

#include "base/functional/callback_forward.h"
#include "components/enterprise/common/proto/synced/browser_events.pb.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"

namespace enterprise_reporting {

// Interface for capturing the upload of browser launch events.
class BrowserLaunchEventUploader {
 public:
  virtual ~BrowserLaunchEventUploader() = default;

  // Uploads the captured browser launch event.
  virtual void UploadEvent(
      const ::chrome::cros::reporting::proto::BrowserLaunchEvent& event,
      base::OnceCallback<void(policy::CloudPolicyClient::Result)>
          upload_callback) = 0;
};

}  // namespace enterprise_reporting

#endif  // COMPONENTS_ENTERPRISE_BROWSER_REPORTING_BROWSER_LAUNCH_BROWSER_LAUNCH_EVENT_UPLOADER_H_
