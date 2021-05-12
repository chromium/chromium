// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GCM_DRIVER_GCM_STATS_RECORDER_ANDROID_H_
#define COMPONENTS_GCM_DRIVER_GCM_STATS_RECORDER_ANDROID_H_

#include <string>

#include "base/containers/circular_deque.h"
#include "base/macros.h"
#include "components/gcm_driver/gcm_activity.h"

namespace gcm {

enum class GCMDecryptionResult;

// Stats recorder for Android, used for recording stats and activities on the
// GCM Driver level for debugging purposes. Based on the GCMStatsRecorder, as
// defined in the GCM Engine, which does not exist on Android.
//
// Note that this class, different from the GCMStatsRecorder(Impl), is expected
// to be used on the UI thread.
class GCMStatsRecorderAndroid {
 public:
  // A delegate interface that allows the GCMStatsRecorderAndroid instance to
  // interact with its container.
  class Delegate {
   public:
    // Called when the GCMStatsRecorderAndroid is recording activities and a new
    // activity has just been recorded.
    virtual void OnActivityRecorded() = 0;
  };

  // A weak reference to |delegate| is stored, so it must outlive the recorder.
  explicit GCMStatsRecorderAndroid(Delegate* delegate);
  ~GCMStatsRecorderAndroid();

  // Clears the recorded activities.
  void Clear();

  // Collects all recorded activities into |*recorded_activities|.
  void CollectActivities(RecordedActivities* recorded_activities) const;

  // Records that a registration for |app_id| has been sent.
  void RecordRegistrationSent(const std::string& app_id);

  // Records that the registration sent for |app_id| has received a response.
  // |success| indicates whether the registration was successful.
  void RecordRegistrationResponse(const std::string& app_id, bool success);

  // Records that an unregistration for |app_id| has been sent.
  void RecordUnregistrationSent(const std::string& app_id);

  // Records that the unregistration sent for |app_id| has received a response.
  // |success| indicates whether the unregistration was successful.
  void RecordUnregistrationResponse(const std::string& app_id, bool success);

  // Records that a data message has been received for |app_id|.
  void RecordDataMessageReceived(const std::string& app_id,
                                 const std::string& from,
                                 int message_byte_size);

  // Records a message decryption failure caused by |result| for |app_id|.
  void RecordDecryptionFailure(const std::string& app_id,
                               GCMDecryptionResult result);

  bool is_recording() const { return is_recording_; }
  void set_is_recording(bool recording) { is_recording_ = recording; }

 private:
  void RecordRegistration(const std::string& app_id,
                          const std::string& event,
                          const std::string& details);

  // Delegate made available by the container. May be a nullptr.
  Delegate* delegate_;

  // Toggle determining whether the recorder is recording.
  bool is_recording_ = false;

  // Recorded registration activities (which includes unregistrations).
  base::circular_deque<RegistrationActivity> registration_activities_;

  // Recorded received message activities.
  base::circular_deque<ReceivingActivity> receiving_activities_;

  // Recorded message decryption failure activities.
  base::circular_deque<DecryptionFailureActivity>
      decryption_failure_activities_;

  DISALLOW_COPY_AND_ASSIGN(GCMStatsRecorderAndroid);
};

}  // namespace gcm

#endif  // COMPONENTS_GCM_DRIVER_GCM_STATS_RECORDER_ANDROID_H_
