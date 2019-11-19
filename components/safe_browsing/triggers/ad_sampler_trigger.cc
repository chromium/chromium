// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/triggers/ad_sampler_trigger.h"

#include <string>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/rand_util.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/post_task.h"
#include "components/safe_browsing/features.h"
#include "components/safe_browsing/triggers/trigger_manager.h"
#include "components/safe_browsing/triggers/trigger_throttler.h"
#include "components/safe_browsing/triggers/trigger_util.h"
#include "components/security_interstitials/content/unsafe_resource.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"

namespace safe_browsing {

// Param name of the denominator for controlling sampling frequency.
const char kAdSamplerFrequencyDenominatorParam[] =
    "safe_browsing_ad_sampler_frequency_denominator";

// Default frequency denominator for the ad sampler.
const size_t kAdSamplerDefaultFrequency = 1000;

// A frequency denominator with this value indicates sampling is disabled.
const size_t kAdSamplerFrequencyDisabled = 0;

// Number of milliseconds to allow data collection to run before sending a
// report (since this trigger runs in the background).
const int64_t kAdSampleCollectionPeriodMilliseconds = 5000;

// Range of number of milliseconds to wait after a page finished loading before
// starting a report. Allows ads which load in the background to finish loading.
const int64_t kMaxAdSampleCollectionStartDelayMilliseconds = 5000;
const int64_t kMinAdSampleCollectionStartDelayMilliseconds = 500;

// Metric for tracking what the Ad Sampler trigger does on each navigation.
const char kAdSamplerTriggerActionMetricName[] =
    "SafeBrowsing.Triggers.AdSampler.Action";

namespace {

size_t GetSamplerFrequencyDenominator() {
  if (!base::FeatureList::IsEnabled(kAdSamplerTriggerFeature))
    return kAdSamplerDefaultFrequency;

  const std::string sampler_frequency_denominator =
      base::GetFieldTrialParamValueByFeature(
          kAdSamplerTriggerFeature, kAdSamplerFrequencyDenominatorParam);
  int result;
  if (!base::StringToInt(sampler_frequency_denominator, &result))
    return kAdSamplerDefaultFrequency;

  return result;
}

bool ShouldSampleAd(const size_t frequency_denominator) {
  return frequency_denominator != kAdSamplerFrequencyDisabled &&
         (base::RandUint64() % frequency_denominator) == 0;
}

}  // namespace

AdSamplerTrigger::AdSamplerTrigger(
    content::WebContents* web_contents,
    TriggerManager* trigger_manager,
    PrefService* prefs,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    history::HistoryService* history_service)
    : content::WebContentsObserver(web_contents),
      sampler_frequency_denominator_(GetSamplerFrequencyDenominator()),
      start_report_delay_ms_(
          base::RandInt(kMinAdSampleCollectionStartDelayMilliseconds,
                        kMaxAdSampleCollectionStartDelayMilliseconds)),
      finish_report_delay_ms_(kAdSampleCollectionPeriodMilliseconds),
      trigger_manager_(trigger_manager),
      prefs_(prefs),
      url_loader_factory_(url_loader_factory),
      history_service_(history_service),
      task_runner_(
          base::CreateSingleThreadTaskRunner({content::BrowserThread::UI})) {}

AdSamplerTrigger::~AdSamplerTrigger() {}

// static
void AdSamplerTrigger::CreateForWebContents(
    content::WebContents* web_contents,
    TriggerManager* trigger_manager,
    PrefService* prefs,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    history::HistoryService* history_service) {
  DCHECK(web_contents);
  if (!FromWebContents(web_contents)) {
    web_contents->SetUserData(UserDataKey(),
                              base::WrapUnique(new AdSamplerTrigger(
                                  web_contents, trigger_manager, prefs,
                                  url_loader_factory, history_service)));
  }
}

void AdSamplerTrigger::DidFinishLoad(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url) {
  UMA_HISTOGRAM_ENUMERATION(kAdSamplerTriggerActionMetricName, TRIGGER_CHECK,
                            MAX_ACTIONS);
  // We are using light-weight ad detection logic here so it's safe to do the
  // check on each navigation for the sake of metrics.
  if (!DetectGoogleAd(render_frame_host, validated_url)) {
    UMA_HISTOGRAM_ENUMERATION(kAdSamplerTriggerActionMetricName,
                              NO_SAMPLE_NO_AD, MAX_ACTIONS);
    return;
  }
  if (!ShouldSampleAd(sampler_frequency_denominator_)) {
    UMA_HISTOGRAM_ENUMERATION(kAdSamplerTriggerActionMetricName,
                              NO_SAMPLE_AD_SKIPPED_FOR_FREQUENCY, MAX_ACTIONS);
    return;
  }

  // Create a report after a short delay. The delay gives more time for ads to
  // finish loading in the background. This is best-effort.
  task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&AdSamplerTrigger::CreateAdSampleReport,
                     weak_ptr_factory_.GetWeakPtr()),
      base::TimeDelta::FromMilliseconds(start_report_delay_ms_));
}

void AdSamplerTrigger::CreateAdSampleReport() {
  SBErrorOptions error_options =
      TriggerManager::GetSBErrorDisplayOptions(*prefs_, web_contents());

  security_interstitials::UnsafeResource resource;
  resource.threat_type = SB_THREAT_TYPE_AD_SAMPLE;
  resource.url = web_contents()->GetURL();
  resource.web_contents_getter = resource.GetWebContentsGetter(
      web_contents()->GetMainFrame()->GetProcess()->GetID(),
      web_contents()->GetMainFrame()->GetRoutingID());

  if (!trigger_manager_->StartCollectingThreatDetails(
          TriggerType::AD_SAMPLE, web_contents(), resource, url_loader_factory_,
          history_service_, error_options)) {
    UMA_HISTOGRAM_ENUMERATION(kAdSamplerTriggerActionMetricName,
                              NO_SAMPLE_COULD_NOT_START_REPORT, MAX_ACTIONS);
    return;
  }

  // Call into TriggerManager to finish the reports after a short delay. Any
  // ads that are detected during this delay will be rejected by TriggerManager
  // because a report is already being collected, so we won't send multiple
  // reports for the same page.
  task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          IgnoreResult(&TriggerManager::FinishCollectingThreatDetails),
          base::Unretained(trigger_manager_), TriggerType::AD_SAMPLE,
          base::Unretained(web_contents()), base::TimeDelta(),
          /*did_proceed=*/false, /*num_visits=*/0, error_options),
      base::TimeDelta::FromMilliseconds(finish_report_delay_ms_));

  UMA_HISTOGRAM_ENUMERATION(kAdSamplerTriggerActionMetricName, AD_SAMPLED,
                            MAX_ACTIONS);
}

void AdSamplerTrigger::SetSamplerFrequencyForTest(size_t denominator) {
  sampler_frequency_denominator_ = denominator;
}

void AdSamplerTrigger::SetTaskRunnerForTest(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  task_runner_ = task_runner;
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(AdSamplerTrigger)

}  // namespace safe_browsing
