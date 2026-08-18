// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FCM_FCM_MESSAGE_H_
#define COMPONENTS_FCM_FCM_MESSAGE_H_

#include <map>
#include <string>

namespace fcm {

// Represents a push message received via FCM.
struct FcmMessage {
  FcmMessage();
  FcmMessage(const FcmMessage& other);
  FcmMessage& operator=(const FcmMessage& other);
  FcmMessage(FcmMessage&& other);
  FcmMessage& operator=(FcmMessage&& other);
  ~FcmMessage();

  // Custom key-value data payload from the message.
  std::map<std::string, std::string> data;

  // Raw binary payload, if present.
  std::string raw_data;

  // Unique identifier for the message, if provided by FCM.
  std::string message_id;
};

}  // namespace fcm

#endif  // COMPONENTS_FCM_FCM_MESSAGE_H_
