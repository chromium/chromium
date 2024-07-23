// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/holding_space/holding_space_client_impl.h"

#include <string>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/holding_space/holding_space_controller.h"
#include "ash/public/cpp/holding_space/holding_space_file.h"
#include "ash/public/cpp/holding_space/holding_space_image.h"
#include "ash/public/cpp/holding_space/holding_space_item.h"
#include "ash/public/cpp/holding_space/holding_space_metrics.h"
#include "ash/public/cpp/holding_space/holding_space_model.h"
#include "ash/public/cpp/holding_space/holding_space_util.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/callback_helpers.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_future.h"
#include "base/threading/thread_restrictions.h"
#include "base/unguessable_token.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/holding_space/holding_space_browsertest_base.h"
#include "chrome/browser/ui/ash/holding_space/holding_space_test_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/drive/drive_pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "ui/gfx/image/image_skia.h"

namespace ash {
namespace {

// Aliases ---------------------------------------------------------------------

using ::testing::AllOf;
using ::testing::Eq;
using ::testing::IsFalse;
using ::testing::IsTrue;
using ::testing::ResultOf;

// Constants -------------------------------------------------------------------

// File paths for test data.
constexpr char kTestDataDir[] = "chrome/test/data/chromeos/file_manager/";
constexpr char kImageFilePath[] = "image.png";
constexpr char kTextFilePath[] = "text.txt";

// Helpers ---------------------------------------------------------------------

// Creates an empty holding space image.
std::unique_ptr<HoldingSpaceImage> CreateTestHoldingSpaceImage(
    HoldingSpaceItem::Type type,
    const base::FilePath& file_path) {
  return std::make_unique<HoldingSpaceImage>(
      holding_space_util::GetMaxImageSizeForType(type), file_path,
      /*async_bitmap_resolver=*/base::DoNothing());
}

// Copies the file for the `relative_path` in the test data directory to
// downloads directory, and returns the path to the copy.
base::FilePath TestFile(Profile* profile, const std::string& relative_path) {
  base::FilePath source_root;
  EXPECT_TRUE(
      base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &source_root));
  const base::FilePath source_file =
      source_root.AppendASCII(kTestDataDir).AppendASCII(relative_path);

  base::FilePath target_dir;
  if (!storage::ExternalMountPoints::GetSystemInstance()->GetRegisteredPath(
          file_manager::util::GetDownloadsMountPointName(profile),
          &target_dir)) {
    ADD_FAILURE() << "Failed to get downloads mount point";
    return base::FilePath();
  }

  const base::FilePath target_path = target_dir.Append(source_file.BaseName());
  base::ScopedAllowBlockingForTesting allow_blocking;
  if (!base::CopyFile(source_file, target_path)) {
    ADD_FAILURE() << "Failed to create file.";
    return base::FilePath();
  }

  return target_path;
}

}  // namespace

// Tests -----------------------------------------------------------------------

using HoldingSpaceClientImplTest = HoldingSpaceBrowserTestBase;

// Verifies that `HoldingSpaceClient::AddItemOfType()` works as intended.
IN_PROC_BROWSER_TEST_F(HoldingSpaceClientImplTest, AddItemOfType) {
  // Verify existence of controller, `client`, and `model`.
  ASSERT_TRUE(HoldingSpaceController::Get());
  auto* client = HoldingSpaceController::Get()->client();
  ASSERT_TRUE(client);
  auto* model = HoldingSpaceController::Get()->model();
  ASSERT_TRUE(model);

  // Verify `model` is initially empty.
  size_t expected_count = 0u;
  EXPECT_EQ(model->items().size(), expected_count);

  // Verify client API works for every item type.
  for (const auto expected_type : holding_space_util::GetAllItemTypes()) {
    // Create the item of the `expected_type` using the client API.
    const base::FilePath expected_file_path =
        TestFile(GetProfile(), kTextFilePath);
    const std::string& expected_id =
        client->AddItemOfType(expected_type, expected_file_path);

    // Verify the item was created as expected.
    ASSERT_EQ(model->items().size(), ++expected_count);
    const HoldingSpaceItem* item = model->items().back().get();
    EXPECT_EQ(item->id(), expected_id);
    EXPECT_EQ(item->type(), expected_type);
    EXPECT_EQ(item->file().file_path, expected_file_path);
  }
}

