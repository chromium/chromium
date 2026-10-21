// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FCM_ENGINE_CHECKIN_INFO_H_
#define COMPONENTS_FCM_ENGINE_CHECKIN_INFO_H_

#include <stdint.h>

namespace fcm {

// The check-in info for the device, including android_id and secret (security
// token).
class CheckinInfo {
 public:
  CheckinInfo();
  CheckinInfo(uint64_t android_id, uint64_t secret);
  ~CheckinInfo();

  CheckinInfo(const CheckinInfo&);
  CheckinInfo& operator=(const CheckinInfo&);
  CheckinInfo(CheckinInfo&&);
  CheckinInfo& operator=(CheckinInfo&&);

  // True if both android_id and secret are non-zero.
  bool IsValid() const { return android_id_ != 0 && secret_ != 0; }

  // Clears all check-in info back to default state.
  void Reset();

  uint64_t android_id() const { return android_id_; }
  void set_android_id(uint64_t android_id) { android_id_ = android_id; }

  uint64_t secret() const { return secret_; }
  void set_secret(uint64_t secret) { secret_ = secret; }

 private:
  // Android ID of the device as assigned by the server.
  uint64_t android_id_ = 0;

  // Security token of the device as assigned by the server.
  uint64_t secret_ = 0;
};

}  // namespace fcm

#endif  // COMPONENTS_FCM_ENGINE_CHECKIN_INFO_H_
