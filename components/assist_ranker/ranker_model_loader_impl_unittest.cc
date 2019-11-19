// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/assist_ranker/ranker_model_loader_impl.h"

#include <initializer_list>
#include <memory>
#include <vector>

#include "base/bind.h"
#include "base/containers/circular_deque.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/ref_counted.h"
#include "base/strings/stringprintf.h"
#include "base/task/post_task.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/assist_ranker/proto/ranker_model.pb.h"
#include "components/assist_ranker/proto/translate_ranker_model.pb.h"
#include "components/assist_ranker/ranker_model.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using assist_ranker::RankerModel;
using assist_ranker::RankerModelLoaderImpl;
using assist_ranker::RankerModelStatus;

const char kInvalidModelData[] = "not a valid model";
const int kInvalidModelSize = sizeof(kInvalidModelData) - 1;

class RankerModelLoaderImplTest : public ::testing::Test {
 protected:
  RankerModelLoaderImplTest();

  void SetUp() override;

  // Returns a copy of |model|.
  static std::unique_ptr<RankerModel> Clone(const RankerModel& model);

  // Returns true if |m1| and |m2| are identical.
  static bool IsEqual(const RankerModel& m1, const RankerModel& m2);

  // Returns true if |m1| and |m2| are identical modulo metadata.
  static bool IsEquivalent(const RankerModel& m1, const RankerModel& m2);

  // Helper method to drive the loader for |model_path| and |model_url|.
  bool DoLoaderTest(const base::FilePath& model_path, const GURL& model_url);

  // Initialize the "remote" model data used for testing.
  void InitRemoteModels();

  // Initialize the "local" model data used for testing.
  void InitLocalModels();

  // Helper method used by InitRemoteModels() and InitLocalModels().
  void InitModel(const GURL& model_url,
                 const base::Time& last_modified,
                 const base::TimeDelta& cache_duration,
                 RankerModel* model);

  // Save |model| to |model_path|.  Used by InitRemoteModels() and
  // InitLocalModels()
  void SaveModel(const RankerModel& model, const base::FilePath& model_path);

  // Implements RankerModelLoaderImpl's ValidateModelCallback interface.
  RankerModelStatus ValidateModel(const RankerModel& model);

  // Implements RankerModelLoaderImpl's OnModelAvailableCallback interface.
  void OnModelAvailable(std::unique_ptr<RankerModel> model);

  // Sets up the task scheduling/task-runner environment for each test.
  base::test::TaskEnvironment task_environment_;

  // Override the default URL loader to return custom responses for tests.
  network::TestURLLoaderFactory test_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;

  // Temporary directory for model files.
  base::ScopedTempDir scoped_temp_dir_;

  // A queue of responses to return from Validate(). If empty, validate will
  // return 'OK'.
  base::circular_deque<RankerModelStatus> validate_model_response_;

  // A cached to remember the model validation calls.
  std::vector<std::unique_ptr<RankerModel>> validated_models_;

  // A cache to remember the OnModelAvailable calls.
  std::vector<std::unique_ptr<RankerModel>> available_models_;

  // Cached model file paths.
  base::FilePath local_model_path_;
  base::FilePath expired_model_path_;
  base::FilePath invalid_model_path_;

  // Model URLS.
  GURL remote_model_url_;
  GURL invalid_model_url_;
  GURL failed_model_url_;

  // Model Data.
  RankerModel remote_model_;
  RankerModel local_model_;
  RankerModel expired_model_;

 private:
  DISALLOW_COPY_AND_ASSIGN(RankerModelLoaderImplTest);
};

RankerModelLoaderImplTest::RankerModelLoaderImplTest() {
  test_shared_loader_factory_ =
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          &test_loader_factory_);
}

void RankerModelLoaderImplTest::SetUp() {
  ASSERT_TRUE(scoped_temp_dir_.CreateUniqueTempDir());
  const auto& temp_dir_path = scoped_temp_dir_.GetPath();

  // Setup the model file paths.
  local_model_path_ = temp_dir_path.AppendASCII("local_model.bin");
  expired_model_path_ = temp_dir_path.AppendASCII("expired_model.bin");
  invalid_model_path_ = temp_dir_path.AppendASCII("invalid_model.bin");

  // Setup the model URLs.
  remote_model_url_ = GURL("https://some.url.net/good.model.bin");
  invalid_model_url_ = GURL("https://some.url.net/bad.model.bin");
  failed_model_url_ = GURL("https://some.url.net/fail");

  // Initialize the model data.
  ASSERT_NO_FATAL_FAILURE(InitRemoteModels());
  ASSERT_NO_FATAL_FAILURE(InitLocalModels());
}

// static
std::unique_ptr<RankerModel> RankerModelLoaderImplTest::Clone(
    const RankerModel& model) {
  auto copy = std::make_unique<RankerModel>();
  *copy->mutable_proto() = model.proto();
  return copy;
}

