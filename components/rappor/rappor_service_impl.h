// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_RAPPOR_RAPPOR_SERVICE_IMPL_H_
#define COMPONENTS_RAPPOR_RAPPOR_SERVICE_IMPL_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/threading/thread_checker.h"
#include "base/timer/timer.h"
#include "components/metrics/daily_event.h"
#include "components/rappor/public/rappor_parameters.h"
#include "components/rappor/public/rappor_service.h"
#include "components/rappor/public/sample.h"
#include "components/rappor/sampler.h"

class PrefRegistrySimple;
class PrefService;

namespace network {
class SharedURLLoaderFactory;
}

namespace rappor {

class LogUploaderInterface;
class RapporMetric;
class RapporReports;

// This class provides an interface for recording samples for rappor metrics,
// and periodically generates and uploads reports based on the collected data.
class RapporServiceImpl : public RapporService {
 public:
  // Constructs a RapporServiceImpl.
  // Calling code is responsible for ensuring that the lifetime of
  // |pref_service| is longer than the lifetime of RapporServiceImpl.
  // |is_incognito_callback| will be called to test if incognito mode is active.
  RapporServiceImpl(PrefService* pref_service,
                    base::RepeatingCallback<bool(void)> is_incognito_callback);
  virtual ~RapporServiceImpl();

  // Add an observer for collecting daily metrics.
  void AddDailyObserver(
      std::unique_ptr<metrics::DailyEvent::Observer> observer);

  // Initializes the rappor service, including loading the cohort and secret
  // preferences from disk.
  void Initialize(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

  // Updates the settings for metric recording and uploading.
  // The RapporServiceImpl must be initialized before this method is called.
  // If |may_record| is true, data will be recorded and periodic reports will
  // be generated and queued for upload.
  // If |may_upload| is true, reports will be uploaded from the queue.
  void Update(bool may_record, bool may_upload);

  // Constructs a Sample object for the caller to record fields in.
  std::unique_ptr<Sample> CreateSample(RapporType) override;

  // Records a Sample of rappor metric specified by |metric_name|.
  //
  // example:
  // std::unique_ptr<Sample> sample =
  // rappor_service->CreateSample(MY_METRIC_TYPE);
  // sample->SetStringField("Field1", "some string");
  // sample->SetFlagsValue("Field2", SOME|FLAGS);
  // rappor_service->RecordSample("MyMetric", std::move(sample));
  //
  // This will result in a report setting two metrics "MyMetric.Field1" and
  // "MyMetric.Field2", and they will both be generated from the same sample,
  // to allow for correlations to be computed.
  void RecordSample(const std::string& metric_name,
                    std::unique_ptr<Sample> sample) override;

  // Records a sample of the rappor metric specified by |metric_name|.
  // Creates and initializes the metric, if it doesn't yet exist.
  void RecordSampleString(const std::string& metric_name,
                          RapporType type,
                          const std::string& sample) override;

  // Registers the names of all of the preferences used by RapporServiceImpl in
  // the provided PrefRegistry. This should be called before calling Start().
  static void RegisterPrefs(PrefRegistrySimple* registry);

 protected:
  // Initializes the state of the RapporServiceImpl.
  void InitializeInternal(std::unique_ptr<LogUploaderInterface> uploader,
                          int32_t cohort,
                          const std::string& secret);

  // Cancels the next call to OnLogInterval.
  virtual void CancelNextLogRotation();

  // Schedules the next call to OnLogInterval.
  virtual void ScheduleNextLogRotation(base::TimeDelta interval);

  // Logs all of the collected metrics to the reports proto message and clears
  // the internal map. Exposed for tests. Returns true if any metrics were
  // recorded.
  bool ExportMetrics(RapporReports* reports);

 private:
  // Records a sample of the rappor metric specified by |parameters|.
  // Creates and initializes the metric, if it doesn't yet exist.
  // Exposed for tests.
  void RecordSampleInternal(const std::string& metric_name,
                            const RapporParameters& parameters,
                            const std::string& sample);

  // Checks if the service has been started successfully.
  bool IsInitialized() const;

  // Called whenever the logging interval elapses to generate a new log of
  // reports and pass it to the uploader.
  void OnLogInterval();

  // Check if recording of the metric is allowed, given it's parameters.
  // This will check that we are not in incognito mode, and that the
  // appropriate recording group is enabled.
  bool RecordingAllowed(const RapporParameters& parameters);

  // Finds a metric in the metrics_map_, creating it if it doesn't already
  // exist.
  RapporMetric* LookUpMetric(const std::string& metric_name,
                             const RapporParameters& parameters);

  // A weak pointer to the PrefService used to read and write preferences.
  PrefService* pref_service_;

  // A callback for testing if incognito mode is active;
  const base::RepeatingCallback<bool(void)> is_incognito_callback_;

  // Client-side secret used to generate fake bits.
  std::string secret_;

  // The cohort this client is assigned to. -1 is uninitialized.
  int32_t cohort_;

  // Timer which schedules calls to OnLogInterval().
  base::OneShotTimer log_rotation_timer_;

  // A daily event for collecting metrics once a day.
  metrics::DailyEvent daily_event_;

  // A private LogUploader instance for sending reports to the server.
  std::unique_ptr<LogUploaderInterface> uploader_;

  // Whether new data can be recorded.
  bool recording_enabled_;

  // We keep all registered metrics in a map, from name to metric.
  std::map<std::string, std::unique_ptr<RapporMetric>> metrics_map_;

  internal::Sampler sampler_;

  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(RapporServiceImpl);
};

}  // namespace rappor

#endif  // COMPONENTS_RAPPOR_RAPPOR_SERVICE_IMPL_H_
