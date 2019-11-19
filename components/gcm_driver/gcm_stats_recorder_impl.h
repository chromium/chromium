// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GCM_DRIVER_GCM_STATS_RECORDER_IMPL_H_
#define COMPONENTS_GCM_DRIVER_GCM_STATS_RECORDER_IMPL_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/containers/circular_deque.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "components/gcm_driver/gcm_activity.h"
#include "google_apis/gcm/engine/connection_factory.h"
#include "google_apis/gcm/engine/mcs_client.h"
#include "google_apis/gcm/engine/registration_request.h"
#include "google_apis/gcm/engine/unregistration_request.h"
#include "google_apis/gcm/monitoring/gcm_stats_recorder.h"

namespace gcm {

enum class GCMDecryptionResult;

// Records GCM internal stats and activities for debugging purpose. Recording
// can be turned on/off by calling set_is_recording(...) function. It is turned
// off by default.
// This class is not thread safe. It is meant to be owned by a gcm client
// instance.
class GCMStatsRecorderImpl : public GCMStatsRecorder {
 public:
  GCMStatsRecorderImpl();
  ~GCMStatsRecorderImpl() override;

  // Set a delegate to receive callback from the recorder.
  void SetDelegate(Delegate* delegate);

  // Clear all recorded activities.
  void Clear();

  // Records a message decryption failure caused by |result| for |app_id|.
  void RecordDecryptionFailure(const std::string& app_id,
                               GCMDecryptionResult result);

  // GCMStatsRecorder implementation:
  void RecordCheckinInitiated(uint64_t android_id) override;
  void RecordCheckinDelayedDueToBackoff(int64_t delay_msec) override;
  void RecordCheckinSuccess() override;
  void RecordCheckinFailure(const std::string& status,
                            bool will_retry) override;
  void RecordConnectionInitiated(const std::string& host) override;
  void RecordConnectionDelayedDueToBackoff(int64_t delay_msec) override;
  void RecordConnectionSuccess() override;
  void RecordConnectionFailure(int network_error) override;
  void RecordConnectionResetSignaled(
      ConnectionFactory::ConnectionResetReason reason) override;
  void RecordRegistrationSent(const std::string& app_id,
                              const std::string& source) override;
  void RecordRegistrationResponse(const std::string& app_id,
                                  const std::string& source,
                                  RegistrationRequest::Status status) override;
  void RecordRegistrationRetryDelayed(const std::string& app_id,
                                      const std::string& source,
                                      int64_t delay_msec,
                                      int retries_left) override;
  void RecordUnregistrationSent(const std::string& app_id,
                                const std::string& source) override;
  void RecordUnregistrationResponse(
      const std::string& app_id,
      const std::string& source,
      UnregistrationRequest::Status status) override;
  void RecordUnregistrationRetryDelayed(const std::string& app_id,
                                        const std::string& source,
                                        int64_t delay_msec,
                                        int retries_left) override;
  void RecordDataMessageReceived(const std::string& app_id,
                                 const std::string& from,
                                 int message_byte_size,
                                 ReceivedMessageType message_type) override;
  void RecordDataSentToWire(const std::string& app_id,
                            const std::string& receiver_id,
                            const std::string& message_id,
                            int queued) override;
  void RecordNotifySendStatus(const std::string& app_id,
                              const std::string& receiver_id,
                              const std::string& message_id,
                              MCSClient::MessageSendStatus status,
                              int byte_size,
                              int ttl) override;
  void RecordIncomingSendError(const std::string& app_id,
                               const std::string& receiver_id,
                               const std::string& message_id) override;

  // Collect all recorded activities into |*recorded_activities|.
  void CollectActivities(RecordedActivities* recorded_activities) const;

  bool is_recording() const { return is_recording_; }
  void set_is_recording(bool recording) { is_recording_ = recording; }

  const base::circular_deque<CheckinActivity>& checkin_activities() const {
    return checkin_activities_;
  }
  const base::circular_deque<ConnectionActivity>& connection_activities()
      const {
    return connection_activities_;
  }
  const base::circular_deque<RegistrationActivity>& registration_activities()
      const {
    return registration_activities_;
  }
  const base::circular_deque<ReceivingActivity>& receiving_activities() const {
    return receiving_activities_;
  }
  const base::circular_deque<SendingActivity>& sending_activities() const {
    return sending_activities_;
  }
  const base::circular_deque<DecryptionFailureActivity>&
  decryption_failure_activities() const {
    return decryption_failure_activities_;
  }

 protected:
  // Notify the recorder delegate, if it exists, that an activity has been
  // recorded.
  void NotifyActivityRecorded();

  void RecordCheckin(const std::string& event,
                     const std::string& details);

  void RecordConnection(const std::string& event,
                        const std::string& details);

  void RecordRegistration(const std::string& app_id,
                          const std::string& source,
                          const std::string& event,
                          const std::string& details);

  void RecordReceiving(const std::string& app_id,
                       const std::string& from,
                       int message_byte_size,
                       const std::string& event,
                       const std::string& details);

  void RecordSending(const std::string& app_id,
                     const std::string& receiver_id,
                     const std::string& message_id,
                     const std::string& event,
                     const std::string& details);

  bool is_recording_;
  Delegate* delegate_;

  base::circular_deque<CheckinActivity> checkin_activities_;
  base::circular_deque<ConnectionActivity> connection_activities_;
  base::circular_deque<RegistrationActivity> registration_activities_;
  base::circular_deque<ReceivingActivity> receiving_activities_;
  base::circular_deque<SendingActivity> sending_activities_;
  base::circular_deque<DecryptionFailureActivity>
      decryption_failure_activities_;

  base::TimeTicks last_connection_initiation_time_;
  base::TimeTicks last_received_data_message_burst_start_time_;
  int64_t received_data_message_burst_size_;

  DISALLOW_COPY_AND_ASSIGN(GCMStatsRecorderImpl);
};

}  // namespace gcm

#endif  // COMPONENTS_GCM_DRIVER_GCM_STATS_RECORDER_IMPL_H_
