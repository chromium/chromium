// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/secure_channel/connection_role.h"

namespace ash::secure_channel {

std::ostream& operator<<(std::ostream& stream, const ConnectionRole& role) {
  switch (role) {
    case ConnectionRole::kInitiatorRole:
      stream << "[initiator role]";
      break;
    case ConnectionRole::kListenerRole:
      stream << "[listener role]";
      break;
  }
  return stream;
}

}  // namespace ash::secure_channel
