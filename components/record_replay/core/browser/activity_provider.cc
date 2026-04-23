// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/record_replay/core/browser/activity_provider.h"

#include "base/command_line.h"
#include "components/record_replay/core/browser/activity_hardcoded_provider.h"
#include "components/record_replay/core/browser/file_activity_provider.h"
#include "components/record_replay/core/common/record_replay_switches.h"

namespace record_replay {

// static
std::vector<std::unique_ptr<ActivityProvider>>
ActivityProvider::CreateProviders() {
  std::vector<std::unique_ptr<ActivityProvider>> providers;

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kActivityMetadataFile)) {
    base::FilePath file_path =
        command_line->GetSwitchValuePath(switches::kActivityMetadataFile);
    providers.push_back(std::make_unique<FileActivityProvider>(file_path));
  }

  providers.push_back(std::make_unique<ActivityHardcodedProvider>());
  return providers;
}

}  // namespace record_replay
