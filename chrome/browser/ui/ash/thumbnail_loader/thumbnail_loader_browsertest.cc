// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/thumbnail_loader/thumbnail_loader.h"

#include <memory>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/test/bind.h"
#include "chrome/browser/ash/file_manager/fileapi_util.h"
#include "chrome/browser/ash/file_manager/open_util.h"
#include "chrome/browser/ash/file_manager/volume_manager.h"
#include "chrome/browser/ash/fileapi/file_system_backend.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/ash/components/file_manager/app_id.h"
#include "content/public/test/browser_test.h"
#include "extensions/common/extension.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "storage/browser/file_system/file_system_context.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "url/origin.h"

namespace {

// References to paths set up in the test mount point during the browser test
// setup.
enum class TestPath { kNonExistent, kEmptyDir, kJpg, kBrokenJpg, kPng, kBin };

// Copies |bitmap| into |copy| and runs |callback|.
void CopyBitmapAndRunClosure(base::OnceClosure callback,
                             SkBitmap* copy,
                             const SkBitmap* bitmap,
                             base::File::Error error) {
  if (bitmap) {
    EXPECT_EQ(base::File::FILE_OK, error);
    *copy = *bitmap;
  } else {
    EXPECT_NE(base::File::FILE_OK, error);
    ADD_FAILURE() << "Got null bitmap";
  }
  std::move(callback).Run();
}

// Utility class that registers an external file system mount point, and grants
// file manager app access permission for the mount point.
class ScopedExternalMountPoint {
 public:
  ScopedExternalMountPoint(Profile* profile, const std::string& name)
      : name_(name) {
    if (!temp_dir_.CreateUniqueTempDir())
      return;

    storage::ExternalMountPoints::GetSystemInstance()->RegisterFileSystem(
        name_, storage::kFileSystemTypeLocal, storage::FileSystemMountOption(),
        temp_dir_.GetPath());
    GURL image_loader_url = extensions::Extension::GetBaseURLFromExtensionId(
        file_manager::kImageLoaderExtensionId);
    ash::FileSystemBackend::Get(
        *file_manager::util::GetFileSystemContextForSourceURL(profile,
                                                              image_loader_url))
        ->GrantFileAccessToOrigin(url::Origin::Create(image_loader_url),
                                  base::FilePath(name_));
  }

  ~ScopedExternalMountPoint() {
    storage::ExternalMountPoints::GetSystemInstance()->RevokeFileSystem(name_);
  }

  bool IsValid() const { return temp_dir_.IsValid(); }
  const base::FilePath& GetRootPath() const { return temp_dir_.GetPath(); }
  const std::string& name() const { return name_; }

 private:
  base::ScopedTempDir temp_dir_;
  std::string name_;
};

}  // namespace

class ThumbnailLoaderTest : public InProcessBrowserTest {
 public:
  ThumbnailLoaderTest() = default;
  ThumbnailLoaderTest(const ThumbnailLoaderTest&) = delete;
  ThumbnailLoaderTest& operator=(const ThumbnailLoaderTest&) = delete;
  ~ThumbnailLoaderTest() override = default;

  // InProcessBrowserTest:
  void SetUpInProcessBrowserTestFixture() override {
    InProcessBrowserTest::SetUpInProcessBrowserTestFixture();
    extensions::ComponentLoader::EnableBackgroundExtensionsForTesting();
  }
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    test_mount_point_ = std::make_unique<ScopedExternalMountPoint>(
        browser()->profile(), "test_downloads");
    thumbnail_loader_ =
        std::make_unique<ash::ThumbnailLoader>(browser()->profile());
    ASSERT_TRUE(test_mount_point_->IsValid());
    SetUpTestDirStructure();
  }
  void TearDownOnMainThread() override {
    test_mount_point_.reset();
    InProcessBrowserTest::TearDownOnMainThread();
  }

  ash::ThumbnailLoader* GetThumbnailLoader() { return thumbnail_loader_.get(); }

  base::FilePath GetTestDataFilePath(const std::string& file_name) {
    // Get the path to file manager's test data directory.
    base::FilePath source_dir;
    CHECK(base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &source_dir));
    auto test_data_dir = source_dir.AppendASCII("chrome")
                             .AppendASCII("test")
                             .AppendASCII("data")
                             .AppendASCII("chromeos")
                             .AppendASCII("file_manager");
    // Return full test data path to the given |file_name|.
    return test_data_dir.AppendASCII(file_name);
  }

  base::FilePath GetTestPath(TestPath test_path) {
    switch (test_path) {
      case TestPath::kNonExistent:
        return mount_point()->GetRootPath().AppendASCII("fake.png");
      case TestPath::kEmptyDir:
        return mount_point()->GetRootPath().AppendASCII("empty_dir");
      case TestPath::kJpg:
        return mount_point()->GetRootPath().AppendASCII("image3.jpg");
      case TestPath::kBrokenJpg:
        return mount_point()->GetRootPath().AppendASCII("broken.jpg");
      case TestPath::kPng:
        return mount_point()->GetRootPath().AppendASCII("image.png");
      case TestPath::kBin:
        return mount_point()->GetRootPath().AppendASCII("random.bin");
    }
  }

  const ScopedExternalMountPoint* mount_point() const {
    return test_mount_point_.get();
  }

 private:
  void SetUpTestDirStructure() {
    ASSERT_TRUE(base::CreateDirectory(GetTestPath(TestPath::kEmptyDir)));
    ASSERT_TRUE(base::CopyFile(GetTestDataFilePath("image.png"),
                               GetTestPath(TestPath::kPng)));
    ASSERT_TRUE(base::CopyFile(GetTestDataFilePath("image3.jpg"),
                               GetTestPath(TestPath::kJpg)));
    ASSERT_TRUE(base::CopyFile(GetTestDataFilePath("broken.jpg"),
                               GetTestPath(TestPath::kBrokenJpg)));
    ASSERT_TRUE(base::CopyFile(GetTestDataFilePath("random.bin"),
                               GetTestPath(TestPath::kBin)));
  }

  std::unique_ptr<ScopedExternalMountPoint> test_mount_point_;

  std::unique_ptr<ash::ThumbnailLoader> thumbnail_loader_;
};

