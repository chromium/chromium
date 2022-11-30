// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_creation/notes/core/templates/template_fetcher.h"

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content_creation {

namespace {
const std::string kFakeContent = "test";
}  // namespace

class TemplateFetcherTest : public testing::Test {
 public:
  TemplateFetcherTest() = default;
  TemplateFetcherTest(const TemplateFetcherTest&) = delete;
  TemplateFetcherTest& operator=(const TemplateFetcherTest&) = delete;
  ~TemplateFetcherTest() override = default;

  void SetUp() override {
    test_url_loader_factory_.AddResponse(kTemplateUrl, kFakeContent,
                                         net::HTTP_OK);
  }

  void GetData(std::string response_body) { test_data_ = response_body; }

 protected:
  scoped_refptr<network::SharedURLLoaderFactory> test_url_loader_factory() {
    return test_url_loader_factory_.GetSafeWeakWrapper();
  }
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::string test_data_;
  network::TestURLLoaderFactory test_url_loader_factory_;
};

TEST_F(TemplateFetcherTest, GetRequest) {
  std::unique_ptr<TemplateFetcher> tf =
      std::make_unique<TemplateFetcher>(test_url_loader_factory());

  tf->Start(
      base::BindOnce(&TemplateFetcherTest::GetData, base::Unretained(this)));

  // Needs to wait for the kTemplateUrl to give a response before we test.
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(test_data_, kFakeContent);
}
}  // namespace content_creation
