// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/browser/client_side_model_loader.h"

#include <stdint.h>

#include <map>
#include <memory>
#include <string>

#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/metrics/field_trial.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "components/safe_browsing/buildflags.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/proto/client_model.pb.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "components/variations/variations_associated_data.h"
#include "content/public/test/browser_task_environment.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using ::testing::_;
using ::testing::Invoke;
using ::testing::Mock;
using ::testing::StrictMock;

namespace safe_browsing {
namespace {

class MockModelLoader : public ModelLoader {
 public:
  MockModelLoader(
      base::RepeatingClosure update_renderers_callback,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const std::string model_name)
      : ModelLoader(update_renderers_callback, url_loader_factory, model_name) {
  }
  ~MockModelLoader() override {}

  MOCK_METHOD1(ScheduleFetch, void(int64_t));
  MOCK_METHOD2(EndFetch, void(ClientModelStatus, base::TimeDelta));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockModelLoader);
};

}  // namespace

class ModelLoaderTest : public testing::Test {
 protected:
  ModelLoaderTest()
      : shared_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)) {
    scoped_feature_list_.Init();
  }

  void SetUp() override {
    variations::testing::ClearAllVariationIDs();
    variations::testing::ClearAllVariationParams();
  }

  // Set up the finch experiment to control the model number
  // used in the model URL. This clears all existing state.
  void SetFinchModelNumber(int model_number) {
#if BUILDFLAG(FULL_SAFE_BROWSING)
    scoped_feature_list_.Reset();
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        kClientSideDetectionModelVersion,
        {{ ModelLoader::kClientModelFinchParam,
           base::NumberToString(model_number) }});
#else
    scoped_feature_list_.Reset();
    scoped_feature_list_.Init();
    variations::testing::ClearAllVariationIDs();
    variations::testing::ClearAllVariationParams();

    const std::string group_name = "ModelFoo";  // Not used in CSD code.
    ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial(
        ModelLoader::kClientModelFinchExperiment, group_name));

    std::map<std::string, std::string> params;
    params[ModelLoader::kClientModelFinchParam] =
        base::NumberToString(model_number);

    ASSERT_TRUE(variations::AssociateVariationParams(
        ModelLoader::kClientModelFinchExperiment, group_name, params));
#endif
  }

  // Set the URL for future SetModelFetchResponse() calls.
  void SetModelUrl(const ModelLoader& loader) { model_url_ = loader.url_; }

  void SetModelFetchResponse(std::string response_data, int net_error) {
    CHECK(model_url_.is_valid());
    if (net_error != net::OK) {
      network::URLLoaderCompletionStatus status;
      test_url_loader_factory_.AddResponse(
          model_url_, network::mojom::URLResponseHead::New(), std::string(),
          network::URLLoaderCompletionStatus(net_error));
      return;
    }
    test_url_loader_factory_.AddResponse(model_url_.spec(), response_data);
  }

  scoped_refptr<network::SharedURLLoaderFactory> shared_loader_factory() {
    return shared_loader_factory_;
  }

  network::TestURLLoaderFactory* test_url_loader_factory() {
    return &test_url_loader_factory_;
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_loader_factory_;
  GURL model_url_;
};

ACTION_P(InvokeClosure, closure) {
  closure.Run();
}

