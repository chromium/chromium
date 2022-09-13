// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GCM_DRIVER_GCM_ACTIVITY_H_
#define COMPONENTS_GCM_DRIVER_GCM_ACTIVITY_H_

#include <string>
#include <vector>

#include "base/time/time.h"

namespace gcm {

// Contains data that are common to all activity kinds below.
struct Activity {
  Activity();
  virtual ~Activity();

  base::Time time;
  std::string event;    // A short description of the event.
  std::string details;  // Any additional detail about the event.
};

// Contains relevant data of a connection activity.
struct ConnectionActivity : Activity {
  ConnectionActivity();
  ~ConnectionActivity() override;
};

// Contains relevant data of a check-in activity.
struct CheckinActivity : Activity {
  CheckinActivity();
  ~CheckinActivity() override;
};

// Contains relevant data of a registration/unregistration step.
struct RegistrationActivity : Activity {
  RegistrationActivity();
  ~RegistrationActivity() override;

  std::string app_id;
  // For GCM, comma separated sender ids. For Instance ID, authorized entity.
  std::string source;
};

// Contains relevant data of a message receiving event.
struct ReceivingActivity : Activity {
  ReceivingActivity();
  ~ReceivingActivity() override;

  std::string app_id;
  std::string from;
  int message_byte_size;
};

// Contains relevant data of a send-message step.
struct SendingActivity : Activity {
  SendingActivity();
  ~SendingActivity() override;

  std::string app_id;
  std::string receiver_id;
  std::string message_id;
};

// Contains relevant data of a message decryption failure.
struct DecryptionFailureActivity : Activity {
  DecryptionFailureActivity();
  ~DecryptionFailureActivity() override;

  std::string app_id;
};

struct RecordedActivities {
  RecordedActivities();
  RecordedActivities(const RecordedActivities& other);
  virtual ~RecordedActivities();

  std::vector<CheckinActivity> checkin_activities;
  std::vector<ConnectionActivity> connection_activities;
  std::vector<RegistrationActivity> registration_activities;
  std::vector<ReceivingActivity> receiving_activities;
  std::vector<SendingActivity> sending_activities;
  std::vector<DecryptionFailureActivity> decryption_failure_activities;
};

}  // namespace gcm

#endif  // COMPONENTS_GCM_DRIVER_GCM_ACTIVITY_H_
