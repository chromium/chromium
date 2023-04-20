// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <initializer_list>

#include "ash/constants/ash_features.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/drive/drive_integration_service_browser_test_base.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/webui/settings/ash/files_page/mojom/google_drive_handler.mojom.h"
#include "chrome/browser/ui/webui/settings/ash/os_settings_browser_test_mixin.h"
#include "chrome/test/data/webui/settings/chromeos/test_api.test-mojom-test-utils.h"
#include "chromeos/ash/components/dbus/spaced/fake_spaced_client.h"
#include "chromeos/ash/components/drivefs/fake_drivefs.h"
#include "chromeos/ash/components/drivefs/mojom/drivefs.mojom-forward.h"
#include "content/public/test/browser_test.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "ui/base/text/bytes_formatting.h"

using base::test::RunOnceCallback;
using testing::_;
using testing::DoAll;
using testing::Return;

namespace ash::settings {
namespace {

const std::string FormatBytesToString(int64_t bytes) {
  return base::UTF16ToUTF8(ui::FormatBytes(bytes));
}

// Provides a minimal interface to initialize a drive item with only the
// required fields.
struct DriveItem {
  int64_t size;
  bool available_offline;
  bool pinned;
};

class FakeSearchQuery : public drivefs::mojom::SearchQuery {
 public:
  void SetSearchResults(std::vector<DriveItem> page_items) {
    std::vector<drivefs::mojom::QueryItemPtr> result;
    result.reserve(page_items.size());
    for (const auto& item : page_items) {
      drivefs::mojom::QueryItemPtr p = drivefs::mojom::QueryItem::New();
      // Paths must be parented at "/root" to be considered for space
      // calculations.
      p->path =
          base::FilePath("/root").Append(base::NumberToString(++item_counter_));
      p->metadata = drivefs::mojom::FileMetadata::New();
      p->metadata->capabilities = drivefs::mojom::Capabilities::New();
      p->metadata->size = item.size;
      p->metadata->available_offline = item.available_offline;
      p->metadata->pinned = item.pinned;
      p->metadata->stable_id = item_counter_;
      result.push_back(std::move(p));
    }
    pages_.push_back(std::move(result));
  }

  void GetNextPage(GetNextPageCallback callback) override {
    ASSERT_LT(page_counter_, pages_.size()) << "Another page was expected";
    absl::optional<std::vector<drivefs::mojom::QueryItemPtr>> items(
        std::move(pages_.at(page_counter_++)));
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), drive::FILE_ERROR_OK,
                                  std::move(items)));
  }

 private:
  size_t page_counter_ = 0;
  size_t item_counter_ = 0;
  std::vector<std::vector<drivefs::mojom::QueryItemPtr>> pages_;
};

class GoogleDriveHandlerTest
    : public drive::DriveIntegrationServiceBrowserTestBase {
 public:
  GoogleDriveHandlerTest() : receiver_(&fake_search_query_) {
    scoped_feature_list_.InitWithFeatures({ash::features::kDriveFsBulkPinning},
                                          {});
  }

  GoogleDriveHandlerTest(const GoogleDriveHandlerTest&) = delete;
  GoogleDriveHandlerTest& operator=(const GoogleDriveHandlerTest&) = delete;

  void SetUp() override {
    drive::DriveIntegrationServiceBrowserTestBase::SetUp();
    ash::SpacedClient::InitializeFake();
  }

  // Open Google drive settings and returns the `GoogleDriveSettings` page
  // object to interact with the test api.
  mojom::GoogleDriveSettingsAsyncWaiter OpenGoogleDriveSettings() {
    os_settings_driver_remote_ =
        mojo::Remote(os_settings_mixin_.OpenOSSettings("/googleDrive"));
    auto os_settings_driver =
        mojom::OSSettingsDriverAsyncWaiter(os_settings_driver_remote_.get());
    google_drive_settings_remote_ =
        mojo::Remote(os_settings_driver.AssertOnGoogleDriveSettings());
    return mojom::GoogleDriveSettingsAsyncWaiter(
        google_drive_settings_remote_.get());
  }

  void SetUpSearchResultExpectations() {
    // Ensure when a search query is made, it binds to the mock search query.
    auto* fake_drivefs = GetFakeDriveFsForProfile(browser()->profile());
    EXPECT_CALL(*fake_drivefs, StartSearchQuery(_, _))
        .WillOnce([&](mojo::PendingReceiver<drivefs::mojom::SearchQuery>
                          pending_receiver,
                      drivefs::mojom::QueryParametersPtr query_params) {
          receiver_.Bind(std::move(pending_receiver));
        });
  }

 protected:
  OSSettingsBrowserTestMixin os_settings_mixin_{&mixin_host_};
  FakeSearchQuery fake_search_query_;

 private:
  mojo::Remote<mojom::OSSettingsDriver> os_settings_driver_remote_;
  mojo::Remote<mojom::GoogleDriveSettings> google_drive_settings_remote_;
  base::test::ScopedFeatureList scoped_feature_list_;
  mojo::Receiver<drivefs::mojom::SearchQuery> receiver_;
};

