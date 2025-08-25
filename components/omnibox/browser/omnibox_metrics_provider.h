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

// High-level groupings for Omnibox Suggestion ResultTypes, indicating whether
// the user Searched or Navigated from the omnibox.
// These values are written to logs. New enum values can be added, but existing
// enums must never be renumbered or deleted and reused.
// Must be kept in sync with ClientSummarizedResultType in
// //tools/metrics/histograms/enums.xml.
// LINT.IfChange(ClientSummarizedResultType)
enum class ClientSummarizedResultType : int {
  kUrl = 0,
  kSearch = 1,
  kApp = 2,
  kContact = 3,
  kOnDevice = 4,
  kUnknown = 5,
  kMaxValue = kUnknown
};
// LINT.ThenChange(//tools/metrics/histograms/enums.xml:ClientSummarizedResultType)

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

  static ClientSummarizedResultType GetClientSummarizedResultType(
      metrics::OmniboxEventProto::Suggestion::ResultType type);

 private:
  friend class OmniboxMetricsProviderTest;

  // Called when a URL is opened from the Omnibox.
  void OnURLOpenedFromOmnibox(OmniboxLog* log);

  // Records a set of metrics, e.g., the input text, available choices, and
  // selected entry, in omnibox_event.proto to log via
  // `metrics::MetricsProvider`.
  void RecordOmniboxEvent(const OmniboxLog& log);

  // Records a set of UMA histograms, e.g., the selected result group, and UKM
  // events from `log`. These client-side metrics are logged in addition to the
  // ones logged on the server via `metrics::MetricsProvider`.
  void RecordMetrics(const OmniboxLog& log);

  // Records zero-prefix suggestion precision/recall/usage metrics.
  void RecordZeroPrefixPrecisionRecallUsage(const OmniboxLog& log);

  // Records contextual search suggestion precision/recall/usage metrics.
  void RecordContextualSearchPrecisionRecallUsage(const OmniboxLog& log);

  // Subscription for receiving Omnibox event callbacks.
  base::CallbackListSubscription subscription_;

  // Saved cache of generated Omnibox event protos, to be copied into the UMA
  // proto when ProvideCurrentSessionData() is called.
  metrics::ChromeUserMetricsExtension omnibox_events_cache;
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_METRICS_PROVIDER_H_
