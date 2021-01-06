// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_ACTIVITY_IMPL_H_
#define CHROME_UPDATER_ACTIVITY_IMPL_H_

#include <string>

namespace updater {

bool GetActiveBit(const std::string& id, bool is_machine_);

void ClearActiveBit(const std::string& id, bool is_machine_);

}  // namespace updater

#endif  // CHROME_UPDATER_ACTIVITY_IMPL_H_
