// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_TRIGGERS_AD_SAMPLER_TRIGGER_H_
#define COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_TRIGGERS_AD_SAMPLER_TRIGGER_H_

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

class PrefService;

namespace history {
class HistoryService;
}

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace safe_browsing {
class ReferrerChainProvider;
class TriggerManager;

// Param name of the denominator for controlling sampling frequency.
extern const char kAdSamplerFrequencyDenominatorParam[];

// Default frequency for the ad sampler, if not configured in Finch.
extern const size_t kAdSamplerDefaultFrequency;

// A frequency denominator with this value indicates sampling is disabled.
extern const size_t kAdSamplerFrequencyDisabled;

// Metric for tracking what the Ad Sampler trigger does on each navigation.
extern const char kAdSamplerTriggerActionMetricName[];

// Actions performed by this trigger. These values are written to logs. New enum
// values can be added, but existing enums must never be renumbered or deleted
// and reused.
enum AdSamplerTriggerAction {
  // An event occurred that caused the trigger to perform its checks.
  TRIGGER_CHECK = 0,
  // An ad was detected and a sample was collected.
  AD_SAMPLED = 1,
  // An ad was detected but no sample was taken to honour sampling frequency.
  NO_SAMPLE_AD_SKIPPED_FOR_FREQUENCY = 2,
  // No ad was detected.
  NO_SAMPLE_NO_AD = 3,
  // An ad was detected and could have been sampled, but the trigger manager
  // rejected the report (eg: because a report was already in progress).
  NO_SAMPLE_COULD_NOT_START_REPORT = 4,
  // New actions must be added before MAX_ACTIONS.
  MAX_ACTIONS
};

// This class periodically checks for Google ads on the page and may decide to
// send a report to Google with the ad's structure for further analysis.
class AdSamplerTrigger : public content::WebContentsObserver,
                         public content::WebContentsUserData<AdSamplerTrigger> {
 public:
  AdSamplerTrigger(const AdSamplerTrigger&) = delete;
  AdSamplerTrigger& operator=(const AdSamplerTrigger&) = delete;

  ~AdSamplerTrigger() override;

  // content::WebContentsObserver implementation.
  void DidFinishLoad(content::RenderFrameHost* render_frame_host,
                     const GURL& validated_url) override;

  static size_t GetSamplerFrequencyDenominatorForTest();

 private:
  friend class AdSamplerTriggerTest;
  friend class content::WebContentsUserData<AdSamplerTrigger>;

  AdSamplerTrigger(
      content::WebContents* web_contents,
      TriggerManager* trigger_manager,
      PrefService* prefs,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      history::HistoryService* history_service,
      ReferrerChainProvider* referrer_chain_provider);

  // Called to create an ad sample report.
  void CreateAdSampleReport();

  // Sets |sampler_frequency_denominator_| for tests.
  void SetSamplerFrequencyForTest(size_t denominator);

  // Sets a task runner to use for tests.
  void SetTaskRunnerForTest(
      scoped_refptr<base::SequencedTaskRunner> task_runner);

  // Ad samples will be collected with frequency
  // 1/|sampler_frequency_denominator_|
  size_t sampler_frequency_denominator_;

  // The delay (in milliseconds) to wait before starting a report. Can be
  // ovewritten for tests.
  int64_t start_report_delay_ms_;

  // The delay (in milliseconds) to wait before finishing a report. Can be
  // overwritten for tests.
  int64_t finish_report_delay_ms_;

  // TriggerManager gets called if this trigger detects an ad and wants to
  // collect some data about it. Not owned.
  raw_ptr<TriggerManager> trigger_manager_;

  raw_ptr<PrefService> prefs_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  raw_ptr<history::HistoryService> history_service_;
  raw_ptr<ReferrerChainProvider> referrer_chain_provider_;

  // Task runner for posting delayed tasks. Normally set to the runner for the
  // UI thread, but can be overwritten for tests.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  base::WeakPtrFactory<AdSamplerTrigger> weak_ptr_factory_{this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_TRIGGERS_AD_SAMPLER_TRIGGER_H_
