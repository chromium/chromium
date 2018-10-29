// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/discover/modules/discover_module_sync_files.h"

#include "chrome/browser/ui/webui/chromeos/login/discover/discover_handler.h"
#include "chrome/grit/generated_resources.h"
#include "components/login/localized_values_builder.h"
#include "ui/chromeos/devicetype_utils.h"

namespace chromeos {

namespace {

class DiscoverModuleSyncFilesHandler : public DiscoverHandler {
 public:
  DiscoverModuleSyncFilesHandler();
  ~DiscoverModuleSyncFilesHandler() override = default;

 private:
  // BaseWebUIHandler: implementation
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;
  void Initialize() override;
  void RegisterMessages() override;

  DISALLOW_COPY_AND_ASSIGN(DiscoverModuleSyncFilesHandler);
};

DiscoverModuleSyncFilesHandler::DiscoverModuleSyncFilesHandler()
    : DiscoverHandler(DiscoverModuleSyncFiles::kModuleName) {}

void DiscoverModuleSyncFilesHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  builder->Add("discoverSyncMyFiles", IDS_DISCOVER_SYNC_MY_FILES);
}

void DiscoverModuleSyncFilesHandler::Initialize() {}

void DiscoverModuleSyncFilesHandler::RegisterMessages() {}

}  // anonymous namespace

/* ***************************************************************** */
/* Discover SyncFiles module implementation below.                   */

const char DiscoverModuleSyncFiles::kModuleName[] = "sync-files";

DiscoverModuleSyncFiles::DiscoverModuleSyncFiles() = default;

DiscoverModuleSyncFiles::~DiscoverModuleSyncFiles() = default;

bool DiscoverModuleSyncFiles::IsCompleted() const {
  return false;
}

std::unique_ptr<DiscoverHandler> DiscoverModuleSyncFiles::CreateWebUIHandler() {
  return std::make_unique<DiscoverModuleSyncFilesHandler>();
}

}  // namespace chromeos
