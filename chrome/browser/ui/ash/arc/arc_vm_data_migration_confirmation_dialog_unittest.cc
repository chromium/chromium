// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/arc/arc_vm_data_migration_confirmation_dialog.h"

#include "ash/components/arc/arc_prefs.h"
#include "ash/components/arc/arc_util.h"
#include "ash/test/ash_test_base.h"
#include "base/test/bind.h"
#include "base/time/time.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/window/dialog_delegate.h"

namespace arc {

namespace {

class ArcVmDataMigrationConfirmationDialogTest : public ash::AshTestBase {
 public:
  ArcVmDataMigrationConfirmationDialogTest() = default;
  ArcVmDataMigrationConfirmationDialogTest(
      const ArcVmDataMigrationConfirmationDialogTest&) = delete;
  ArcVmDataMigrationConfirmationDialogTest& operator=(
      const ArcVmDataMigrationConfirmationDialogTest&) = delete;
  ~ArcVmDataMigrationConfirmationDialogTest() override = default;

  void SetUp() override {
    ash::AshTestBase::SetUp();

    prefs::RegisterProfilePrefs(profile_prefs_.registry());
    profile_prefs()->SetTime(
        prefs::kArcVmDataMigrationNotificationFirstShownTime,
        base::Time::Now());

    layout_provider_ = ChromeLayoutProvider::CreateLayoutProvider();
  }

 protected:
  PrefService* profile_prefs() { return &profile_prefs_; }

 private:
  TestingPrefServiceSimple profile_prefs_;

  std::unique_ptr<views::LayoutProvider> layout_provider_;
};

TEST_F(ArcVmDataMigrationConfirmationDialogTest, Show) {
  ShowArcVmDataMigrationConfirmationDialog(profile_prefs(), base::DoNothing());
}

TEST_F(ArcVmDataMigrationConfirmationDialogTest, Accept) {
  int accept_count = 0;
  int cancel_count = 0;
  std::unique_ptr<ArcVmDataMigrationConfirmationDialog> dialog =
      std::make_unique<ArcVmDataMigrationConfirmationDialog>(
          profile_prefs(), base::BindLambdaForTesting([&](bool accepted) {
            if (accepted) {
              accept_count++;
            } else {
              cancel_count++;
            }
          }));
  dialog->Accept();
  EXPECT_EQ(1, accept_count);
  EXPECT_EQ(0, cancel_count);
}

TEST_F(ArcVmDataMigrationConfirmationDialogTest, Cancel) {
  int accept_count = 0;
  int cancel_count = 0;
  std::unique_ptr<ArcVmDataMigrationConfirmationDialog> dialog =
      std::make_unique<ArcVmDataMigrationConfirmationDialog>(
          profile_prefs(), base::BindLambdaForTesting([&](bool accepted) {
            if (accepted) {
              accept_count++;
            } else {
              cancel_count++;
            }
          }));
  dialog->Cancel();
  EXPECT_EQ(0, accept_count);
  EXPECT_EQ(1, cancel_count);
}

TEST_F(ArcVmDataMigrationConfirmationDialogTest, UpdateNeededNow) {
  profile_prefs()->SetTime(
      prefs::kArcVmDataMigrationNotificationFirstShownTime,
      base::Time::Now() - kArcVmDataMigrationDismissibleTimeDelta);

  int accept_count = 0;
  int cancel_count = 0;
  std::unique_ptr<ArcVmDataMigrationConfirmationDialog> dialog =
      std::make_unique<ArcVmDataMigrationConfirmationDialog>(
          profile_prefs(), base::BindLambdaForTesting([&](bool accepted) {
            if (accepted) {
              accept_count++;
            } else {
              cancel_count++;
            }
          }));
  // There should be no cancel button.
  EXPECT_EQ(dialog->buttons(), static_cast<int>(ui::mojom::DialogButton::kOk));
  dialog->Accept();
  EXPECT_EQ(1, accept_count);
  EXPECT_EQ(0, cancel_count);
}

}  // namespace

}  // namespace arc
