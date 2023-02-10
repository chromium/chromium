// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/win/install_progress_observer.h"

namespace updater {

AppCompletionInfo::AppCompletionInfo()
    : completion_code(CompletionCodes::COMPLETION_CODE_SUCCESS) {}
AppCompletionInfo::AppCompletionInfo(const AppCompletionInfo&) = default;
AppCompletionInfo& AppCompletionInfo::operator=(const AppCompletionInfo&) =
    default;
AppCompletionInfo::~AppCompletionInfo() = default;

ObserverCompletionInfo::ObserverCompletionInfo() = default;
ObserverCompletionInfo::ObserverCompletionInfo(const ObserverCompletionInfo&) =
    default;
ObserverCompletionInfo& ObserverCompletionInfo::operator=(
    const ObserverCompletionInfo&) = default;
ObserverCompletionInfo::~ObserverCompletionInfo() = default;

}  // namespace updater
