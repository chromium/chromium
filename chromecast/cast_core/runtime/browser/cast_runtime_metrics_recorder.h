// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_CAST_CORE_RUNTIME_BROWSER_CAST_RUNTIME_METRICS_RECORDER_H_
#define CHROMECAST_CAST_CORE_RUNTIME_BROWSER_CAST_RUNTIME_METRICS_RECORDER_H_

#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "chromecast/metrics/metrics_recorder.h"
#include "third_party/cast_core/public/src/proto/metrics/metrics_recorder.pb.h"

namespace chromecast {

// This class handles receiving histograms and CastEvents from the Cast OS
// MetricsRecorder interface.  Histograms are forwarded to the base UMA system
// and collected by CastRuntimeHistogramFlattener and CastEvents are store here
// to be sent later.
class CastRuntimeMetricsRecorder final : public MetricsRecorder {
 public:
  class EventBuilderFactory {
   public:
    virtual ~EventBuilderFactory();

    virtual std::unique_ptr<CastEventBuilder> CreateEventBuilder() = 0;
  };

  explicit CastRuntimeMetricsRecorder(
      EventBuilderFactory* event_builder_factory);
  ~CastRuntimeMetricsRecorder() override;

  static void RecordSimpleActionWithValue(const std::string& action,
                                          int64_t value);

  std::vector<cast::metrics::Event> TakeEvents();

 private:
  // MetricsRecorder implementation:
  std::unique_ptr<CastEventBuilder> CreateEventBuilder(
      const std::string& name) override;
  void AddActiveConnection(const std::string& transport_connection_id,
                           const std::string& virtual_connection_id,
                           const base::Value& sender_info,
                           const net::IPAddressBytes& sender_ip) override;
  void RemoveActiveConnection(const std::string& connection_id) override;
  void RecordCastEvent(
      std::unique_ptr<CastEventBuilder> event_builder) override;
  void RecordHistogramTime(const std::string& name,
                           int sample,
                           int min,
                           int max,
                           int num_buckets) override;
  void RecordHistogramCount(const std::string& name,
                            int sample,
                            int min,
                            int max,
                            int num_buckets) override;
  void RecordHistogramCountRepeated(const std::string& name,
                                    int sample,
                                    int min,
                                    int max,
                                    int num_buckets,
                                    int count) override;
  void RecordHistogramEnum(const std::string& name,
                           int sample,
                           int boundary) override;
  void RecordHistogramSparse(const std::string& name, int sample) override;

  void RecordCastEventOnSequence(
      std::unique_ptr<CastEventBuilder> event_builder);
  void CastMetricsHelperRecordActionCallback(const std::string& action);
  bool RecordJsonCastEvent(const std::string& event);

  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  std::vector<cast::metrics::Event> events_;

  EventBuilderFactory* const event_builder_factory_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<CastRuntimeMetricsRecorder> weak_factory_{this};
};

}  // namespace chromecast

#endif  // CHROMECAST_CAST_CORE_RUNTIME_BROWSER_CAST_RUNTIME_METRICS_RECORDER_H_
