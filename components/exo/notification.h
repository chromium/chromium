// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_NOTIFICATION_H_
#define COMPONENTS_EXO_NOTIFICATION_H_

#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback.h"

namespace exo {

// Handles notification shown in message center.
class Notification {
 public:
  Notification(const std::string& title,
               const std::string& message,
               const std::string& display_source,
               const std::string& notification_id,
               const std::string& notifier_id,
               const std::vector<std::string>& buttons,
               const base::RepeatingCallback<void(bool)>& close_callback,
               const base::RepeatingCallback<void(const std::optional<int>&)>&
                   click_callback);

  Notification(const Notification&) = delete;
  Notification& operator=(const Notification&) = delete;

  // Closes this notification.
  void Close();

 private:
  const std::string notification_id_;
};

}  // namespace exo

#endif  // COMPONENTS_EXO_NOTIFICATION_H_
