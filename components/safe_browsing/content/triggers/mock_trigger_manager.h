// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CONTENT_TRIGGERS_MOCK_TRIGGER_MANAGER_H_
#define COMPONENTS_SAFE_BROWSING_CONTENT_TRIGGERS_MOCK_TRIGGER_MANAGER_H_

#include "base/macros.h"
#include "components/safe_browsing/content/triggers/trigger_manager.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace safe_browsing {

class MockTriggerManager : public TriggerManager {
 public:
  MockTriggerManager();
  ~MockTriggerManager() override;

  MOCK_METHOD6(
      StartCollectingThreatDetails,
      bool(TriggerType trigger_type,
           content::WebContents* web_contents,
           const security_interstitials::UnsafeResource& resource,
           scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
           history::HistoryService* history_service,
           const SBErrorOptions& error_display_options));
  MOCK_METHOD7(
      StartCollectingThreatDetailsWithReason,
      bool(TriggerType trigger_type,
           content::WebContents* web_contents,
           const security_interstitials::UnsafeResource& resource,
           scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
           history::HistoryService* history_service,
           const SBErrorOptions& error_display_options,
           TriggerManagerReason* out_reason));

  MOCK_METHOD6(FinishCollectingThreatDetails,
               bool(TriggerType trigger_type,
                    content::WebContents* web_contents,
                    const base::TimeDelta& delay,
                    bool did_proceed,
                    int num_visits,
                    const SBErrorOptions& error_display_options));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockTriggerManager);
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CONTENT_TRIGGERS_MOCK_TRIGGER_MANAGER_H_
