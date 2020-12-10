// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_PHONEHUB_USER_ACTION_RECORDER_IMPL_H_
#define CHROMEOS_COMPONENTS_PHONEHUB_USER_ACTION_RECORDER_IMPL_H_

#include <memory>

#include "base/gtest_prod_util.h"
#include "base/time/time.h"
#include "chromeos/components/phonehub/user_action_recorder.h"

namespace chromeos {
namespace phonehub {

// UserActionRecorder implementation which generates engagement metrics for
// Phone Hub.
class UserActionRecorderImpl : public UserActionRecorder {
 public:
  UserActionRecorderImpl();
  ~UserActionRecorderImpl() override;

 private:
  friend class UserActionRecorderImplTest;
  FRIEND_TEST_ALL_PREFIXES(UserActionRecorderImplTest, Enabled_RecordActions);

  // Types of user actions; numerical value should not be reused or reordered
  // since this enum is used in metrics.
  enum class UserAction {
    kUiOpened = 0,
    kTether = 1,
    kDnd = 2,
    kFindMyDevice = 3,
    kBrowserTab = 4,
    kNotificationDismissal = 5,
    kNotificationReply = 6,
    kMaxValue = kNotificationReply,
  };

  // UserActionRecorder:
  void RecordUiOpened() override;
  void RecordTetherConnectionAttempt() override;
  void RecordDndAttempt() override;
  void RecordFindMyDeviceAttempt() override;
  void RecordBrowserTabOpened() override;
  void RecordNotificationDismissAttempt() override;
  void RecordNotificationReplyAttempt() override;

  void HandleUserAction(UserAction action);
};

}  // namespace phonehub
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_PHONEHUB_USER_ACTION_RECORDER_IMPL_H_
