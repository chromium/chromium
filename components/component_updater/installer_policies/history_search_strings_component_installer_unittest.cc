// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/component_updater/installer_policies/history_search_strings_component_installer.h"

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_set>
#include <utility>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/run_until.h"
#include "base/test/task_environment.h"
#include "base/version.h"
#include "components/component_updater/mock_component_updater_service.h"
#include "components/history_embeddings/core/search_strings_update_listener.h"
#include "components/optimization_guide/proto/features/history_search_strings.pb.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace component_updater {

namespace {

using testing::_;
using testing::Return;

}  // namespace

class HistorySearchStringsComponentInstallerPolicyPublic
    : public HistorySearchStringsComponentInstallerPolicy {
 public:
  HistorySearchStringsComponentInstallerPolicyPublic() = default;
  using HistorySearchStringsComponentInstallerPolicy::ComponentReady;
  using HistorySearchStringsComponentInstallerPolicy::VerifyInstallation;
};

class HistorySearchStringsComponentInstallerPolicyTest : public PlatformTest {
 public:
  HistorySearchStringsComponentInstallerPolicyTest() = default;
  ~HistorySearchStringsComponentInstallerPolicyTest() override = default;
  HistorySearchStringsComponentInstallerPolicyTest(
      const HistorySearchStringsComponentInstallerPolicyTest&) = delete;
  HistorySearchStringsComponentInstallerPolicyTest& operator=(
      const HistorySearchStringsComponentInstallerPolicyTest&) = delete;

  void SetUp() override {
    PlatformTest::SetUp();

    cus_ = std::make_unique<component_updater::MockComponentUpdateService>();
    installer_ =
        std::make_unique<HistorySearchStringsComponentInstallerPolicyPublic>();
    ASSERT_TRUE(listener()->filter_words_hashes().empty());
  }

  void TearDown() override {
    PlatformTest::TearDown();

    listener()->ResetForTesting();
  }

 protected:
  void RunUntilIdle() { task_environment_.RunUntilIdle(); }
  component_updater::MockComponentUpdateService* cus() { return cus_.get(); }
  HistorySearchStringsComponentInstallerPolicyPublic* installer() {
    return installer_.get();
  }
  history_embeddings::SearchStringsUpdateListener* listener() {
    return history_embeddings::SearchStringsUpdateListener::GetInstance();
  }

 private:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<component_updater::MockComponentUpdateService> cus_;
  std::unique_ptr<HistorySearchStringsComponentInstallerPolicyPublic>
      installer_;
};

TEST_F(HistorySearchStringsComponentInstallerPolicyTest,
       ComponentRegistration) {
  EXPECT_CALL(*cus(), RegisterComponent(_)).Times(1).WillOnce(Return(true));
  RegisterHistorySearchStringsComponent(cus());
  RunUntilIdle();
}

TEST_F(HistorySearchStringsComponentInstallerPolicyTest, BadBinaryProtoFile) {
  base::ScopedTempDir install_dir;
  ASSERT_TRUE(install_dir.CreateUniqueTempDir());
  ASSERT_TRUE(base::WriteFile(
      install_dir.GetPath().AppendASCII(kHistorySearchStringsBinaryPbFileName),
      "foobar"));

  base::Value::Dict manifest;
  ASSERT_TRUE(installer()->VerifyInstallation(manifest, install_dir.GetPath()));

  installer()->ComponentReady(base::Version("1.2.3"), install_dir.GetPath(),
                              std::move(manifest));
  RunUntilIdle();
  ASSERT_TRUE(listener()->filter_words_hashes().empty());
}

TEST_F(HistorySearchStringsComponentInstallerPolicyTest, LoadBinaryProtoFile) {
  optimization_guide::proto::HistorySearchStrings proto;
  proto.add_filter_words("3962775614");
  proto.add_filter_words("4220142007");
  proto.add_filter_words("430397466");
  std::string file_content;
  proto.SerializeToString(&file_content);

  base::ScopedTempDir install_dir;
  ASSERT_TRUE(install_dir.CreateUniqueTempDir());
  ASSERT_TRUE(base::WriteFile(
      install_dir.GetPath().AppendASCII(kHistorySearchStringsBinaryPbFileName),
      file_content));

  base::Value::Dict manifest;
  ASSERT_TRUE(installer()->VerifyInstallation(manifest, install_dir.GetPath()));

  installer()->ComponentReady(base::Version("1.2.3"), install_dir.GetPath(),
                              std::move(manifest));
  RunUntilIdle();
  ASSERT_EQ(listener()->filter_words_hashes(),
            std::unordered_set<uint32_t>({3962775614, 4220142007, 430397466}));
}

}  // namespace component_updater