TEST_F(ModelLoaderTest, FetchModelFromLocalFileTest) {
  StrictMock<MockModelLoader> loader(base::RepeatingClosure(),
                                     shared_loader_factory(), "top_model.pb");
  SetModelUrl(loader);

  // The model fetch tries to read from local file but is empty.
  {
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        "csd-model-override-path", "");
    loader.StartFetch(/*only_from_cache=*/false);
    Mock::VerifyAndClearExpectations(&loader);
  }

  // The model fetch tries to read from invalid local file.
  {
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        "csd-model-override-path", "invalid-file");
    loader.StartFetch(/*only_from_cache=*/false);
    Mock::VerifyAndClearExpectations(&loader);
  }

  // The model fetch tries to read from local file with invalid model data.
  {
    base::ScopedTempDir test_path;
    ASSERT_TRUE(test_path.CreateUniqueTempDir());
    ClientSideModel model;
    model.set_max_words_per_term(4);
    ASSERT_TRUE(base::WriteFile(test_path.GetPath().AppendASCII("model.txt"),
                                model.SerializeAsString()));
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        "csd-model-override-path",
        test_path.GetPath().AppendASCII("model.txt").MaybeAsASCII());
    loader.StartFetch(/*only_from_cache=*/false);
    Mock::VerifyAndClearExpectations(&loader);
  }

  // The model fetch tries to read from local file with valid model data.
  {
    base::ScopedTempDir test_path;
    ASSERT_TRUE(test_path.CreateUniqueTempDir());
    base::RunLoop loop;
    ClientSideModel model;
    model.set_version(10);
    model.set_max_words_per_term(4);
    EXPECT_EQ(static_cast<int>(model.SerializeAsString().size()),
              base::WriteFile(test_path.GetPath().AppendASCII("model.txt"),
                              model.SerializeAsString().c_str(),
                              model.SerializeAsString().size()));

    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        "csd-model-override-path",
        test_path.GetPath().AppendASCII("model.txt").MaybeAsASCII());
    EXPECT_CALL(loader, EndFetch(ModelLoader::MODEL_SUCCESS, _))
        .WillOnce(InvokeClosure(loop.QuitClosure()));
    loader.StartFetch(/*only_from_cache=*/false);
    loop.Run();
    Mock::VerifyAndClearExpectations(&loader);
  }
}

