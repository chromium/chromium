// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/sync/password_data_type_controller.h"

#include <memory>
#include <string>
#include <utility>

#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/sync/base/sync_mode.h"
#include "components/sync/engine/configure_reason.h"
#include "components/sync/service/configure_context.h"
#include "components/sync/test/mock_data_type_controller_delegate.h"
#include "components/sync/test/mock_data_type_local_data_batch_uploader.h"
#include "google_apis/gaia/core_account_id.h"
#include "google_apis/gaia/gaia_id.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {
namespace {

class PasswordDataTypeControllerTest : public ::testing::Test {
 public:
  PasswordDataTypeControllerTest() {
    auto full_sync_delegate =
        std::make_unique<syncer::MockDataTypeControllerDelegate>();
    full_sync_delegate_ = full_sync_delegate.get();
    auto transport_only_delegate =
        std::make_unique<syncer::MockDataTypeControllerDelegate>();
    transport_only_delegate_ = transport_only_delegate.get();
    controller_ = std::make_unique<PasswordDataTypeController>(
        std::move(full_sync_delegate), std::move(transport_only_delegate),
        std::make_unique<syncer::MockDataTypeLocalDataBatchUploader>());
  }

  PasswordDataTypeController* controller() { return controller_.get(); }

  syncer::MockDataTypeControllerDelegate* full_sync_delegate() {
    return full_sync_delegate_;
  }

  syncer::MockDataTypeControllerDelegate* transport_only_delegate() {
    return transport_only_delegate_;
  }

 private:
  std::unique_ptr<PasswordDataTypeController> controller_;
  raw_ptr<syncer::MockDataTypeControllerDelegate> full_sync_delegate_;
  raw_ptr<syncer::MockDataTypeControllerDelegate> transport_only_delegate_;
};

#if BUILDFLAG(IS_ANDROID)
TEST_F(PasswordDataTypeControllerTest, OverrideFullSyncMode) {
  // `transport_only_delegate` should be used, despite syncer::SyncMode::kFull
  // being passed below.
  EXPECT_CALL(*full_sync_delegate(), OnSyncStarting).Times(0);
  EXPECT_CALL(*transport_only_delegate(), OnSyncStarting);

  syncer::ConfigureContext context;
  context.authenticated_gaia_id = GaiaId("gaia");
  context.cache_guid = "cache_guid";
  context.sync_mode = syncer::SyncMode::kFull;
  context.reason = syncer::ConfigureReason::kReconfiguration;
  context.configuration_start_time = base::Time::Now();
  controller()->LoadModels(context, base::DoNothing());
}
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace
}  // namespace password_manager