// static
bool RankerModelLoaderImplTest::IsEqual(const RankerModel& m1,
                                        const RankerModel& m2) {
  return m1.SerializeAsString() == m2.SerializeAsString();
}

// static
bool RankerModelLoaderImplTest::IsEquivalent(const RankerModel& m1,
                                             const RankerModel& m2) {
  auto copy_m1 = Clone(m1);
  copy_m1->mutable_proto()->mutable_metadata()->Clear();

  auto copy_m2 = Clone(m2);
  copy_m2->mutable_proto()->mutable_metadata()->Clear();

  return IsEqual(*copy_m1, *copy_m2);
}

bool RankerModelLoaderImplTest::DoLoaderTest(const base::FilePath& model_path,
                                             const GURL& model_url) {
  auto loader = std::make_unique<RankerModelLoaderImpl>(
      base::Bind(&RankerModelLoaderImplTest::ValidateModel,
                 base::Unretained(this)),
      base::Bind(&RankerModelLoaderImplTest::OnModelAvailable,
                 base::Unretained(this)),
      test_shared_loader_factory_, model_path, model_url,
      "RankerModelLoaderImplTest");
  loader->NotifyOfRankerActivity();
  task_environment_.RunUntilIdle();

  return true;
}

void RankerModelLoaderImplTest::InitRemoteModels() {
  InitModel(remote_model_url_, base::Time(), base::TimeDelta(), &remote_model_);
  test_loader_factory_.AddResponse(remote_model_url_.spec(),
                                   remote_model_.SerializeAsString());
  test_loader_factory_.AddResponse(invalid_model_url_.spec(),
                                   kInvalidModelData);
  test_loader_factory_.AddResponse(
      failed_model_url_, network::mojom::URLResponseHead::New(), "",
      network::URLLoaderCompletionStatus(net::HTTP_INTERNAL_SERVER_ERROR));
}

void RankerModelLoaderImplTest::InitLocalModels() {
  InitModel(remote_model_url_, base::Time::Now(), base::TimeDelta::FromDays(30),
            &local_model_);
  InitModel(remote_model_url_,
            base::Time::Now() - base::TimeDelta::FromDays(60),
            base::TimeDelta::FromDays(30), &expired_model_);
  SaveModel(local_model_, local_model_path_);
  SaveModel(expired_model_, expired_model_path_);
  ASSERT_EQ(base::WriteFile(invalid_model_path_, kInvalidModelData,
                            kInvalidModelSize),
            kInvalidModelSize);
}

void RankerModelLoaderImplTest::InitModel(const GURL& model_url,
                                          const base::Time& last_modified,
                                          const base::TimeDelta& cache_duration,
                                          RankerModel* model) {
  ASSERT_TRUE(model != nullptr);
  model->mutable_proto()->Clear();

  auto* metadata = model->mutable_proto()->mutable_metadata();
  if (!model_url.is_empty())
    metadata->set_source(model_url.spec());
  if (!last_modified.is_null()) {
    auto last_modified_sec = (last_modified - base::Time()).InSeconds();
    metadata->set_last_modified_sec(last_modified_sec);
  }
  if (!cache_duration.is_zero())
    metadata->set_cache_duration_sec(cache_duration.InSeconds());

  auto* translate = model->mutable_proto()->mutable_translate();
  translate->set_version(1);

  auto* logit = translate->mutable_translate_logistic_regression_model();
  logit->set_bias(0.1f);
  logit->set_accept_ratio_weight(0.2f);
  logit->set_decline_ratio_weight(0.3f);
  logit->set_ignore_ratio_weight(0.4f);
}

void RankerModelLoaderImplTest::SaveModel(const RankerModel& model,
                                          const base::FilePath& model_path) {
  std::string model_str = model.SerializeAsString();
  ASSERT_EQ(base::WriteFile(model_path, model_str.data(), model_str.size()),
            static_cast<int>(model_str.size()));
}

RankerModelStatus RankerModelLoaderImplTest::ValidateModel(
    const RankerModel& model) {
  validated_models_.push_back(Clone(model));
  RankerModelStatus response = RankerModelStatus::OK;
  if (!validate_model_response_.empty()) {
    response = validate_model_response_.front();
    validate_model_response_.pop_front();
  }
  return response;
}

void RankerModelLoaderImplTest::OnModelAvailable(
    std::unique_ptr<RankerModel> model) {
  available_models_.push_back(std::move(model));
}

}  // namespace

TEST_F(RankerModelLoaderImplTest, NoLocalOrRemoteModel) {
  ASSERT_TRUE(DoLoaderTest(base::FilePath(), GURL()));

  EXPECT_EQ(0U, validated_models_.size());
  EXPECT_EQ(0U, available_models_.size());
}

