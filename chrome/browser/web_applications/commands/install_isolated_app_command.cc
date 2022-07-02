// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "chrome/browser/web_applications/commands/install_isolated_app_command.h"

#include "base/callback_helpers.h"
#include "base/containers/flat_set.h"
#include "base/values.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"

namespace web_app {

InstallIsolatedAppCommand::InstallIsolatedAppCommand(base::StringPiece url)
    : WebAppCommand(WebAppCommandLock::CreateForAppAndWebContentsLock(
          base::flat_set<AppId>{"some random app id"})) {
  weak_this_ = weak_factory_.GetWeakPtr();
}

InstallIsolatedAppCommand::~InstallIsolatedAppCommand() = default;

void InstallIsolatedAppCommand::Start() {
  SignalCompletionAndSelfDestruct(CommandResult::kSuccess, base::DoNothing());
}

void InstallIsolatedAppCommand::OnSyncSourceRemoved() {
  SignalCompletionAndSelfDestruct(CommandResult::kSuccess, base::DoNothing());
}

void InstallIsolatedAppCommand::OnShutdown() {
  SignalCompletionAndSelfDestruct(CommandResult::kSuccess, base::DoNothing());
}

base::Value InstallIsolatedAppCommand::ToDebugValue() const {
  return base::Value{};
}
}  // namespace web_app
