// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_TRIGGERS_MOCK_TRIGGER_MANAGER_H_
#define COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_TRIGGERS_MOCK_TRIGGER_MANAGER_H_

#include "components/safe_browsing/content/browser/triggers/trigger_manager.h"
#include "components/safe_browsing/content/browser/web_contents_key.h"
#include "components/safe_browsing/core/browser/referrer_chain_provider.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace safe_browsing {

class MockTriggerManager : public TriggerManager {
 public:
  MockTriggerManager();

  MockTriggerManager(const MockTriggerManager&) = delete;
  MockTriggerManager& operator=(const MockTriggerManager&) = delete;

  ~MockTriggerManager() override;

  MOCK_METHOD7(
      StartCollectingThreatDetails,
      bool(TriggerType trigger_type,
           content::WebContents* web_contents,
           const security_interstitials::UnsafeResource& resource,
           scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
           history::HistoryService* history_service,
           ReferrerChainProvider* referrer_chain_provider,
           const SBErrorOptions& error_display_options));
  MOCK_METHOD8(
      StartCollectingThreatDetailsWithReason,
      bool(TriggerType trigger_type,
           content::WebContents* web_contents,
           const security_interstitials::UnsafeResource& resource,
           scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
           history::HistoryService* history_service,
           ReferrerChainProvider* referrer_chain_provider,
           const SBErrorOptions& error_display_options,
           TriggerManagerReason* out_reason));

  MOCK_METHOD8(FinishCollectingThreatDetails,
               FinishCollectingThreatDetailsResult(
                   TriggerType trigger_type,
                   WebContentsKey web_contents_key,
                   const base::TimeDelta& delay,
                   bool did_proceed,
                   int num_visits,
                   const SBErrorOptions& error_display_options,
                   std::optional<int64_t> warning_shown_ts,
                   bool is_hats_candidate));
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_TRIGGERS_MOCK_TRIGGER_MANAGER_H_