IN_PROC_BROWSER_TEST_F(GoogleDriveHandlerTest,
                       NoSearchResultsReturnsNoRequiredOnlyFreeSpace) {
  SetUpSearchResultExpectations();
  fake_search_query_.SetSearchResults({});

  auto* fake_drivefs = GetFakeDriveFsForProfile(browser()->profile());
  EXPECT_CALL(*fake_drivefs, GetOfflineFilesSpaceUsage(_))
      .WillOnce(RunOnceCallback<0>(drive::FILE_ERROR_OK, 1));

  // Expect the free space to be 1 GB (1,073,741,824 bytes), the required space
  // to be 0 KB (0 items).
  int64_t free_space = 1024 * 1024 * 1024;
  auto required_space = FormatBytesToString(0);
  auto remaining_space = FormatBytesToString(free_space);

  // Fake the free space returned, open Google drive settings and ensure the
  // values on the google drive subpage element match those returned via the
  // observable.
  ash::FakeSpacedClient::Get()->set_free_disk_space(free_space);
  auto google_drive_settings = OpenGoogleDriveSettings();
  google_drive_settings.AssertBulkPinningSpace(required_space, remaining_space);
}

IN_PROC_BROWSER_TEST_F(GoogleDriveHandlerTest,
                       OnlyUnpinnedResultsUpdateTheSpaceRequirements) {
  SetUpSearchResultExpectations();

  auto* fake_drivefs = GetFakeDriveFsForProfile(browser()->profile());
  EXPECT_CALL(*fake_drivefs, GetOfflineFilesSpaceUsage(_))
      .WillOnce(RunOnceCallback<0>(drive::FILE_ERROR_OK, 1));

  // Each item is 125 MB in size, total required space should be 500 MB.
  int64_t file_size = 125 * 1024 * 1024;
  fake_search_query_.SetSearchResults(
      {{.size = file_size}, {.size = file_size}});
  fake_search_query_.SetSearchResults(
      {{.size = file_size}, {.size = file_size}});
  fake_search_query_.SetSearchResults({});

  int64_t free_space = 1024 * 1024 * 1024;
  auto required_space = FormatBytesToString(file_size * 4);
  auto remaining_space = FormatBytesToString(free_space - (file_size * 4));

  ash::FakeSpacedClient::Get()->set_free_disk_space(free_space);
  auto google_drive_settings = OpenGoogleDriveSettings();
  google_drive_settings.AssertBulkPinningSpace(required_space, remaining_space);
}

IN_PROC_BROWSER_TEST_F(GoogleDriveHandlerTest,
                       TotalPinnedSizeUpdatesValueOnElement) {
  SetUpSearchResultExpectations();

  // Mock no search results are returned (this avoids the call to
  // `CalculateRequiredSpace` from being ran here).
  fake_search_query_.SetSearchResults({});
  ash::FakeSpacedClient::Get()->set_free_disk_space(1024 * 1024 * 1024);

  int64_t pinned_size = 1024 * 1024;
  auto* fake_drivefs = GetFakeDriveFsForProfile(browser()->profile());
  EXPECT_CALL(*fake_drivefs, GetOfflineFilesSpaceUsage(_))
      .WillOnce(RunOnceCallback<0>(drive::FILE_ERROR_OK, pinned_size));

  auto google_drive_settings = OpenGoogleDriveSettings();
  google_drive_settings.AssertBulkPinningPinnedSize(
      FormatBytesToString(pinned_size));
}

IN_PROC_BROWSER_TEST_F(GoogleDriveHandlerTest,
                       InvalidSizeUpdatesRemainingSizeToUnknown) {
  SetUpSearchResultExpectations();

  // Mock no search results are returned (this avoids the call to
  // `CalculateRequiredSpace` from being ran here).
  fake_search_query_.SetSearchResults({});
  ash::FakeSpacedClient::Get()->set_free_disk_space(1024 * 1024 * 1024);

  auto* fake_drivefs = GetFakeDriveFsForProfile(browser()->profile());
  EXPECT_CALL(*fake_drivefs, GetOfflineFilesSpaceUsage(_))
      .WillOnce(RunOnceCallback<0>(drive::FILE_ERROR_OK, -1));

  auto google_drive_settings = OpenGoogleDriveSettings();
  google_drive_settings.AssertBulkPinningPinnedSize("Unknown");
}

IN_PROC_BROWSER_TEST_F(GoogleDriveHandlerTest,
                       ClearingOfflineFilesCallsProperMethods) {
  SetUpSearchResultExpectations();

  // Mock no search results are returned (this avoids the call to
  // `CalculateRequiredSpace` from being ran here).
  fake_search_query_.SetSearchResults({});
  ash::FakeSpacedClient::Get()->set_free_disk_space(1024 * 1024 * 1024);

  int64_t pinned_size = 1024 * 1024;
  auto* fake_drivefs = GetFakeDriveFsForProfile(browser()->profile());
  EXPECT_CALL(*fake_drivefs, GetOfflineFilesSpaceUsage(_))
      .Times(2)
      .WillOnce(RunOnceCallback<0>(drive::FILE_ERROR_OK, pinned_size))
      .WillOnce(RunOnceCallback<0>(drive::FILE_ERROR_OK, 0));

  auto google_drive_settings = OpenGoogleDriveSettings();
  google_drive_settings.ClickClearOfflineFilesAndAssertNewSize(
      FormatBytesToString(0));
}

}  // namespace
}  // namespace ash::settings
