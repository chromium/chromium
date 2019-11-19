// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/win/install_progress_observer.h"

namespace updater {

AppCompletionInfo::AppCompletionInfo()
    : completion_code(CompletionCodes::COMPLETION_CODE_SUCCESS),
      error_code(0),
      extra_code1(0),
      installer_result_code(0),
      is_canceled(false),
      is_noupdate(false) {}
AppCompletionInfo::AppCompletionInfo(const AppCompletionInfo&) = default;
AppCompletionInfo::~AppCompletionInfo() = default;

ObserverCompletionInfo::ObserverCompletionInfo() = default;
ObserverCompletionInfo::~ObserverCompletionInfo() = default;

}  // namespace updater
