// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_tab_context/tab_context_data_type_controller.h"

#include <memory>
#include <utility>

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

class TabContextDataTypeControllerTest : public ::testing::Test {
 protected:
  TabContextDataTypeControllerTest() {
    controller_ = std::make_unique<TabContextDataTypeController>(
        syncer::ENCRYPTED_TAB_CONTEXT_CONTAINER,
        std::make_unique<syncer::FakeDataTypeControllerDelegate>(
            syncer::ENCRYPTED_TAB_CONTEXT_CONTAINER),
        std::make_unique<syncer::FakeDataTypeControllerDelegate>(
            syncer::ENCRYPTED_TAB_CONTEXT_CONTAINER),
        &sync_service_);
  }

  base::test::TaskEnvironment task_environment_;
  syncer::TestSyncService sync_service_;
  std::unique_ptr<TabContextDataTypeController> controller_;
};

TEST_F(TabContextDataTypeControllerTest,
       ShouldEnableForConsumerAccountWithTrustedVaultPassphrase) {
  sync_service_.GetUserSettings()->SetPassphraseType(
      syncer::PassphraseType::kTrustedVaultPassphrase);

  syncer::DataTypeController::PreconditionContext context(
      signin::AccountManagedStatusFinderOutcome::kConsumerGmail);

  EXPECT_EQ(controller_->GetPreconditionState(context),
            syncer::DataTypeController::PreconditionState::kPreconditionsMet);
}

TEST_F(TabContextDataTypeControllerTest, ShouldDisableForEnterpriseAccount) {
  sync_service_.GetUserSettings()->SetPassphraseType(
      syncer::PassphraseType::kTrustedVaultPassphrase);

  syncer::DataTypeController::PreconditionContext context(
      signin::AccountManagedStatusFinderOutcome::kEnterprise);

  EXPECT_EQ(
      controller_->GetPreconditionState(context),
      syncer::DataTypeController::PreconditionState::kMustStopAndClearData);
}

TEST_F(TabContextDataTypeControllerTest,
       ShouldDisableWhenPassphraseIsNotTrustedVault) {
  sync_service_.GetUserSettings()->SetPassphraseType(
      syncer::PassphraseType::kKeystorePassphrase);

  syncer::DataTypeController::PreconditionContext context(
      signin::AccountManagedStatusFinderOutcome::kConsumerGmail);

  EXPECT_EQ(
      controller_->GetPreconditionState(context),
      syncer::DataTypeController::PreconditionState::kMustStopAndClearData);
}

TEST_F(TabContextDataTypeControllerTest,
       ShouldKeepDataWhenAccountStatusIsPending) {
  sync_service_.GetUserSettings()->SetPassphraseType(
      syncer::PassphraseType::kTrustedVaultPassphrase);

  syncer::DataTypeController::PreconditionContext context(
      signin::AccountManagedStatusFinderOutcome::kPending);

  EXPECT_EQ(
      controller_->GetPreconditionState(context),
      syncer::DataTypeController::PreconditionState::kMustStopAndKeepData);
}

TEST_F(
    TabContextDataTypeControllerTest,
    ShouldDisableWhenPassphraseIsNotTrustedVaultEvenIfAccountStatusIsPending) {
  sync_service_.GetUserSettings()->SetPassphraseType(
      syncer::PassphraseType::kKeystorePassphrase);

  syncer::DataTypeController::PreconditionContext context(
      signin::AccountManagedStatusFinderOutcome::kPending);

  EXPECT_EQ(
      controller_->GetPreconditionState(context),
      syncer::DataTypeController::PreconditionState::kMustStopAndClearData);
}

}  // namespace
}  // namespace sync_tab_context