IN_PROC_BROWSER_TEST_F(ThumbnailLoaderTest, LoadNonExistentFile) {
  ash::ThumbnailLoader* loader = GetThumbnailLoader();
  ASSERT_TRUE(loader);

  base::RunLoop run_loop;
  ash::ThumbnailLoader::ThumbnailRequest request(
      GetTestPath(TestPath::kNonExistent), gfx::Size(48, 48));
  loader->Load(request,
               base::BindLambdaForTesting([&run_loop](const SkBitmap* bitmap,
                                                      base::File::Error error) {
                 EXPECT_FALSE(bitmap);
                 EXPECT_EQ(base::File::FILE_ERROR_NOT_FOUND, error);
                 run_loop.Quit();
               }));
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(ThumbnailLoaderTest, LoadFolder) {
  ash::ThumbnailLoader* loader = GetThumbnailLoader();
  ASSERT_TRUE(loader);

  base::RunLoop run_loop;
  ash::ThumbnailLoader::ThumbnailRequest request(
      GetTestPath(TestPath::kEmptyDir), gfx::Size(48, 48));
  loader->Load(request,
               base::BindLambdaForTesting([&run_loop](const SkBitmap* bitmap,
                                                      base::File::Error error) {
                 EXPECT_FALSE(bitmap);
                 EXPECT_EQ(base::File::FILE_ERROR_NOT_A_FILE, error);
                 run_loop.Quit();
               }));
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(ThumbnailLoaderTest, LoadJpg) {
  ash::ThumbnailLoader* loader = GetThumbnailLoader();
  ASSERT_TRUE(loader);

  SkBitmap bitmap;
  base::RunLoop run_loop;
  ash::ThumbnailLoader::ThumbnailRequest request(GetTestPath(TestPath::kJpg),
                                                 gfx::Size(48, 48));
  loader->Load(request, base::BindOnce(&CopyBitmapAndRunClosure,
                                       run_loop.QuitClosure(), &bitmap));
  run_loop.Run();

  EXPECT_FALSE(bitmap.isNull());
  EXPECT_EQ(48, bitmap.width());
  EXPECT_EQ(48, bitmap.height());
}

IN_PROC_BROWSER_TEST_F(ThumbnailLoaderTest, LoadBrokenJpg) {
  ash::ThumbnailLoader* loader = GetThumbnailLoader();
  ASSERT_TRUE(loader);

  base::RunLoop run_loop;
  ash::ThumbnailLoader::ThumbnailRequest request(
      GetTestPath(TestPath::kBrokenJpg), gfx::Size(48, 48));
  loader->Load(request,
               base::BindLambdaForTesting([&run_loop](const SkBitmap* bitmap,
                                                      base::File::Error error) {
                 EXPECT_FALSE(bitmap);
                 EXPECT_EQ(base::File::FILE_ERROR_FAILED, error);
                 run_loop.Quit();
               }));
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(ThumbnailLoaderTest, LoadPng) {
  ash::ThumbnailLoader* loader = GetThumbnailLoader();
  ASSERT_TRUE(loader);

  SkBitmap bitmap;
  base::RunLoop run_loop;
  ash::ThumbnailLoader::ThumbnailRequest request(GetTestPath(TestPath::kPng),
                                                 gfx::Size(48, 48));
  loader->Load(request, base::BindOnce(&CopyBitmapAndRunClosure,
                                       run_loop.QuitClosure(), &bitmap));
  run_loop.Run();
  EXPECT_FALSE(bitmap.isNull());
  EXPECT_EQ(48, bitmap.width());
  EXPECT_EQ(48, bitmap.height());
}

IN_PROC_BROWSER_TEST_F(ThumbnailLoaderTest, LoadUnsupportedFiletype) {
  ash::ThumbnailLoader* loader = GetThumbnailLoader();
  ASSERT_TRUE(loader);

  ash::ThumbnailLoader::ThumbnailRequest request(GetTestPath(TestPath::kBin),
                                                 gfx::Size(48, 48));

  base::RunLoop run_loop;
  loader->Load(request,
               base::BindLambdaForTesting(
                   [&](const SkBitmap* bitmap, base::File::Error error) {
                     EXPECT_FALSE(bitmap);
                     EXPECT_EQ(error, base::File::FILE_ERROR_ABORT);
                     run_loop.Quit();
                   }));
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(ThumbnailLoaderTest, RepeatedLoads) {
  ash::ThumbnailLoader* loader = GetThumbnailLoader();
  ASSERT_TRUE(loader);

  ash::ThumbnailLoader::ThumbnailRequest request(GetTestPath(TestPath::kPng),
                                                 gfx::Size(48, 48));

  SkBitmap bitmap1;
  base::RunLoop run_loop1;
  loader->Load(request, base::BindOnce(&CopyBitmapAndRunClosure,
                                       run_loop1.QuitClosure(), &bitmap1));
  run_loop1.Run();
  ASSERT_FALSE(bitmap1.isNull());

  SkBitmap bitmap2;
  base::RunLoop run_loop2;
  loader->Load(request, base::BindOnce(&CopyBitmapAndRunClosure,
                                       run_loop2.QuitClosure(), &bitmap2));
  run_loop2.Run();
  ASSERT_FALSE(bitmap2.isNull());

  EXPECT_TRUE(gfx::test::AreBitmapsEqual(bitmap1, bitmap2));

  // Change the backing image, and verify the loaded bitmap changes, too.
  {
    base::ScopedAllowBlockingForTesting allow_io;
    ASSERT_TRUE(base::CopyFile(GetTestPath(TestPath::kJpg),
                               GetTestPath(TestPath::kPng)));
  }

  SkBitmap bitmap3;
  base::RunLoop run_loop3;
  loader->Load(request, base::BindOnce(&CopyBitmapAndRunClosure,
                                       run_loop3.QuitClosure(), &bitmap3));
  run_loop3.Run();
  ASSERT_FALSE(bitmap3.isNull());

  EXPECT_FALSE(gfx::test::AreBitmapsEqual(bitmap1, bitmap3));
}

IN_PROC_BROWSER_TEST_F(ThumbnailLoaderTest, ConcurrentLoads) {
  ash::ThumbnailLoader* loader = GetThumbnailLoader();
  ASSERT_TRUE(loader);

  SkBitmap bitmap1;
  ash::ThumbnailLoader::ThumbnailRequest request1(GetTestPath(TestPath::kPng),
                                                  gfx::Size(48, 48));
  base::RunLoop run_loop1;
  loader->Load(request1, base::BindOnce(&CopyBitmapAndRunClosure,
                                        run_loop1.QuitClosure(), &bitmap1));

  SkBitmap bitmap2;
  ash::ThumbnailLoader::ThumbnailRequest request2(GetTestPath(TestPath::kPng),
                                                  gfx::Size(96, 96));
  base::RunLoop run_loop2;
  loader->Load(request2, base::BindOnce(&CopyBitmapAndRunClosure,
                                        run_loop2.QuitClosure(), &bitmap2));

  SkBitmap bitmap3;
  base::RunLoop run_loop3;
  ash::ThumbnailLoader::ThumbnailRequest request3(GetTestPath(TestPath::kJpg),
                                                  gfx::Size(48, 48));
  loader->Load(request3, base::BindOnce(&CopyBitmapAndRunClosure,
                                        run_loop3.QuitClosure(), &bitmap3));

  run_loop1.Run();
  EXPECT_FALSE(bitmap1.isNull());
  EXPECT_EQ(48, bitmap1.width());
  EXPECT_EQ(48, bitmap1.height());

  run_loop2.Run();
  EXPECT_FALSE(bitmap2.isNull());
  EXPECT_EQ(96, bitmap2.width());
  EXPECT_EQ(96, bitmap2.height());

  run_loop3.Run();
  EXPECT_FALSE(bitmap3.isNull());
  EXPECT_EQ(48, bitmap3.width());
  EXPECT_EQ(48, bitmap3.height());

  EXPECT_FALSE(gfx::test::AreBitmapsEqual(bitmap1, bitmap2));
  EXPECT_FALSE(gfx::test::AreBitmapsEqual(bitmap1, bitmap3));
}
