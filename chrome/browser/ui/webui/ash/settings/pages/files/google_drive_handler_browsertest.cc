// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <initializer_list>

#include "ash/constants/ash_features.h"
#include "base/files/file_util.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/ash/drive/drive_integration_service_browser_test_base.h"
#include "chrome/browser/ash/drive/file_system_util.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/webui/ash/settings/pages/files/mojom/google_drive_handler.mojom.h"
#include "chrome/browser/ui/webui/ash/settings/test_support/os_settings_browser_test_mixin.h"
#include "chrome/test/data/webui/chromeos/settings/test_api.test-mojom-test-utils.h"
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
using testing::InSequence;
using testing::Return;
using testing::TestParamInfo;
using testing::ValuesIn;
using testing::WithParamInterface;

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

struct TestParam {
  std::string test_suffix;
  std::vector<base::test::FeatureRef> enabled_features;
  std::vector<base::test::FeatureRef> disabled_features;
};

std::string ParamToTestSuffix(const TestParamInfo<TestParam>& info) {
  return info.param.test_suffix;
}

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
    std::optional<std::vector<drivefs::mojom::QueryItemPtr>> items(
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

class GoogleDriveHandlerBaseTest
    : public drive::DriveIntegrationServiceBrowserTestBase {
 public:
  GoogleDriveHandlerBaseTest() : receiver_(&fake_search_query_) {}

  GoogleDriveHandlerBaseTest(const GoogleDriveHandlerBaseTest&) = delete;
  GoogleDriveHandlerBaseTest& operator=(const GoogleDriveHandlerBaseTest&) =
      delete;

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

  base::FilePath CreateFileInContentCache(int file_size_in_bytes) {
    auto* const service =
        drive::util::GetIntegrationServiceByProfile(browser()->profile());
    {
      // Ensure the content cache directory exists.
      base::ScopedAllowBlockingForTesting allow_blocking;
      if (!base::DirectoryExists(service->GetDriveFsContentCachePath())) {
        EXPECT_TRUE(
            base::CreateDirectory(service->GetDriveFsContentCachePath()));
      }
    }
    const base::FilePath file_path =
        service->GetDriveFsContentCachePath().Append("foo.txt");
    {
      // Create a file of `file_size_in_bytes` bytes in the content_cache
      // directory.
      base::ScopedAllowBlockingForTesting allow_blocking;
      EXPECT_TRUE(base::WriteFile(file_path,
                                  base::RandBytesAsString(file_size_in_bytes)));
    }

    return file_path;
  }

 protected:
  OSSettingsBrowserTestMixin os_settings_mixin_{&mixin_host_};
  FakeSearchQuery fake_search_query_;
  base::test::ScopedFeatureList scoped_feature_list_;

 private:
  mojo::Remote<mojom::OSSettingsDriver> os_settings_driver_remote_;
  mojo::Remote<mojom::GoogleDriveSettings> google_drive_settings_remote_;
  mojo::Receiver<drivefs::mojom::SearchQuery> receiver_;
};

class GoogleDriveHandlerBulkPinningTest : public GoogleDriveHandlerBaseTest {
 public:
  GoogleDriveHandlerBulkPinningTest() {
    scoped_feature_list_.InitWithFeatures(
        {ash::features::kDriveFsBulkPinning,
         ash::features::kFeatureManagementDriveFsBulkPinning},
        {});
  }
};

IN_PROC_BROWSER_TEST_F(GoogleDriveHandlerBulkPinningTest,
                       NoSearchResultsReturnsNoRequiredOnlyFreeSpace) {
  SetUpSearchResultExpectations();
  fake_search_query_.SetSearchResults({});

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

IN_PROC_BROWSER_TEST_F(GoogleDriveHandlerBulkPinningTest,
                       OnlyUnpinnedResultsUpdateTheSpaceRequirements) {
  SetUpSearchResultExpectations();

  // Each item is 125 MB in size, total required space should be 500 MB.
  int64_t file_size = 125 * 1024 * 1024;
  fake_search_query_.SetSearchResults(
      {{.size = file_size}, {.size = file_size}});
  fake_search_query_.SetSearchResults(
      {{.size = file_size}, {.size = file_size}});
  fake_search_query_.SetSearchResults({});

  int64_t free_space = int64_t(3) << 30;  // 3 GB.
  auto required_space = FormatBytesToString(file_size * 4);
  auto free_space_str = FormatBytesToString(free_space);

  ash::FakeSpacedClient::Get()->set_free_disk_space(free_space);
  auto google_drive_settings = OpenGoogleDriveSettings();
  google_drive_settings.AssertBulkPinningSpace(required_space, free_space_str);
}

class GoogleDriveHandlerTest : public GoogleDriveHandlerBaseTest,
                               public WithParamInterface<TestParam> {
 public:
  GoogleDriveHandlerTest() {
    scoped_feature_list_.InitWithFeatures(GetParam().enabled_features,
                                          GetParam().disabled_features);
  }
};

IN_PROC_BROWSER_TEST_P(GoogleDriveHandlerTest,
                       TotalPinnedSizeUpdatesValueOnElement) {
  // Mock no search results are returned (this avoids the call to
  // `CalculateRequiredSpace` from being ran here).
  fake_search_query_.SetSearchResults({});
  ash::FakeSpacedClient::Get()->set_free_disk_space(int64_t(3) << 30);

  CreateFileInContentCache(32);

  auto google_drive_settings = OpenGoogleDriveSettings();
  google_drive_settings.AssertContentCacheSize(FormatBytesToString(4096));
}

IN_PROC_BROWSER_TEST_P(GoogleDriveHandlerTest,
                       ClearingOfflineFilesCallsProperMethods) {
  // Mock no search results are returned (this avoids the call to
  // `CalculateRequiredSpace` from being ran here).
  fake_search_query_.SetSearchResults({});
  ash::FakeSpacedClient::Get()->set_free_disk_space(int64_t(3) << 30);

  const base::FilePath file_path = CreateFileInContentCache(32);

  auto* fake_drivefs = GetFakeDriveFsForProfile(browser()->profile());
  EXPECT_CALL(*fake_drivefs, ClearOfflineFiles(_))
      .WillOnce(
          [&file_path](
              drivefs::mojom::DriveFs::ClearOfflineFilesCallback callback) {
            {
              base::ScopedAllowBlockingForTesting allow_blocking;
              ASSERT_TRUE(base::DeleteFile(file_path));
            }
            std::move(callback).Run(drive::FILE_ERROR_OK);
          });

  auto google_drive_settings = OpenGoogleDriveSettings();
  google_drive_settings.ClickClearOfflineFilesAndAssertNewSize(
      FormatBytesToString(0));
}

const TestParam kTestParams[] = {
    {
        .test_suffix = "BulkPinning",
        .enabled_features =
            {ash::features::kDriveFsBulkPinning,
             ash::features::kFeatureManagementDriveFsBulkPinning},
        .disabled_features = {},
    },

    // OsSettingsRevampWayfinding feature test variations
    {
        .test_suffix = "BulkPinning_Revamp",
        .enabled_features =
            {ash::features::kOsSettingsRevampWayfinding,
             ash::features::kDriveFsBulkPinning,
             ash::features::kFeatureManagementDriveFsBulkPinning},
        .disabled_features = {},
    },
};

INSTANTIATE_TEST_SUITE_P(,
                         GoogleDriveHandlerTest,
                         ValuesIn(kTestParams),
                         &ParamToTestSuffix);

}  // namespace
}  // namespace ash::settings
