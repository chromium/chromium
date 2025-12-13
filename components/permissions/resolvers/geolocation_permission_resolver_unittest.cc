// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/resolvers/geolocation_permission_resolver.h"

#include <memory>
#include <variant>

#include "base/test/gtest_util.h"
#include "base/values.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/permissions/resolvers/permission_prompt_options.h"
#include "components/permissions/resolvers/permission_resolver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom.h"

namespace permissions {

class GeolocationPermissionResolverTest
    : public testing::Test,
      public testing::WithParamInterface<base::Value> {
 protected:
  static std::unique_ptr<PermissionResolver> approximate_request_resolver() {
    return std::make_unique<GeolocationPermissionResolver>(
        /*requested_precise=*/false);
  }

  static std::unique_ptr<PermissionResolver> precise_request_resolver() {
    return std::make_unique<GeolocationPermissionResolver>(
        /*requested_precise=*/true);
  }
};

TEST_F(GeolocationPermissionResolverTest, TestDeterminePermissionStatusAskAsk) {
  auto setting =
      GeolocationSetting(PermissionOption::kAsk, PermissionOption::kAsk);
  EXPECT_EQ(approximate_request_resolver()->DeterminePermissionStatus(setting),
            blink::mojom::PermissionStatus::ASK);
  EXPECT_EQ(precise_request_resolver()->DeterminePermissionStatus(setting),
            blink::mojom::PermissionStatus::ASK);
}

TEST_F(GeolocationPermissionResolverTest,
       TestDeterminePermissionStatusAllowedAsk) {
  auto setting =
      GeolocationSetting(PermissionOption::kAllowed, PermissionOption::kAsk);
  EXPECT_EQ(approximate_request_resolver()->DeterminePermissionStatus(setting),
            blink::mojom::PermissionStatus::GRANTED);
  EXPECT_EQ(precise_request_resolver()->DeterminePermissionStatus(setting),
            blink::mojom::PermissionStatus::ASK);
}

TEST_F(GeolocationPermissionResolverTest,
       TestDeterminePermissionStatusAllowedAllowed) {
  auto setting = GeolocationSetting(PermissionOption::kAllowed,
                                    PermissionOption::kAllowed);

  EXPECT_EQ(approximate_request_resolver()->DeterminePermissionStatus(setting),
            blink::mojom::PermissionStatus::GRANTED);
  EXPECT_EQ(precise_request_resolver()->DeterminePermissionStatus(setting),
            blink::mojom::PermissionStatus::GRANTED);
}

TEST_F(GeolocationPermissionResolverTest,
       TestDeterminePermissionStatusAllowBlock) {
  auto setting =
      GeolocationSetting(PermissionOption::kAllowed, PermissionOption::kDenied);

  EXPECT_EQ(approximate_request_resolver()->DeterminePermissionStatus(setting),
            blink::mojom::PermissionStatus::GRANTED);
  EXPECT_EQ(precise_request_resolver()->DeterminePermissionStatus(setting),
            blink::mojom::PermissionStatus::GRANTED);
}

TEST_F(GeolocationPermissionResolverTest,
       TestDeterminePermissionStatusDeniedDenied) {
  auto setting =
      GeolocationSetting(PermissionOption::kDenied, PermissionOption::kDenied);

  EXPECT_EQ(approximate_request_resolver()->DeterminePermissionStatus(setting),
            blink::mojom::PermissionStatus::DENIED);
  EXPECT_EQ(precise_request_resolver()->DeterminePermissionStatus(setting),
            blink::mojom::PermissionStatus::DENIED);
}

TEST_F(GeolocationPermissionResolverTest,
       TestComputePermissionDecisionResultAskAsk) {
  auto previous_setting =
      GeolocationSetting(PermissionOption::kAsk, PermissionOption::kAsk);

  EXPECT_EQ(
      std::get<GeolocationSetting>(
          approximate_request_resolver()->ComputePermissionDecisionResult(
              previous_setting, PermissionDecision::kAllow, std::monostate())),
      GeolocationSetting(PermissionOption::kAllowed, PermissionOption::kAsk));

  EXPECT_EQ(
      std::get<GeolocationSetting>(
          approximate_request_resolver()->ComputePermissionDecisionResult(
              previous_setting, PermissionDecision::kAllowThisTime,
              std::monostate())),
      GeolocationSetting(PermissionOption::kAllowed, PermissionOption::kAsk));

  EXPECT_EQ(
      std::get<GeolocationSetting>(
          approximate_request_resolver()->ComputePermissionDecisionResult(
              previous_setting, PermissionDecision::kDeny, std::monostate())),
      GeolocationSetting(PermissionOption::kDenied, PermissionOption::kDenied));

  EXPECT_EQ(
      std::get<GeolocationSetting>(
          precise_request_resolver()->ComputePermissionDecisionResult(
              previous_setting, PermissionDecision::kAllow,
              GeolocationPromptOptions{
                  /*selected_accuracy=*/GeolocationAccuracy::kApproximate})),
      GeolocationSetting(PermissionOption::kAllowed,
                         PermissionOption::kDenied));

  EXPECT_EQ(
      std::get<GeolocationSetting>(
          precise_request_resolver()->ComputePermissionDecisionResult(
              previous_setting, PermissionDecision::kAllowThisTime,
              GeolocationPromptOptions{
                  /*selected_accuracy=*/GeolocationAccuracy::kApproximate})),
      GeolocationSetting(PermissionOption::kAllowed,
                         PermissionOption::kDenied));

  EXPECT_EQ(
      std::get<GeolocationSetting>(
          precise_request_resolver()->ComputePermissionDecisionResult(
              previous_setting, PermissionDecision::kDeny,
              GeolocationPromptOptions{
                  /*selected_accuracy=*/GeolocationAccuracy::kApproximate})),
      GeolocationSetting(PermissionOption::kDenied, PermissionOption::kDenied));

  EXPECT_EQ(std::get<GeolocationSetting>(
                precise_request_resolver()->ComputePermissionDecisionResult(
                    previous_setting, PermissionDecision::kAllow,
                    GeolocationPromptOptions{
                        /*selected_accuracy=*/GeolocationAccuracy::kPrecise})),
            GeolocationSetting(PermissionOption::kAllowed,
                               PermissionOption::kAllowed));

  EXPECT_EQ(std::get<GeolocationSetting>(
                precise_request_resolver()->ComputePermissionDecisionResult(
                    previous_setting, PermissionDecision::kAllowThisTime,
                    GeolocationPromptOptions{
                        /*selected_accuracy=*/GeolocationAccuracy::kPrecise})),
            GeolocationSetting(PermissionOption::kAllowed,
                               PermissionOption::kAllowed));

  EXPECT_EQ(
      std::get<GeolocationSetting>(
          precise_request_resolver()->ComputePermissionDecisionResult(
              previous_setting, PermissionDecision::kDeny,
              GeolocationPromptOptions{
                  /*selected_accuracy=*/GeolocationAccuracy::kPrecise})),
      GeolocationSetting(PermissionOption::kDenied, PermissionOption::kDenied));
}

TEST_F(GeolocationPermissionResolverTest,
       TestComputePermissionDecisionResultAllowedAsk) {
  auto previous_setting =
      GeolocationSetting(PermissionOption::kAllowed, PermissionOption::kAsk);

  EXPECT_EQ(
      std::get<GeolocationSetting>(
          approximate_request_resolver()->ComputePermissionDecisionResult(
              previous_setting, PermissionDecision::kAllow, std::monostate())),
      GeolocationSetting(PermissionOption::kAllowed, PermissionOption::kAsk));

  EXPECT_EQ(
      std::get<GeolocationSetting>(
          approximate_request_resolver()->ComputePermissionDecisionResult(
              previous_setting, PermissionDecision::kAllowThisTime,
              std::monostate())),
      GeolocationSetting(PermissionOption::kAllowed, PermissionOption::kAsk));

  EXPECT_EQ(
      std::get<GeolocationSetting>(
          approximate_request_resolver()->ComputePermissionDecisionResult(
              previous_setting, PermissionDecision::kDeny, std::monostate())),
      GeolocationSetting(PermissionOption::kDenied, PermissionOption::kDenied));

  EXPECT_EQ(
      std::get<GeolocationSetting>(
          precise_request_resolver()->ComputePermissionDecisionResult(
              previous_setting, PermissionDecision::kAllow,
              GeolocationPromptOptions{
                  /*selected_accuracy=*/GeolocationAccuracy::kApproximate})),
      GeolocationSetting(PermissionOption::kAllowed,
                         PermissionOption::kDenied));

  EXPECT_EQ(
      std::get<GeolocationSetting>(
          precise_request_resolver()->ComputePermissionDecisionResult(
              previous_setting, PermissionDecision::kAllowThisTime,
              GeolocationPromptOptions{
                  /*selected_accuracy=*/GeolocationAccuracy::kApproximate})),
      GeolocationSetting(PermissionOption::kAllowed,
                         PermissionOption::kDenied));

  EXPECT_EQ(
      std::get<GeolocationSetting>(
          precise_request_resolver()->ComputePermissionDecisionResult(
              previous_setting, PermissionDecision::kDeny,
              GeolocationPromptOptions{
                  /*selected_accuracy=*/GeolocationAccuracy::kApproximate})),
      GeolocationSetting(PermissionOption::kDenied, PermissionOption::kDenied));

  EXPECT_EQ(std::get<GeolocationSetting>(
                precise_request_resolver()->ComputePermissionDecisionResult(
                    previous_setting, PermissionDecision::kAllow,
                    GeolocationPromptOptions{
                        /*selected_accuracy=*/GeolocationAccuracy::kPrecise})),
            GeolocationSetting(PermissionOption::kAllowed,
                               PermissionOption::kAllowed));

  EXPECT_EQ(std::get<GeolocationSetting>(
                precise_request_resolver()->ComputePermissionDecisionResult(
                    previous_setting, PermissionDecision::kAllowThisTime,
                    GeolocationPromptOptions{
                        /*selected_accuracy=*/GeolocationAccuracy::kPrecise})),
            GeolocationSetting(PermissionOption::kAllowed,
                               PermissionOption::kAllowed));

  EXPECT_EQ(
      std::get<GeolocationSetting>(
          precise_request_resolver()->ComputePermissionDecisionResult(
              previous_setting, PermissionDecision::kDeny,
              GeolocationPromptOptions{
                  /*selected_accuracy=*/GeolocationAccuracy::kPrecise})),
      GeolocationSetting(PermissionOption::kDenied, PermissionOption::kDenied));
}

TEST_F(GeolocationPermissionResolverTest,
       TestComputePermissionDecisionResultAllowedDenied) {
  auto previous_setting =
      GeolocationSetting(PermissionOption::kAllowed, PermissionOption::kDenied);

  EXPECT_EQ(
      std::get<GeolocationSetting>(
          approximate_request_resolver()->ComputePermissionDecisionResult(
              previous_setting, PermissionDecision::kAllow, std::monostate())),
      GeolocationSetting(PermissionOption::kAllowed,
                         PermissionOption::kDenied));

  EXPECT_EQ(std::get<GeolocationSetting>(
                approximate_request_resolver()->ComputePermissionDecisionResult(
                    previous_setting, PermissionDecision::kAllowThisTime,
                    std::monostate())),
            GeolocationSetting(PermissionOption::kAllowed,
                               PermissionOption::kDenied));

  EXPECT_EQ(
      std::get<GeolocationSetting>(
          approximate_request_resolver()->ComputePermissionDecisionResult(
              previous_setting, PermissionDecision::kDeny, std::monostate())),
      GeolocationSetting(PermissionOption::kDenied, PermissionOption::kDenied));

  EXPECT_EQ(
      std::get<GeolocationSetting>(
          precise_request_resolver()->ComputePermissionDecisionResult(
              previous_setting, PermissionDecision::kAllow, std::monostate())),
      GeolocationSetting(PermissionOption::kAllowed,
                         PermissionOption::kDenied));

  EXPECT_EQ(
      std::get<GeolocationSetting>(
          precise_request_resolver()->ComputePermissionDecisionResult(
              previous_setting, PermissionDecision::kAllow,
              GeolocationPromptOptions{
                  /*selected_accuracy=*/GeolocationAccuracy::kApproximate})),
      GeolocationSetting(PermissionOption::kAllowed,
                         PermissionOption::kDenied));

  EXPECT_EQ(
      std::get<GeolocationSetting>(
          precise_request_resolver()->ComputePermissionDecisionResult(
              previous_setting, PermissionDecision::kAllowThisTime,
              GeolocationPromptOptions{
                  /*selected_accuracy=*/GeolocationAccuracy::kApproximate})),
      GeolocationSetting(PermissionOption::kAllowed,
                         PermissionOption::kDenied));

  EXPECT_EQ(
      std::get<GeolocationSetting>(
          precise_request_resolver()->ComputePermissionDecisionResult(
              previous_setting, PermissionDecision::kDeny,
              GeolocationPromptOptions{
                  /*selected_accuracy=*/GeolocationAccuracy::kApproximate})),
      GeolocationSetting(PermissionOption::kDenied, PermissionOption::kDenied));

  EXPECT_EQ(std::get<GeolocationSetting>(
                precise_request_resolver()->ComputePermissionDecisionResult(
                    previous_setting, PermissionDecision::kAllow,
                    GeolocationPromptOptions{
                        /*selected_accuracy=*/GeolocationAccuracy::kPrecise})),
            GeolocationSetting(PermissionOption::kAllowed,
                               PermissionOption::kAllowed));

  EXPECT_EQ(std::get<GeolocationSetting>(
                precise_request_resolver()->ComputePermissionDecisionResult(
                    previous_setting, PermissionDecision::kAllowThisTime,
                    GeolocationPromptOptions{
                        /*selected_accuracy=*/GeolocationAccuracy::kPrecise})),
            GeolocationSetting(PermissionOption::kAllowed,
                               PermissionOption::kAllowed));

  EXPECT_EQ(
      std::get<GeolocationSetting>(
          precise_request_resolver()->ComputePermissionDecisionResult(
              previous_setting, PermissionDecision::kDeny,
              GeolocationPromptOptions{
                  /*selected_accuracy=*/GeolocationAccuracy::kPrecise})),
      GeolocationSetting(PermissionOption::kDenied, PermissionOption::kDenied));
}

TEST_F(GeolocationPermissionResolverTest,
       TestComputePermissionDecisionResultDeniedDenied) {
  auto previous_setting =
      GeolocationSetting(PermissionOption::kDenied, PermissionOption::kDenied);

  EXPECT_EQ(
      std::get<GeolocationSetting>(
          approximate_request_resolver()->ComputePermissionDecisionResult(
              previous_setting, PermissionDecision::kAllow, std::monostate())),
      GeolocationSetting(PermissionOption::kAllowed,
                         PermissionOption::kDenied));

  EXPECT_EQ(std::get<GeolocationSetting>(
                approximate_request_resolver()->ComputePermissionDecisionResult(
                    previous_setting, PermissionDecision::kAllowThisTime,
                    std::monostate())),
            GeolocationSetting(PermissionOption::kAllowed,
                               PermissionOption::kDenied));

  EXPECT_EQ(
      std::get<GeolocationSetting>(
          approximate_request_resolver()->ComputePermissionDecisionResult(
              previous_setting, PermissionDecision::kDeny, std::monostate())),
      GeolocationSetting(PermissionOption::kDenied, PermissionOption::kDenied));

  EXPECT_EQ(
      std::get<GeolocationSetting>(
          precise_request_resolver()->ComputePermissionDecisionResult(
              previous_setting, PermissionDecision::kAllow,
              GeolocationPromptOptions{
                  /*selected_accuracy=*/GeolocationAccuracy::kApproximate})),
      GeolocationSetting(PermissionOption::kAllowed,
                         PermissionOption::kDenied));

  EXPECT_EQ(
      std::get<GeolocationSetting>(
          precise_request_resolver()->ComputePermissionDecisionResult(
              previous_setting, PermissionDecision::kAllowThisTime,
              GeolocationPromptOptions{
                  /*selected_accuracy=*/GeolocationAccuracy::kApproximate})),
      GeolocationSetting(PermissionOption::kAllowed,
                         PermissionOption::kDenied));

  EXPECT_EQ(
      std::get<GeolocationSetting>(
          precise_request_resolver()->ComputePermissionDecisionResult(
              previous_setting, PermissionDecision::kDeny,
              GeolocationPromptOptions{
                  /*selected_accuracy=*/GeolocationAccuracy::kApproximate})),
      GeolocationSetting(PermissionOption::kDenied, PermissionOption::kDenied));

  EXPECT_EQ(std::get<GeolocationSetting>(
                precise_request_resolver()->ComputePermissionDecisionResult(
                    previous_setting, PermissionDecision::kAllow,
                    GeolocationPromptOptions{
                        /*selected_accuracy=*/GeolocationAccuracy::kPrecise})),
            GeolocationSetting(PermissionOption::kAllowed,
                               PermissionOption::kAllowed));

  EXPECT_EQ(std::get<GeolocationSetting>(
                precise_request_resolver()->ComputePermissionDecisionResult(
                    previous_setting, PermissionDecision::kAllowThisTime,
                    GeolocationPromptOptions{
                        /*selected_accuracy=*/GeolocationAccuracy::kPrecise})),
            GeolocationSetting(PermissionOption::kAllowed,
                               PermissionOption::kAllowed));

  EXPECT_EQ(
      std::get<GeolocationSetting>(
          precise_request_resolver()->ComputePermissionDecisionResult(
              previous_setting, PermissionDecision::kDeny,
              GeolocationPromptOptions{
                  /*selected_accuracy=*/GeolocationAccuracy::kPrecise})),
      GeolocationSetting(PermissionOption::kDenied, PermissionOption::kDenied));
}

}  // namespace permissions
