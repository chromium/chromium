// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_METRICS_METRICS_RECORDER_H_
#define CHROMECAST_METRICS_METRICS_RECORDER_H_

#include <stdint.h>
#include <memory>
#include <string>

#include "base/containers/flat_map.h"
#include "base/time/time.h"
#include "chromecast/metrics/cast_event_builder.h"

namespace base {
class Value;
}  // namespace base

namespace net {
class IPAddressBytes;
}  // namespace net

namespace chromecast {

// TODO(b/176503058): Further remove the following 3 static functions.
void RecordEventWithLogPrefix(const std::string& action,
                              std::unique_ptr<CastEventBuilder> event_builder,
                              int verbose_log_level,
                              const std::string& log_prefix);

// Create a CastEventBuilder, caller takes ownership.
std::unique_ptr<CastEventBuilder> CreateCastEvent(const std::string& name);

// Records a cast event (and reports it to UMA server if CastReceiver sets an
// instance of MetricsRecorder).
void RecordCastEvent(const std::string& log_name,
                     std::unique_ptr<CastEventBuilder> event_builder,
                     int verbose_log_level);

// Records action (and reports it to UMA server if CastReceiver sets an instance
// of MetricsRecorder).
void RecordAction(const std::string& action, int verbose_log_level);

// Logs action but not reporting to UMA server.
void LogAction(const std::string& action, int verbose_log_level);

// Records time/count/enum histograms.
void RecordHistogramTime(const std::string& histogram_name,
                         int sample,
                         int min,
                         int max,
                         int num_buckets,
                         int verbose_log_level);
void RecordHistogramCount(const std::string& histogram_name,
                          int sample,
                          int min,
                          int max,
                          int num_buckets,
                          int verbose_log_level);
void RecordHistogramEnum(const std::string& histogram_name,
                         int value,
                         int boundary,
                         int verbose_log_level);

// Base class for metrics reporting. The caller of cast receiver should
// implement the subclass of this.
class MetricsRecorder {
 public:
  static void SetInstance(MetricsRecorder* recorder);
  static MetricsRecorder* GetInstance();

  class Observer {
   public:
    // Will be called right before metrics are uploaded.
    // Will be called on the UI thread. Must not post tasks, otherwise they will
    // not be guaranteed to run before metrics upload.
    virtual void OnPreUpload() = 0;

   protected:
    virtual ~Observer() = default;
  };

  virtual ~MetricsRecorder();

  // Creates cast event.
  virtual std::unique_ptr<CastEventBuilder> CreateEventBuilder(
      const std::string& name) = 0;

  // Adds/Removes active virtual connections. Infos of active connections whose
  // ids are referred by RecordEvent() will be sent at the same UMA log entry.
  // The sender's IPv4 or IPv6 address can be passed into |sender_ip|, which is
  // a network order byte array for the address. If the address is invalid
  // then empty address will be logged (0.0.0.0).
  virtual void AddActiveConnection(const std::string& transport_connection_id,
                                   const std::string& virtual_connection_id,
                                   const base::Value& sender_info,
                                   const net::IPAddressBytes& sender_ip) = 0;
  virtual void RemoveActiveConnection(const std::string& connection_id) = 0;

  // Records a cast event.
  virtual void RecordCastEvent(
      std::unique_ptr<CastEventBuilder> event_builder) = 0;

  // Records |sample| time in milliseconds.
  virtual void RecordHistogramTime(const std::string& name,
                                   int sample,
                                   int min,
                                   int max,
                                   int num_buckets) = 0;

  // Records |sample| count.
  virtual void RecordHistogramCount(const std::string& name,
                                    int sample,
                                    int min,
                                    int max,
                                    int num_buckets) = 0;

  // Records |sample| |count| number of times.
  virtual void RecordHistogramCountRepeated(const std::string& name,
                                            int sample,
                                            int min,
                                            int max,
                                            int num_buckets,
                                            int count) = 0;

  // Records |sample| enumeration value.
  virtual void RecordHistogramEnum(const std::string& name,
                                   int sample,
                                   int boundary) = 0;

  // Records |sample| in sparse histogram.
  virtual void RecordHistogramSparse(const std::string& name, int sample) = 0;

  // Measures the time elapsed between now and the next occurrence of
  // |event_name|. A metrics event named |measurement_name| will be logged when
  // the event occurs, along with the elapsed time in milliseconds. These
  // methods can be called from any thread.
  virtual void MeasureTimeUntilEvent(const std::string& event_name,
                                     const std::string& measurement_name) {}
  virtual void MeasureTimeUntilEvent(const std::string& event_name,
                                     const std::string& measurement_name,
                                     base::TimeTicks start_time) {}
  virtual void RecordTimelineEvent(const std::string& event_name) {}

  void AddObserver(Observer* o);
  void RemoveObserver(Observer* o);

 protected:
  MetricsRecorder();
  void NotifyOnPreUpload();

 private:
  struct ObserverList;

  std::unique_ptr<ObserverList> observer_list_;
};

// Records a cast event.
void RecordCastEvent(const std::string& event,
                     bool has_extra_value,
                     int64_t value,
                     MetricsRecorder* metrics_recorder);

// Records a cast event.
void RecordCastEventWithMetadata(
    const std::string& event,
    const base::flat_map<std::string, int64_t>& settings_map,
    MetricsRecorder* metrics_recorder);

}  // namespace chromecast

#endif  // CHROMECAST_METRICS_METRICS_RECORDER_H_