TEST_F(RankerModelLoaderImplTest, BadLocalAndRemoteModel) {
  ASSERT_TRUE(DoLoaderTest(invalid_model_path_, invalid_model_url_));

  EXPECT_EQ(0U, validated_models_.size());
  EXPECT_EQ(0U, available_models_.size());
}

TEST_F(RankerModelLoaderImplTest, LoadFromFileOnly) {
  EXPECT_TRUE(DoLoaderTest(local_model_path_, GURL()));

  ASSERT_EQ(1U, validated_models_.size());
  ASSERT_EQ(1U, available_models_.size());
  EXPECT_TRUE(IsEqual(*validated_models_[0], local_model_));
  EXPECT_TRUE(IsEqual(*available_models_[0], local_model_));
}

TEST_F(RankerModelLoaderImplTest, LoadFromFileSkipsDownload) {
  ASSERT_TRUE(DoLoaderTest(local_model_path_, remote_model_url_));

  ASSERT_EQ(1U, validated_models_.size());
  ASSERT_EQ(1U, available_models_.size());
  EXPECT_TRUE(IsEqual(*validated_models_[0], local_model_));
  EXPECT_TRUE(IsEqual(*available_models_[0], local_model_));
}

TEST_F(RankerModelLoaderImplTest, LoadFromFileAndBadUrl) {
  ASSERT_TRUE(DoLoaderTest(local_model_path_, invalid_model_url_));
  ASSERT_EQ(1U, validated_models_.size());
  ASSERT_EQ(1U, available_models_.size());
  EXPECT_TRUE(IsEqual(*validated_models_[0], local_model_));
  EXPECT_TRUE(IsEqual(*available_models_[0], local_model_));
}

TEST_F(RankerModelLoaderImplTest, LoadFromURLOnly) {
  ASSERT_TRUE(DoLoaderTest(base::FilePath(), remote_model_url_));
  ASSERT_EQ(1U, validated_models_.size());
  ASSERT_EQ(1U, available_models_.size());
  EXPECT_TRUE(IsEquivalent(*validated_models_[0], remote_model_));
  EXPECT_TRUE(IsEquivalent(*available_models_[0], remote_model_));
}

TEST_F(RankerModelLoaderImplTest, LoadFromExpiredFileTriggersDownload) {
  ASSERT_TRUE(DoLoaderTest(expired_model_path_, remote_model_url_));
  ASSERT_EQ(2U, validated_models_.size());
  ASSERT_EQ(2U, available_models_.size());
  EXPECT_TRUE(IsEquivalent(*validated_models_[0], local_model_));
  EXPECT_TRUE(IsEquivalent(*available_models_[0], local_model_));
  EXPECT_TRUE(IsEquivalent(*validated_models_[1], remote_model_));
  EXPECT_TRUE(IsEquivalent(*available_models_[1], remote_model_));
}

TEST_F(RankerModelLoaderImplTest, LoadFromBadFileTriggersDownload) {
  ASSERT_TRUE(DoLoaderTest(invalid_model_path_, remote_model_url_));
  ASSERT_EQ(1U, validated_models_.size());
  ASSERT_EQ(1U, available_models_.size());
  EXPECT_TRUE(IsEquivalent(*validated_models_[0], remote_model_));
  EXPECT_TRUE(IsEquivalent(*available_models_[0], remote_model_));
}

TEST_F(RankerModelLoaderImplTest, IncompatibleCachedFileTriggersDownload) {
  validate_model_response_.push_back(RankerModelStatus::INCOMPATIBLE);

  ASSERT_TRUE(DoLoaderTest(local_model_path_, remote_model_url_));
  ASSERT_EQ(2U, validated_models_.size());
  ASSERT_EQ(1U, available_models_.size());
  EXPECT_TRUE(IsEquivalent(*validated_models_[0], local_model_));
  EXPECT_TRUE(IsEquivalent(*validated_models_[1], remote_model_));
  EXPECT_TRUE(IsEquivalent(*available_models_[0], remote_model_));
}

TEST_F(RankerModelLoaderImplTest, IncompatibleDownloadedFileKeepsExpired) {
  validate_model_response_.push_back(RankerModelStatus::OK);
  validate_model_response_.push_back(RankerModelStatus::INCOMPATIBLE);

  ASSERT_TRUE(DoLoaderTest(expired_model_path_, remote_model_url_));
  ASSERT_EQ(2U, validated_models_.size());
  ASSERT_EQ(1U, available_models_.size());
  EXPECT_TRUE(IsEquivalent(*validated_models_[0], local_model_));
  EXPECT_TRUE(IsEquivalent(*validated_models_[1], remote_model_));
  EXPECT_TRUE(IsEquivalent(*available_models_[0], local_model_));
}