// Verifies that `HoldingSpaceClient::CopyImageToClipboard()` works as intended
// when attempting to copy both image backed and non-image backed holding space
// items.
IN_PROC_BROWSER_TEST_F(HoldingSpaceClientImplTest, CopyImageToClipboard) {
  ASSERT_TRUE(HoldingSpaceController::Get());

  auto* holding_space_client = HoldingSpaceController::Get()->client();
  ASSERT_TRUE(holding_space_client);

  {
    // Create a holding space item backed by a non-image file.
    auto* holding_space_item =
        AddItem(GetProfile(), HoldingSpaceItem::Type::kDownload,
                TestFile(GetProfile(), kTextFilePath));

    // We expect `HoldingSpaceClient::CopyImageToClipboard()` to fail when the
    // backing file for `holding_space_item` is not an image file.
    base::RunLoop run_loop;
    holding_space_client->CopyImageToClipboard(
        *holding_space_item, holding_space_metrics::EventSource::kTest,
        base::BindLambdaForTesting([&run_loop](bool success) {
          EXPECT_FALSE(success);
          run_loop.Quit();
        }));
    run_loop.Run();
  }

  {
    // Create a holding space item backed by an image file.
    auto* holding_space_item =
        AddItem(GetProfile(), HoldingSpaceItem::Type::kDownload,
                TestFile(GetProfile(), kImageFilePath));

    // We expect `HoldingSpaceClient::CopyImageToClipboard()` to succeed when
    // the backing file for `holding_space_item` is an image file.
    base::RunLoop run_loop;
    holding_space_client->CopyImageToClipboard(
        *holding_space_item, holding_space_metrics::EventSource::kTest,
        base::BindLambdaForTesting([&run_loop](bool success) {
          EXPECT_TRUE(success);
          run_loop.Quit();
        }));
    run_loop.Run();
  }
}

// Verifies that `HoldingSpaceClient::IsDriveDisabled()` works as intended.
IN_PROC_BROWSER_TEST_F(HoldingSpaceClientImplTest, IsDriveDisabled) {
  ASSERT_TRUE(HoldingSpaceController::Get());

  auto* holding_space_client = HoldingSpaceController::Get()->client();
  ASSERT_TRUE(holding_space_client);

  auto* prefs = GetProfile()->GetPrefs();
  EXPECT_EQ(holding_space_client->IsDriveDisabled(),
            prefs->GetBoolean(drive::prefs::kDisableDrive));
  prefs->SetBoolean(drive::prefs::kDisableDrive, true);
  EXPECT_EQ(holding_space_client->IsDriveDisabled(), true);
  prefs->SetBoolean(drive::prefs::kDisableDrive, false);
  EXPECT_EQ(holding_space_client->IsDriveDisabled(), false);
}

// Verifies that `HoldingSpaceClient::OpenDownloads()` works as intended.
IN_PROC_BROWSER_TEST_F(HoldingSpaceClientImplTest, OpenDownloads) {
  ASSERT_TRUE(HoldingSpaceController::Get());

  auto* holding_space_client = HoldingSpaceController::Get()->client();
  ASSERT_TRUE(holding_space_client);

  // We expect `HoldingSpaceClient::OpenDownloads()` to succeed.
  base::RunLoop run_loop;
  holding_space_client->OpenDownloads(
      base::BindLambdaForTesting([&run_loop](bool success) {
        EXPECT_TRUE(success);
        run_loop.Quit();
      }));
  run_loop.Run();
}

// Verifies that `HoldingSpaceClient::OpenMyFiles()` works as intended.
IN_PROC_BROWSER_TEST_F(HoldingSpaceClientImplTest, OpenMyFiles) {
  ASSERT_TRUE(HoldingSpaceController::Get());

  auto* holding_space_client = HoldingSpaceController::Get()->client();
  ASSERT_TRUE(holding_space_client);

  // We expect `HoldingSpaceClient::OpenMyFiles()` to succeed.
  base::RunLoop run_loop;
  holding_space_client->OpenMyFiles(
      base::BindLambdaForTesting([&run_loop](bool success) {
        EXPECT_TRUE(success);
        run_loop.Quit();
      }));
  run_loop.Run();
}

