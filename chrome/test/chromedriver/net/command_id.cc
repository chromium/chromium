// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/net/command_id.h"

bool CommandId::IsChromeDriverCommandId(int command_id) {
  return command_id > 0;
}

bool CommandId::IsClientCommandId(int command_id) {
  return command_id < 0;
}
