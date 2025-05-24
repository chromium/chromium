// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/media_gallery_util/public/cpp/safe_audio_video_checker.h"

#include <utility>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

SafeAudioVideoChecker* CreateCheckerAndSetCallback(
    base::File file,
    SafeAudioVideoChecker::ResultCallback cb) {
  auto checker = std::make_unique<SafeAudioVideoChecker>(std::move(file),
                                                         base::DoNothing());

  SafeAudioVideoChecker* checker_ptr = checker.get();
  checker_ptr->SetResultCallbackForTesting(base::BindLambdaForTesting(
      [checker = std::move(checker),
       cb = std::move(cb)](base::File::Error result) mutable {
        content::GetUIThreadTaskRunner({})->PostTask(
            FROM_HERE, base::BindOnce(std::move(cb), result));
      }));
  return checker_ptr;
}

}  // namespace

class SafeAudioVideoCheckerBrowserTest : public InProcessBrowserTest {
 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    InProcessBrowserTest::SetUp();
  }

  base::File CreateTempFile() {
    base::ScopedAllowBlockingForTesting allow_blocking;
    base::FilePath file_path = temp_dir_.GetPath().AppendASCII("test_file");
    return base::File(file_path,
                      base::File::FLAG_CREATE | base::File::FLAG_READ);
  }

 private:
  base::ScopedTempDir temp_dir_;
};

IN_PROC_BROWSER_TEST_F(SafeAudioVideoCheckerBrowserTest, OnConnectionError) {
  base::File file = CreateTempFile();
  ASSERT_TRUE(file.IsValid());

  base::test::TestFuture<base::File::Error> future;
  SafeAudioVideoChecker::ResultCallback cb = future.GetCallback();

  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        auto* checker =
            CreateCheckerAndSetCallback(std::move(file), std::move(cb));
        checker->OnConnectionErrorForTesting();
      }));

  EXPECT_EQ(future.Get(), base::File::FILE_ERROR_SECURITY);
}

IN_PROC_BROWSER_TEST_F(SafeAudioVideoCheckerBrowserTest, StartWithInvalidFile) {
  base::File file;
  ASSERT_TRUE(!file.IsValid());

  base::test::TestFuture<base::File::Error> future;
  SafeAudioVideoChecker::ResultCallback cb = future.GetCallback();

  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        auto* checker =
            CreateCheckerAndSetCallback(std::move(file), std::move(cb));
        checker->Start();
      }));

  EXPECT_EQ(future.Get(), base::File::FILE_ERROR_SECURITY);
}

IN_PROC_BROWSER_TEST_F(SafeAudioVideoCheckerBrowserTest,
                       StartWithValidNonMediaFile) {
  base::File file = CreateTempFile();
  ASSERT_TRUE(file.IsValid());

  base::test::TestFuture<base::File::Error> future;
  SafeAudioVideoChecker::ResultCallback cb = future.GetCallback();

  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        auto* checker =
            CreateCheckerAndSetCallback(std::move(file), std::move(cb));
        checker->Start();
      }));

  EXPECT_EQ(future.Get(), base::File::FILE_ERROR_SECURITY);
}
