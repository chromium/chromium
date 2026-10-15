// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_tab_context/tab_context_container_data_type_controller.h"

#include <memory>

#include "base/test/task_environment.h"
#include "components/signin/public/identity_manager/account_managed_status_finder.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/passphrase_enums.h"
#include "components/sync/service/data_type_controller.h"
#include "components/sync/test/fake_data_type_controller_delegate.h"
#include "components/sync/test/test_sync_service.h"
#include "components/sync/test/test_sync_user_settings.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sync_tab_context {
namespace {

class TabContextContainerDataTypeControllerTest : public ::testing::Test {
 protected:
  TabContextContainerDataTypeControllerTest()
      : controller_(std::make_unique<syncer::FakeDataTypeControllerDelegate>(
                        syncer::ENCRYPTED_TAB_CONTEXT_CONTAINER),
                    std::make_unique<syncer::FakeDataTypeControllerDelegate>(
                        syncer::ENCRYPTED_TAB_CONTEXT_CONTAINER),
                    &sync_service_) {}

  base::test::TaskEnvironment task_environment_;
  syncer::TestSyncService sync_service_;
  TabContextContainerDataTypeController controller_;
};

TEST_F(TabContextContainerDataTypeControllerTest,
       ShouldEnableWhenPassphraseIsTrustedVault) {
  sync_service_.GetUserSettings()->SetPassphraseType(
      syncer::PassphraseType::kTrustedVaultPassphrase);

  EXPECT_EQ(controller_.GetPreconditionState(
                syncer::DataTypeController::PreconditionContext(
                    signin::AccountManagedStatusFinderOutcome::kConsumerGmail)),
            syncer::DataTypeController::PreconditionState::kPreconditionsMet);
}

TEST_F(TabContextContainerDataTypeControllerTest,
       ShouldDisableWhenPassphraseIsNotTrustedVault) {
  sync_service_.GetUserSettings()->SetPassphraseType(
      syncer::PassphraseType::kKeystorePassphrase);

  EXPECT_EQ(
      controller_.GetPreconditionState(
          syncer::DataTypeController::PreconditionContext(
              signin::AccountManagedStatusFinderOutcome::kConsumerGmail)),
      syncer::DataTypeController::PreconditionState::kMustStopAndClearData);

  sync_service_.GetUserSettings()->SetPassphraseType(
      syncer::PassphraseType::kCustomPassphrase);

  EXPECT_EQ(
      controller_.GetPreconditionState(
          syncer::DataTypeController::PreconditionContext(
              signin::AccountManagedStatusFinderOutcome::kConsumerGmail)),
      syncer::DataTypeController::PreconditionState::kMustStopAndClearData);
}

}  // namespace
}  // namespace sync_tab_context