// Verifies that `HoldingSpaceClient::OpenItems()` works as intended when
// attempting to open holding space items backed by both non-existing and
// existing files.
IN_PROC_BROWSER_TEST_F(HoldingSpaceClientImplTest, OpenItems) {
  ASSERT_TRUE(HoldingSpaceController::Get());

  auto* holding_space_client = HoldingSpaceController::Get()->client();
  ASSERT_TRUE(holding_space_client);

  // `HoldingSpaceClient::OpenItems()` depends on Files app installation.
  WaitForTestSystemAppInstall();

  // Alias histogram names.
  static constexpr char kItemLaunchEmpty[] =
      "HoldingSpace.Item.Action.Launch.Empty";
  static constexpr char kItemLaunchEmptyExtension[] =
      "HoldingSpace.Item.Action.Launch.Empty.Extension";
  static constexpr char kItemLaunchFailure[] =
      "HoldingSpace.Item.Action.Launch.Failure";
  static constexpr char kItemLaunchFailureExtension[] =
      "HoldingSpace.Item.Action.Launch.Failure.Extension";
  static constexpr char kItemLaunchFailureReason[] =
      "HoldingSpace.Item.Action.Launch.Failure.Reason";

  // Verify no histograms have yet been recorded.
  base::HistogramTester histogram_tester;
  histogram_tester.ExpectTotalCount(kItemLaunchEmpty, 0);
  histogram_tester.ExpectTotalCount(kItemLaunchEmptyExtension, 0);
  histogram_tester.ExpectTotalCount(kItemLaunchFailure, 0);
  histogram_tester.ExpectTotalCount(kItemLaunchFailureExtension, 0);
  histogram_tester.ExpectTotalCount(kItemLaunchFailureReason, 0);

  {
    // Create a holding space `item` backed by a non-existing file.
    auto item = HoldingSpaceItem::CreateFileBackedItem(
        HoldingSpaceItem::Type::kDownload,
        HoldingSpaceFile(base::FilePath("foo.pdf"),
                         HoldingSpaceFile::FileSystemType::kTest,
                         GURL("filesystem:fake")),
        base::BindOnce(&CreateTestHoldingSpaceImage));

    // We expect `HoldingSpaceClient::OpenItems()` to fail when the backing file
    // for `item` does not exist.
    base::test::TestFuture<bool> success;
    holding_space_client->OpenItems({item.get()},
                                    holding_space_metrics::EventSource::kTest,
                                    success.GetCallback());
    EXPECT_FALSE(success.Take());

    // Verify the failure has been recorded.
    histogram_tester.ExpectBucketCount(kItemLaunchFailure, item->type(), 1);
    histogram_tester.ExpectBucketCount(
        kItemLaunchFailureExtension,
        holding_space_metrics::FilePathToExtension(item->file().file_path), 1);
    histogram_tester.ExpectBucketCount(
        kItemLaunchFailureReason,
        holding_space_metrics::ItemLaunchFailureReason::kFileInfoError, 1);
  }

  {
    // Create a holding space `item` backed by a newly created file.
    HoldingSpaceItem* item = AddPinnedFile();

    // We expect `HoldingSpaceClient::OpenItems()` to succeed when the backing
    // file for `item` exists and is empty.
    base::test::TestFuture<bool> success;
    holding_space_client->OpenItems({item},
                                    holding_space_metrics::EventSource::kTest,
                                    success.GetCallback());
    EXPECT_TRUE(success.Take());

    // Verify the empty launch has been recorded.
    histogram_tester.ExpectBucketCount(kItemLaunchEmpty, item->type(), 1);
    histogram_tester.ExpectBucketCount(
        kItemLaunchEmptyExtension,
        holding_space_metrics::FilePathToExtension(item->file().file_path), 1);

    {
      // Write "contents" to `item`'s backing file.
      base::ScopedAllowBlockingForTesting allow_blocking;
      EXPECT_TRUE(base::WriteFile(item->file().file_path, "contents"));
    }

    // We expect `HoldingSpaceClient::OpenItems()` to succeed when the backing
    // file for `item` exists and is non-empty.
    holding_space_client->OpenItems({item},
                                    holding_space_metrics::EventSource::kTest,
                                    success.GetCallback());
    EXPECT_TRUE(success.Take());
  }

  // Verify that only the expected histograms were recorded.
  histogram_tester.ExpectTotalCount(kItemLaunchEmpty, 1);
  histogram_tester.ExpectTotalCount(kItemLaunchEmptyExtension, 1);
  histogram_tester.ExpectTotalCount(kItemLaunchFailure, 1);
  histogram_tester.ExpectTotalCount(kItemLaunchFailureExtension, 1);
  histogram_tester.ExpectTotalCount(kItemLaunchFailureReason, 1);
}

