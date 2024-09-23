// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/component_updater/android/component_loader_policy.h"

#include <fcntl.h>
#include <jni.h>
#include <stdint.h>

#include <iterator>
#include <string>
#include <utility>
#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "base/version.h"
#include "components/crash/core/common/crash_key.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace component_updater {

namespace {

constexpr char kComponentId[] = "llkgjffcdpffmhiakmfcdcblohccpfmo";
// This hash corresponds to kComponentId.
constexpr uint8_t kSha256Hash[] = {
    0xbb, 0xa6, 0x95, 0x52, 0x3f, 0x55, 0xc7, 0x80, 0xac, 0x52, 0x32,
    0x1b, 0xe7, 0x22, 0xf5, 0xce, 0x6a, 0xfd, 0x9c, 0x9e, 0xa9, 0x2a,
    0x0b, 0x50, 0x60, 0x2b, 0x7f, 0x6c, 0x64, 0x80, 0x09, 0x04};

constexpr char kMockComponentHistogramName[] =
    "ComponentUpdater.AndroidComponentLoader.LoadStatus.MockComponent";

void GetPkHash(std::vector<uint8_t>* hash) {
  hash->assign(std::begin(kSha256Hash), std::end(kSha256Hash));
}

std::vector<int> OpenFileFds(const base::FilePath& base,
                             const std::vector<std::string>& files) {
  std::vector<int> fds;
  for (const std::string& file : files) {
    base::FilePath path = base.AppendASCII(file);
    fds.push_back(open(path.MaybeAsASCII().c_str(), O_RDONLY));
  }
  return fds;
}

using OnLoadedTestCallBack =
    base::OnceCallback<void(const base::Version&,
                            base::flat_map<std::string, base::ScopedFD>&,
                            base::Value::Dict)>;
using OnFailedTestCallBack = base::OnceCallback<void(ComponentLoadResult)>;

class MockLoaderPolicy : public ComponentLoaderPolicy {
 public:
  explicit MockLoaderPolicy(OnLoadedTestCallBack on_loaded,
                            OnFailedTestCallBack on_failed)
      : on_loaded_(std::move(on_loaded)), on_failed_(std::move(on_failed)) {}

  MockLoaderPolicy()
      : on_loaded_(base::DoNothing()), on_failed_(base::DoNothing()) {}

  ~MockLoaderPolicy() override = default;

  MockLoaderPolicy(const MockLoaderPolicy&) = delete;
  MockLoaderPolicy& operator=(const MockLoaderPolicy&) = delete;

  void ComponentLoaded(const base::Version& version,
                       base::flat_map<std::string, base::ScopedFD>& fd_map,
                       base::Value::Dict manifest) override {
    std::move(on_loaded_).Run(version, fd_map, std::move(manifest));
  }

  void ComponentLoadFailed(ComponentLoadResult error) override {
    std::move(on_failed_).Run(error);
  }

  void GetHash(std::vector<uint8_t>* hash) const override { GetPkHash(hash); }

  std::string GetMetricsSuffix() const override { return "MockComponent"; }

 private:
  OnLoadedTestCallBack on_loaded_;
  OnFailedTestCallBack on_failed_;
};

void VerifyComponentLoaded(base::OnceClosure on_done,
                           const base::Version& version,
                           base::flat_map<std::string, base::ScopedFD>& fd_map,
                           base::Value::Dict manifest) {
  EXPECT_EQ(version.GetString(), "123.456.789");
  EXPECT_EQ(fd_map.size(), 2u);
  EXPECT_NE(fd_map.find("file1.txt"), fd_map.end());
  EXPECT_NE(fd_map.find("file2.txt"), fd_map.end());

  std::move(on_done).Run();
}

}  // namespace

class AndroidComponentLoaderPolicyTest : public testing::Test {
 public:
  AndroidComponentLoaderPolicyTest() {
    crash_reporter::ResetCrashKeysForTesting();
    crash_reporter::InitializeCrashKeysForTesting();
  }
  ~AndroidComponentLoaderPolicyTest() override {
    crash_reporter::ResetCrashKeysForTesting();
  }

  AndroidComponentLoaderPolicyTest(const AndroidComponentLoaderPolicyTest&) =
      delete;
  AndroidComponentLoaderPolicyTest& operator=(
      const AndroidComponentLoaderPolicyTest&) = delete;

  void SetUp() override {
    env_ = base::android::AttachCurrentThread();
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  }

 protected:
  void WriteFile(const std::string& file_name, const std::string& content) {
    ASSERT_TRUE(
        base::WriteFile(temp_dir_.GetPath().AppendASCII(file_name), content));
    files_.push_back(file_name);
  }

  std::vector<int> GetFileFds() const {
    return OpenFileFds(temp_dir_.GetPath(), files_);
  }

  raw_ptr<JNIEnv> env_ = nullptr;
  std::vector<std::string> files_;
  base::HistogramTester histogram_tester_;

 private:
  base::ScopedTempDir temp_dir_;
};