// Test the response to many variations of model responses.
TEST_F(ModelLoaderTest, FetchModelTest) {
  StrictMock<MockModelLoader> loader(base::RepeatingClosure(),
                                     shared_loader_factory(), "top_model.pb");
  SetModelUrl(loader);

  // The model fetch failed.
  {
    base::RunLoop loop;
    SetModelFetchResponse("blamodel", net::ERR_FAILED);
    EXPECT_CALL(loader, EndFetch(ModelLoader::MODEL_FETCH_FAILED, _))
        .WillOnce(InvokeClosure(loop.QuitClosure()));
    loader.StartFetch(/*only_from_cache=*/false);
    loop.Run();
    Mock::VerifyAndClearExpectations(&loader);
  }

  // Empty model file.
  {
    base::RunLoop loop;
    SetModelFetchResponse(std::string(), net::OK);
    EXPECT_CALL(loader, EndFetch(ModelLoader::MODEL_EMPTY, _))
        .WillOnce(InvokeClosure(loop.QuitClosure()));
    loader.StartFetch(/*only_from_cache=*/false);
    loop.Run();
    Mock::VerifyAndClearExpectations(&loader);
  }

  // Model is too large.
  {
    base::RunLoop loop;
    SetModelFetchResponse(std::string(ModelLoader::kMaxModelSizeBytes + 1, 'x'),
                          net::OK);
    EXPECT_CALL(loader, EndFetch(ModelLoader::MODEL_TOO_LARGE, _))
        .WillOnce(InvokeClosure(loop.QuitClosure()));
    loader.StartFetch(/*only_from_cache=*/false);
    loop.Run();
    Mock::VerifyAndClearExpectations(&loader);
  }

  // Unable to parse the model file.
  {
    base::RunLoop loop;
    SetModelFetchResponse("Invalid model file", net::OK);
    EXPECT_CALL(loader, EndFetch(ModelLoader::MODEL_PARSE_ERROR, _))
        .WillOnce(InvokeClosure(loop.QuitClosure()));
    loader.StartFetch(/*only_from_cache=*/false);
    loop.Run();
    Mock::VerifyAndClearExpectations(&loader);
  }

  // Model that is missing some required fields (missing the version field).
  ClientSideModel model;
  model.set_max_words_per_term(4);
  {
    base::RunLoop loop;
    SetModelFetchResponse(model.SerializePartialAsString(), net::OK);
    EXPECT_CALL(loader, EndFetch(ModelLoader::MODEL_MISSING_FIELDS, _))
        .WillOnce(InvokeClosure(loop.QuitClosure()));
    loader.StartFetch(/*only_from_cache=*/false);
    loop.Run();
    Mock::VerifyAndClearExpectations(&loader);
  }

  // Model that points to hashes that don't exist.
  model.set_version(10);
  model.add_hashes("bla");
  model.add_page_term(1);  // Should be 0 instead of 1.
  {
    base::RunLoop loop;
    SetModelFetchResponse(model.SerializePartialAsString(), net::OK);
    EXPECT_CALL(loader, EndFetch(ModelLoader::MODEL_BAD_HASH_IDS, _))
        .WillOnce(InvokeClosure(loop.QuitClosure()));
    loader.StartFetch(/*only_from_cache=*/false);
    loop.Run();
    Mock::VerifyAndClearExpectations(&loader);
  }
  model.set_page_term(0, 0);

  // Model version number is wrong.
  model.set_version(-1);
  {
    base::RunLoop loop;
    SetModelFetchResponse(model.SerializeAsString(), net::OK);
    EXPECT_CALL(loader, EndFetch(ModelLoader::MODEL_INVALID_VERSION_NUMBER, _))
        .WillOnce(InvokeClosure(loop.QuitClosure()));
    loader.StartFetch(/*only_from_cache=*/false);
    loop.Run();
    Mock::VerifyAndClearExpectations(&loader);
  }

  // Normal model.
  model.set_version(10);
  {
    base::RunLoop loop;
    SetModelFetchResponse(model.SerializeAsString(), net::OK);
    EXPECT_CALL(loader, EndFetch(ModelLoader::MODEL_SUCCESS, _))
        .WillOnce(InvokeClosure(loop.QuitClosure()));
    loader.StartFetch(/*only_from_cache=*/false);
    loop.Run();
    Mock::VerifyAndClearExpectations(&loader);
  }

  // Model version number is decreasing.  Set the model version number of the
  // model that is currently loaded in the loader object to 11.
  loader.model_ = std::make_unique<ClientSideModel>(model);
  loader.model_->set_version(11);
  {
    base::RunLoop loop;
    SetModelFetchResponse(model.SerializeAsString(), net::OK);
    EXPECT_CALL(loader, EndFetch(ModelLoader::MODEL_INVALID_VERSION_NUMBER, _))
        .WillOnce(InvokeClosure(loop.QuitClosure()));
    loader.StartFetch(/*only_from_cache=*/false);
    loop.Run();
    Mock::VerifyAndClearExpectations(&loader);
  }

  // Model version hasn't changed since the last reload.
  loader.model_->set_version(10);
  {
    base::RunLoop loop;
    SetModelFetchResponse(model.SerializeAsString(), net::OK);
    EXPECT_CALL(loader, EndFetch(ModelLoader::MODEL_NOT_CHANGED, _))
        .WillOnce(InvokeClosure(loop.QuitClosure()));
    loader.StartFetch(/*only_from_cache=*/false);
    loop.Run();
    Mock::VerifyAndClearExpectations(&loader);
  }
}

// Test that a successful reponse will update the renderers
TEST_F(ModelLoaderTest, UpdateRenderersTest) {
  // Use runloop for convenient callback detection.
  base::RunLoop loop;
  StrictMock<MockModelLoader> loader(loop.QuitClosure(), nullptr,
                                     "top_model.pb");
  EXPECT_CALL(loader, ScheduleFetch(_));
  loader.ModelLoader::EndFetch(ModelLoader::MODEL_SUCCESS, base::TimeDelta());
  loop.Run();
  Mock::VerifyAndClearExpectations(&loader);
}