// Verifies that `HoldingSpaceClient::ShowItemInFolder()` works as intended when
// attempting to open holding space items backed by both non-existing and
// existing files.
IN_PROC_BROWSER_TEST_F(HoldingSpaceClientImplTest, ShowItemInFolder) {
  ASSERT_TRUE(HoldingSpaceController::Get());

  auto* holding_space_client = HoldingSpaceController::Get()->client();
  ASSERT_TRUE(holding_space_client);

  {
    // Create a holding space item backed by a non-existing file.
    auto holding_space_item = HoldingSpaceItem::CreateFileBackedItem(
        HoldingSpaceItem::Type::kDownload,
        HoldingSpaceFile(base::FilePath("foo"),
                         HoldingSpaceFile::FileSystemType::kTest,
                         GURL("filesystem:fake")),
        base::BindOnce(&CreateTestHoldingSpaceImage));

    // We expect `HoldingSpaceClient::ShowItemInFolder()` to fail when the
    // backing file for `holding_space_item` does not exist.
    base::RunLoop run_loop;
    holding_space_client->ShowItemInFolder(
        *holding_space_item, holding_space_metrics::EventSource::kTest,
        base::BindLambdaForTesting([&run_loop](bool success) {
          EXPECT_FALSE(success);
          run_loop.Quit();
        }));
    run_loop.Run();
  }

  {
    // Create a holding space item backed by a newly created txt file.
    HoldingSpaceItem* holding_space_item = AddPinnedFile();

    // We expect `HoldingSpaceClient::ShowItemInFolder()` to succeed when the
    // backing file for `holding_space_item` exists.
    base::RunLoop run_loop;
    holding_space_client->ShowItemInFolder(
        *holding_space_item, holding_space_metrics::EventSource::kTest,
        base::BindLambdaForTesting([&run_loop](bool success) {
          EXPECT_TRUE(success);
          run_loop.Quit();
        }));
    run_loop.Run();
  }
}

// Verifies that `HoldingSpaceClient::PinItems()` works as intended.
IN_PROC_BROWSER_TEST_F(HoldingSpaceClientImplTest, PinItems) {
  ASSERT_TRUE(HoldingSpaceController::Get());

  auto* holding_space_client = HoldingSpaceController::Get()->client();
  auto* holding_space_model = HoldingSpaceController::Get()->model();
  ASSERT_TRUE(holding_space_client && holding_space_model);

  // Create a download holding space item.
  HoldingSpaceItem* download_item = AddDownloadFile();
  ASSERT_EQ(1u, holding_space_model->items().size());

  // Attempt to pin the download holding space item.
  holding_space_client->PinItems({download_item},
                                 holding_space_metrics::EventSource::kTest);
  ASSERT_EQ(2u, holding_space_model->items().size());

  // The pinned holding space item should have type `kPinnedFile` but share the
  // same text and file path as the original download holding space item.
  HoldingSpaceItem* pinned_file_item = holding_space_model->items()[1].get();
  EXPECT_EQ(pinned_file_item->type(), HoldingSpaceItem::Type::kPinnedFile);
  EXPECT_EQ(download_item->GetText(), pinned_file_item->GetText());
  EXPECT_EQ(download_item->file().file_path,
            pinned_file_item->file().file_path);
}

// Verifies that `HoldingSpaceClient::UnpinItems()` works as intended.
IN_PROC_BROWSER_TEST_F(HoldingSpaceClientImplTest, UnpinItems) {
  ASSERT_TRUE(HoldingSpaceController::Get());

  auto* holding_space_client = HoldingSpaceController::Get()->client();
  auto* holding_space_model = HoldingSpaceController::Get()->model();
  ASSERT_TRUE(holding_space_client && holding_space_model);

  // Create a pinned file holding space item.
  HoldingSpaceItem* pinned_file_item = AddPinnedFile();
  ASSERT_EQ(1u, holding_space_model->items().size());

  // Attempt to unpin the pinned file holding space item.
  holding_space_client->UnpinItems({pinned_file_item},
                                   holding_space_metrics::EventSource::kTest);
  ASSERT_EQ(0u, holding_space_model->items().size());
}

}  // namespace ash