TEST_F(AndroidComponentLoaderPolicyTest, TestValidManifest) {
  base::test::TaskEnvironment task_environment;

  WriteFile("file1.txt", "1");
  WriteFile("file2.txt", "2");
  WriteFile("manifest.json",
            "{\n\"manifest_version\": 2,\n\"version\": \"123.456.789\"\n}");

  base::RunLoop run_loop;
  auto* android_policy =
      new AndroidComponentLoaderPolicy(std::make_unique<MockLoaderPolicy>(
          base::BindOnce(&VerifyComponentLoaded, run_loop.QuitClosure()),
          base::BindOnce([](ComponentLoadResult) { FAIL(); })));

  android_policy->ComponentLoaded(
      env_, base::android::ToJavaArrayOfStrings(env_, files_),
      base::android::ToJavaIntArray(env_, GetFileFds()));
  run_loop.Run();

  histogram_tester_.ExpectBucketCount(kMockComponentHistogramName,
                                      ComponentLoadResult::kComponentLoaded, 1);
  histogram_tester_.ExpectTotalCount(kMockComponentHistogramName, 1);
  EXPECT_EQ("ORIGIN_TRIALS-123.456.789",
            crash_reporter::GetCrashKeyValue("crx-components"));
}

TEST_F(AndroidComponentLoaderPolicyTest, TestMissingManifest) {
  base::test::TaskEnvironment task_environment;

  WriteFile("file.txt", "test");

  base::RunLoop run_loop;
  auto* android_policy =
      new AndroidComponentLoaderPolicy(std::make_unique<MockLoaderPolicy>(
          base::BindOnce([](const base::Version& version,
                            base::flat_map<std::string, base::ScopedFD>& fd_map,
                            base::Value::Dict manifest) { FAIL(); }),
          base::BindLambdaForTesting([&](ComponentLoadResult error) {
            ASSERT_EQ(error, ComponentLoadResult::kMissingManifest);
            run_loop.Quit();
          })));

  android_policy->ComponentLoaded(
      env_, base::android::ToJavaArrayOfStrings(env_, files_),
      base::android::ToJavaIntArray(env_, GetFileFds()));
  run_loop.Run();

  histogram_tester_.ExpectBucketCount(kMockComponentHistogramName,
                                      ComponentLoadResult::kMissingManifest, 1);
  histogram_tester_.ExpectTotalCount(kMockComponentHistogramName, 1);
}

TEST_F(AndroidComponentLoaderPolicyTest, TestInvalidVersion) {
  base::test::TaskEnvironment task_environment;

  WriteFile("file.txt", "test");
  WriteFile("manifest.json",
            "{\n\"manifest_version\": 2,\n\"version\": \"\"\n}");

  base::RunLoop run_loop;
  auto* android_policy =
      new AndroidComponentLoaderPolicy(std::make_unique<MockLoaderPolicy>(
          base::BindOnce([](const base::Version& version,
                            base::flat_map<std::string, base::ScopedFD>& fd_map,
                            base::Value::Dict manifest) { FAIL(); }),
          base::BindLambdaForTesting([&](ComponentLoadResult error) {
            ASSERT_EQ(error, ComponentLoadResult::kInvalidVersion);
            run_loop.Quit();
          })));

  android_policy->ComponentLoaded(
      env_, base::android::ToJavaArrayOfStrings(env_, files_),
      base::android::ToJavaIntArray(env_, GetFileFds()));
  run_loop.Run();

  histogram_tester_.ExpectBucketCount(kMockComponentHistogramName,
                                      ComponentLoadResult::kInvalidVersion, 1);
  histogram_tester_.ExpectTotalCount(kMockComponentHistogramName, 1);
}

TEST_F(AndroidComponentLoaderPolicyTest, TestInvalidManifest) {
  base::test::TaskEnvironment task_environment;

  WriteFile("file.txt", "test");
  WriteFile("manifest.json", "{\n\"manifest_version\":}");

  base::RunLoop run_loop;
  auto* android_policy =
      new AndroidComponentLoaderPolicy(std::make_unique<MockLoaderPolicy>(
          base::BindOnce([](const base::Version& version,
                            base::flat_map<std::string, base::ScopedFD>& fd_map,
                            base::Value::Dict manifest) { FAIL(); }),
          base::BindLambdaForTesting([&](ComponentLoadResult error) {
            ASSERT_EQ(error, ComponentLoadResult::kMalformedManifest);
            run_loop.Quit();
          })));

  android_policy->ComponentLoaded(
      env_, base::android::ToJavaArrayOfStrings(env_, files_),
      base::android::ToJavaIntArray(env_, GetFileFds()));
  run_loop.Run();

  histogram_tester_.ExpectBucketCount(
      kMockComponentHistogramName, ComponentLoadResult::kMalformedManifest, 1);
  histogram_tester_.ExpectTotalCount(kMockComponentHistogramName, 1);
}

TEST_F(AndroidComponentLoaderPolicyTest, TestGetComponentId) {
  base::test::TaskEnvironment task_environment;

  auto* android_policy =
      new AndroidComponentLoaderPolicy(std::make_unique<MockLoaderPolicy>());

  base::android::ScopedJavaLocalRef<jstring> jcomponentId =
      android_policy->GetComponentId(env_);

  EXPECT_EQ(base::android::ConvertJavaStringToUTF8(jcomponentId), kComponentId);
}

}  // namespace component_updater
