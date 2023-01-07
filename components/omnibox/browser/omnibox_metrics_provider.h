// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_METRICS_PROVIDER_H_
#define COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_METRICS_PROVIDER_H_

#include "base/callback_list.h"
#include "components/metrics/metrics_provider.h"
#include "components/omnibox/browser/omnibox_event_global_tracker.h"
#include "third_party/metrics_proto/chrome_user_metrics_extension.pb.h"

struct OmniboxLog;

// OmniboxMetricsProvider is responsible for filling out the |omnibox_event|
// section of the UMA proto.
class OmniboxMetricsProvider : public metrics::MetricsProvider {
 public:
  OmniboxMetricsProvider();
  ~OmniboxMetricsProvider() override;
  OmniboxMetricsProvider(const OmniboxMetricsProvider&) = delete;
  OmniboxMetricsProvider& operator=(const OmniboxMetricsProvider&) = delete;

  // metrics::MetricsProvider:
  void OnRecordingEnabled() override;
  void OnRecordingDisabled() override;
  void ProvideCurrentSessionData(
      metrics::ChromeUserMetricsExtension* uma_proto) override;

 private:
  friend class OmniboxMetricsProviderTest;

  // Called when a URL is opened from the Omnibox.
  void OnURLOpenedFromOmnibox(OmniboxLog* log);

  // Records the input text, available choices, and selected entry when the
  // user uses the Omnibox to open a URL.
  void RecordOmniboxOpenedURL(const OmniboxLog& log);

  // Records the summary group of the selected Omnibox result. This is recorded
  // on the client side in addition to being generated on the server side, such
  // that it can be used by client side code that listens to UMA histograms.
  void RecordOmniboxOpenedURLClientSummarizedResultType(const OmniboxLog& log);

  // Subscription for receiving Omnibox event callbacks.
  base::CallbackListSubscription subscription_;

  // Saved cache of generated Omnibox event protos, to be copied into the UMA
  // proto when ProvideCurrentSessionData() is called.
  metrics::ChromeUserMetricsExtension omnibox_events_cache;
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_METRICS_PROVIDER_H_
