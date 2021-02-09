// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_STRUCTURED_STRUCTURED_METRICS_PROVIDER_H_
#define COMPONENTS_METRICS_STRUCTURED_STRUCTURED_METRICS_PROVIDER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "components/metrics/metrics_provider.h"
#include "components/metrics/structured/event_base.h"
#include "components/metrics/structured/key_data.h"
#include "components/metrics/structured/recorder.h"

namespace metrics {
namespace structured {

class EventsProto;

// StructuredMetricsProvider is responsible for filling out the
// |structured_metrics_event| section of the UMA proto. This class should not be
// instantiated except by the ChromeMetricsServiceClient. This class is not
// thread safe and should only be called on the browser UI sequence, because
// calls from the metrics service come on the UI sequence.
//
// Initialization of the StructuredMetricsProvider must wait until a profile is
// added, because state is stored within the profile directory. Initialization
// happens in several steps:
//
// 1. A StructuredMetricsProvider instance is constructed and owned by the
//    MetricsService. It registers itself as an observer of
//    metrics::structured::Recorder.
//
// 2. When a profile is added that is eligible for recording,
//    ChromeMetricsServiceClient calls Recorder::ProfileAdded, which notifies
//    this class.
//
// 3. This class then begins initialization by asynchronously reading keys and
//    unsent logs from the cryptohome.
//
// 4. If the read succeeds, initialization is complete and this class starts
//    accepting events to record.
//
// After initialization, this class accepts events to record from
// StructuredMetricsProvider::OnRecord via Recorder::Record via
// EventBase::Record. These events are not uploaded immediately, and are cached
// in ready-to-upload form.
//
// On a call to ProvideCurrentSessionData, the cache of unsent logs is added to
// a ChromeUserMetricsExtension for upload, and is then cleared.
class StructuredMetricsProvider : public metrics::MetricsProvider,
                                  public Recorder::Observer {
 public:
  StructuredMetricsProvider();
  ~StructuredMetricsProvider() override;
  StructuredMetricsProvider(const StructuredMetricsProvider&) = delete;
  StructuredMetricsProvider& operator=(const StructuredMetricsProvider&) =
      delete;

 private:
  friend class Recorder;
  friend class StructuredMetricsProviderTest;

  // State machine for step 4 of initialization. These are stored in two files
  // that are asynchronously read from disk at startup. When both files have
  // been read, the provider has been initialized.
  enum class InitState {
    kUninitialized = 1,
    // Set after we observe the recorder, which happens on construction.
    kProfileAdded = 2,
    // Set after the key file is read from disk.
    kKeysInitialized = 3,
    // Set after the event file is read from disk.
    kEventsInitialized = 4,
    // Set after both the key and event files are read from disk.
    kInitialized = 5,
  };

  void OnKeyDataInitialized();
  void OnRead(ReadStatus status);
  void OnWrite(WriteStatus status);

  // Recorder::Observer:
  void OnProfileAdded(const base::FilePath& profile_path) override;
  void OnRecord(const EventBase& event) override;

  // metrics::MetricsProvider:
  void OnRecordingEnabled() override;
  void OnRecordingDisabled() override;
  void ProvideCurrentSessionData(
      metrics::ChromeUserMetricsExtension* uma_proto) override;
  bool HasIndependentMetrics() override;
  void ProvideIndependentMetrics(base::OnceCallback<void(bool)> done_callback,
                                 ChromeUserMetricsExtension* uma_proto,
                                 base::HistogramSnapshotManager*) override;

  void WriteNowForTest();

  // Beyond this number of logging events between successive calls to
  // ProvideCurrentSessionData, we stop recording events.
  static int kMaxEventsPerUpload;

  // The directory used to store unsent logs and keys. Relative to the user's
  // cryptohome.
  static char kStorageDirectory[];

  // Whether the metrics provider has completed initialization. Initialization
  // occurs across OnProfileAdded and OnInitializationCompleted. No incoming
  // events are recorded until initialization has succeeded.
  //
  // Execution is:
  //  - A profile is added.
  //  - OnProfileAdded is called, which constructs |storage_| and
  //    asynchronously reads events and keys.
  //  - OnInitializationCompleted is called once reading from disk is complete,
  //    which sets |initialized_| to true.
  //
  // The metrics provider does not handle multiprofile: initialization happens
  // only once, for the first-logged-in account aka. primary user.
  //
  InitState init_state_ = InitState::kUninitialized;

  // Tracks the recording state signalled to the metrics provider by
  // OnRecordingEnabled and OnRecordingDisabled.
  bool recording_enabled_ = false;

  // Set by OnRecordingDisabled if |events_| hasn't been initialized yet to
  // indicate events should be deleted from disk when |events_| is initialized.
  // See OnRecordingDisabled for more information.
  bool wipe_events_on_init_ = false;

  // On-device storage within the user's cryptohome for unsent logs.
  std::unique_ptr<PersistentProto<EventsProto>> events_;

  // Storage for all event's keys, and hashing logic for values. This stores
  // keys on disk.
  std::unique_ptr<KeyData> key_data_;

  base::WeakPtrFactory<StructuredMetricsProvider> weak_factory_{this};
};

}  // namespace structured
}  // namespace metrics

#endif  // COMPONENTS_METRICS_STRUCTURED_STRUCTURED_METRICS_PROVIDER_H_
