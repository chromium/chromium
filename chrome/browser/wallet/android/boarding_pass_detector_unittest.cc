// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/wallet/android/boarding_pass_detector.h"
#include "chrome/common/wallet/boarding_pass_extractor.mojom.h"

#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chrome/common/chrome_features.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace wallet {

class FakeBoardingPassExtractor : public mojom::BoardingPassExtractor {
 public:
  mojo::PendingRemote<mojom::BoardingPassExtractor> BindAndPassRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  // mojom::BoardingPassExtractor
  void ExtractBoardingPass(ExtractBoardingPassCallback callback) override {
    CHECK(!HasPendingRequest());
    callback_ = std::move(callback);
    if (!signal_.IsReady()) {
      signal_.SetValue();
    }
  }

  void WaitForRequest() {
    if (HasPendingRequest()) {
      return;
    }
    signal_.Clear();
    EXPECT_TRUE(signal_.Wait());
  }

  void AnswerRequest(const std::vector<std::string>& results) {
    CHECK(HasPendingRequest());
    std::move(callback_).Run(results);
  }

  void ResetReceiver() { receiver_.reset(); }

 private:
  bool HasPendingRequest() const { return bool(callback_); }
  base::test::TestFuture<void> signal_;

  ExtractBoardingPassCallback callback_;
  mojo::Receiver<mojom::BoardingPassExtractor> receiver_{this};
};

class BoardingPassDetectorTest : public ::testing::Test {
 public:
  void SetAllowList(const std::string& allowlist) {
    feature_list_.InitAndEnableFeatureWithParameters(
        features::kBoardingPassDetector,
        {{features::kBoardingPassDetectorUrlParam.name, allowlist}});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  base::test::TaskEnvironment task_env_;
};

TEST_F(BoardingPassDetectorTest, ShouldDetect) {
  SetAllowList("https://aa.com/adc, https://www.google.com/boarding");

  EXPECT_TRUE(BoardingPassDetector::ShouldDetect("https://aa.com/adc"));
  EXPECT_TRUE(BoardingPassDetector::ShouldDetect(
      "https://www.google.com/boarding/abc"));
}

TEST_F(BoardingPassDetectorTest, ShouldNotDetect) {
  SetAllowList("https://aa.com/adc, https://www.google.com/boarding");

  EXPECT_FALSE(BoardingPassDetector::ShouldDetect("https://aa.com/"));
  EXPECT_FALSE(
      BoardingPassDetector::ShouldDetect("https://www.google.com/abc"));
}

TEST_F(BoardingPassDetectorTest, DetectBoardingPassSuccess) {
  FakeBoardingPassExtractor boarding_pass_extractor;
  base::test::TestFuture<const std::vector<std::string>&> test_future;
  BoardingPassDetector* detector = new BoardingPassDetector();
  detector->DetectBoardingPassWithRemote(
      mojo::Remote<mojom::BoardingPassExtractor>(
          boarding_pass_extractor.BindAndPassRemote()),
      test_future.GetCallback());
  boarding_pass_extractor.WaitForRequest();

  std::vector<std::string> extractor_response;
  extractor_response.push_back("test1");
  boarding_pass_extractor.AnswerRequest(extractor_response);

  auto results = test_future.Get();
  EXPECT_EQ(results.size(), extractor_response.size());
  EXPECT_EQ(results[0], extractor_response[0]);
}

TEST_F(BoardingPassDetectorTest, DetectBoardingPassDisconnect) {
  FakeBoardingPassExtractor boarding_pass_extractor;
  base::test::TestFuture<const std::vector<std::string>&> test_future;
  BoardingPassDetector* detector = new BoardingPassDetector();
  detector->DetectBoardingPassWithRemote(
      mojo::Remote<mojom::BoardingPassExtractor>(
          boarding_pass_extractor.BindAndPassRemote()),
      test_future.GetCallback());
  boarding_pass_extractor.ResetReceiver();

  auto results = test_future.Get();
  EXPECT_EQ(results.size(), 0u);
}

}  // namespace wallet