// Test that a one fetch schedules another fetch.
TEST_F(ModelLoaderTest, RescheduleFetchTest) {
  StrictMock<MockModelLoader> loader(base::RepeatingClosure(), nullptr,
                                     "top_model.pb");

  // Zero max_age.  Uses default.
  base::TimeDelta max_age;
  EXPECT_CALL(loader, ScheduleFetch(ModelLoader::kClientModelFetchIntervalMs));
  loader.ModelLoader::EndFetch(ModelLoader::MODEL_NOT_CHANGED, max_age);
  Mock::VerifyAndClearExpectations(&loader);

  // Non-zero max_age from header.
  max_age = base::TimeDelta::FromMinutes(42);
  EXPECT_CALL(
      loader,
      ScheduleFetch(
          (max_age + base::TimeDelta::FromMinutes(1)).InMilliseconds()));
  loader.ModelLoader::EndFetch(ModelLoader::MODEL_NOT_CHANGED, max_age);
  Mock::VerifyAndClearExpectations(&loader);

  // Non-zero max_age, but failed load should use default interval.
  max_age = base::TimeDelta::FromMinutes(42);
  EXPECT_CALL(loader, ScheduleFetch(ModelLoader::kClientModelFetchIntervalMs));
  loader.ModelLoader::EndFetch(ModelLoader::MODEL_FETCH_FAILED, max_age);
  Mock::VerifyAndClearExpectations(&loader);
}

// Test that Finch params control the model names.
TEST_F(ModelLoaderTest, ModelNamesTest) {
  // Test the name-templating.
  EXPECT_EQ(ModelLoader::FillInModelName(true, 3),
            "client_model_v5_ext_variation_3.pb");
  EXPECT_EQ(ModelLoader::FillInModelName(false, 5),
            "client_model_v5_variation_5.pb");

  // No Finch setup. Should default to 9.
  std::unique_ptr<ModelLoader> loader;
  loader = std::make_unique<ModelLoader>(base::RepeatingClosure(), nullptr,
                                         false /* is_extended_reporting */);
  EXPECT_EQ(loader->name(), "client_model_v5_variation_9.pb");
  EXPECT_EQ(loader->url_.spec(),
            "https://ssl.gstatic.com/safebrowsing/csd/"
            "client_model_v5_variation_9.pb");

  // Model 1, no extended reporting.
  SetFinchModelNumber(1);
  loader =
      std::make_unique<ModelLoader>(base::RepeatingClosure(), nullptr, false);
  EXPECT_EQ(loader->name(), "client_model_v5_variation_1.pb");

  // Model 2, with extended reporting.
  SetFinchModelNumber(2);
  loader =
      std::make_unique<ModelLoader>(base::RepeatingClosure(), nullptr, true);
  EXPECT_EQ(loader->name(), "client_model_v5_ext_variation_2.pb");
}

TEST_F(ModelLoaderTest, ModelHasValidHashIds) {
  ClientSideModel model;
  EXPECT_TRUE(ModelLoader::ModelHasValidHashIds(model));
  model.add_hashes("bla");
  EXPECT_TRUE(ModelLoader::ModelHasValidHashIds(model));
  model.add_page_term(0);
  EXPECT_TRUE(ModelLoader::ModelHasValidHashIds(model));

  model.add_page_term(-1);
  EXPECT_FALSE(ModelLoader::ModelHasValidHashIds(model));
  model.set_page_term(1, 1);
  EXPECT_FALSE(ModelLoader::ModelHasValidHashIds(model));
  model.set_page_term(1, 0);
  EXPECT_TRUE(ModelLoader::ModelHasValidHashIds(model));

  // Test bad rules.
  model.add_hashes("blu");
  ClientSideModel::Rule* rule = model.add_rule();
  rule->add_feature(0);
  rule->add_feature(1);
  rule->set_weight(0.1f);
  EXPECT_TRUE(ModelLoader::ModelHasValidHashIds(model));

  rule = model.add_rule();
  rule->add_feature(0);
  rule->add_feature(1);
  rule->add_feature(-1);
  rule->set_weight(0.2f);
  EXPECT_FALSE(ModelLoader::ModelHasValidHashIds(model));

  rule->set_feature(2, 2);
  EXPECT_FALSE(ModelLoader::ModelHasValidHashIds(model));

  rule->set_feature(2, 1);
  EXPECT_TRUE(ModelLoader::ModelHasValidHashIds(model));
}

}  // namespace safe_browsing
