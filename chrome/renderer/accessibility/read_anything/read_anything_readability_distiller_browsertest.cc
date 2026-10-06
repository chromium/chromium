// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/accessibility/read_anything/read_anything_readability_distiller.h"

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/functional/callback_helpers.h"
#include "base/test/bind.h"
#include "chrome/common/read_anything/read_anything.mojom.h"
#include "chrome/renderer/accessibility/read_anything/read_anything_app_model.h"
#include "chrome/renderer/accessibility/read_anything/read_anything_distiller.h"
#include "chrome/test/base/chrome_render_view_test.h"
#include "testing/gtest/include/gtest/gtest.h"

using ReadabilityDistillationResult =
    read_anything::mojom::ReadabilityDistillationResult;
using ReadabilityDistillationReason =
    read_anything::mojom::ReadabilityDistillationReason;

namespace {

// Readability requires an explicit reason. Tests that don't care which reason
// is forwarded use kRedistill.
DistillationRequest RedistillRequest() {
  return DistillationRequest{.reason =
                                 ReadabilityDistillationReason::kRedistill};
}

}  // namespace

class ReadabilityDistillerTest : public ChromeRenderViewTest {
 public:
  ReadabilityDistillerTest() = default;
  ~ReadabilityDistillerTest() override = default;
};

TEST_F(ReadabilityDistillerTest, Distill_Success_RunsDistillationAndCompletes) {
  std::optional<DistillationResult> captured_result;
  ReadabilityDistiller::ReadabilityResultCallback captured_reply;

  ReadabilityDistiller distiller(
      base::BindLambdaForTesting(
          [&](ReadabilityDistillationReason,
              ReadabilityDistiller::ReadabilityResultCallback callback) {
            captured_reply = std::move(callback);
          }),
      base::BindLambdaForTesting(
          [&](const DistillationResult& result) { captured_result = result; }));

  EXPECT_EQ(distiller.GetDistillationMethod(),
            ReadAnythingAppModel::DistillationMethod::kReadability);
  EXPECT_FALSE(distiller.IsDistillationInProgress());

  distiller.Distill(RedistillRequest());
  ASSERT_FALSE(captured_reply.is_null());
  EXPECT_TRUE(distiller.IsDistillationInProgress());

  std::move(captured_reply)
      .Run(ReadabilityDistillationResult::kSuccess, "Title", "<p>Content</p>");

  EXPECT_FALSE(distiller.IsDistillationInProgress());
  ASSERT_TRUE(captured_result.has_value());
  EXPECT_EQ(captured_result->type, DistillationResult::Type::kHTML);
  EXPECT_EQ(captured_result->title, "Title");
  EXPECT_EQ(captured_result->html_content, "<p>Content</p>");
}

TEST_F(ReadabilityDistillerTest, Distill_EmptyContent_CompletesWithEmptyHtml) {
  std::vector<DistillationResult> captured_results;
  ReadabilityDistiller::ReadabilityResultCallback captured_reply;

  ReadabilityDistiller distiller(
      base::BindLambdaForTesting(
          [&](ReadabilityDistillationReason,
              ReadabilityDistiller::ReadabilityResultCallback callback) {
            captured_reply = std::move(callback);
          }),
      base::BindLambdaForTesting([&](const DistillationResult& result) {
        captured_results.push_back(result);
      }));

  // Both kEmpty and kIneligible should forward an empty HTML result so the
  // controller can trigger its fallback policy (e.g. switching to Screen2x).
  distiller.Distill(RedistillRequest());
  ASSERT_FALSE(captured_reply.is_null());
  std::move(captured_reply).Run(ReadabilityDistillationResult::kEmpty, "", "");

  captured_reply = base::NullCallback();
  distiller.Distill(RedistillRequest());
  ASSERT_FALSE(captured_reply.is_null());
  std::move(captured_reply)
      .Run(ReadabilityDistillationResult::kIneligible, "", "");

  EXPECT_FALSE(distiller.IsDistillationInProgress());
  ASSERT_EQ(captured_results.size(), 2u);
  EXPECT_EQ(captured_results[0].type, DistillationResult::Type::kHTML);
  EXPECT_TRUE(captured_results[0].html_content.empty());
  EXPECT_EQ(captured_results[1].type, DistillationResult::Type::kHTML);
  EXPECT_TRUE(captured_results[1].html_content.empty());
}

TEST_F(ReadabilityDistillerTest, Distill_CancelledResult_IsIgnored) {
  std::optional<DistillationResult> captured_result;
  ReadabilityDistiller::ReadabilityResultCallback captured_reply;

  ReadabilityDistiller distiller(
      base::BindLambdaForTesting(
          [&](ReadabilityDistillationReason,
              ReadabilityDistiller::ReadabilityResultCallback callback) {
            captured_reply = std::move(callback);
          }),
      base::BindLambdaForTesting(
          [&](const DistillationResult& result) { captured_result = result; }));

  distiller.Distill(RedistillRequest());
  ASSERT_FALSE(captured_reply.is_null());
  EXPECT_TRUE(distiller.IsDistillationInProgress());

  std::move(captured_reply)
      .Run(ReadabilityDistillationResult::kCancelled, "", "");

  EXPECT_FALSE(distiller.IsDistillationInProgress());
  EXPECT_FALSE(captured_result.has_value());
}

TEST_F(ReadabilityDistillerTest,
       ConsecutiveDistillations_SupersededRequestInvalidatesPreviousWeakPtr) {
  std::optional<DistillationResult> captured_result;
  std::vector<ReadabilityDistiller::ReadabilityResultCallback> captured_replies;

  ReadabilityDistiller distiller(
      base::BindLambdaForTesting(
          [&](ReadabilityDistillationReason,
              ReadabilityDistiller::ReadabilityResultCallback callback) {
            captured_replies.push_back(std::move(callback));
          }),
      base::BindLambdaForTesting(
          [&](const DistillationResult& result) { captured_result = result; }));

  // Issue two requests back-to-back, superseding the first.
  distiller.Distill(RedistillRequest());
  distiller.Distill(RedistillRequest());
  ASSERT_EQ(captured_replies.size(), 2u);
  EXPECT_TRUE(distiller.IsDistillationInProgress());

  // Running the superseded callback (even with kEmpty or kSuccess) must be
  // dropped because its WeakPtr was invalidated by the second Distill() call.
  std::move(captured_replies[0])
      .Run(ReadabilityDistillationResult::kEmpty, "", "");
  EXPECT_TRUE(distiller.IsDistillationInProgress());
  EXPECT_FALSE(captured_result.has_value());

  // Running the active callback completes distillation normally.
  std::move(captured_replies[1])
      .Run(ReadabilityDistillationResult::kSuccess, "Title 2",
           "<p>Content 2</p>");
  EXPECT_FALSE(distiller.IsDistillationInProgress());
  ASSERT_TRUE(captured_result.has_value());
  EXPECT_EQ(captured_result->title, "Title 2");
  EXPECT_EQ(captured_result->html_content, "<p>Content 2</p>");
}

TEST_F(ReadabilityDistillerTest, Distill_DestroyingDistillerInCallback_IsSafe) {
  std::optional<DistillationResult> captured_result;
  ReadabilityDistiller::ReadabilityResultCallback captured_reply;
  std::unique_ptr<ReadabilityDistiller> distiller;

  distiller = std::make_unique<ReadabilityDistiller>(
      base::BindLambdaForTesting(
          [&](ReadabilityDistillationReason,
              ReadabilityDistiller::ReadabilityResultCallback callback) {
            captured_reply = std::move(callback);
          }),
      base::BindLambdaForTesting([&](const DistillationResult& result) {
        captured_result = result;
        // Simulate ReadAnythingAppController falling back to Screen2x and
        // replacing active_distiller_ synchronously inside the callback.
        distiller.reset();
      }));

  distiller->Distill(RedistillRequest());
  ASSERT_FALSE(captured_reply.is_null());

  std::move(captured_reply).Run(ReadabilityDistillationResult::kEmpty, "", "");

  EXPECT_EQ(distiller, nullptr);
  ASSERT_TRUE(captured_result.has_value());
  EXPECT_TRUE(captured_result->html_content.empty());
}

TEST_F(ReadabilityDistillerTest, Distill_ForwardsReadabilityReason) {
  std::vector<ReadabilityDistillationReason> captured_reasons;

  ReadabilityDistiller distiller(
      base::BindLambdaForTesting(
          [&](ReadabilityDistillationReason reason,
              ReadabilityDistiller::ReadabilityResultCallback callback) {
            captured_reasons.push_back(reason);
          }),
      base::DoNothing());

  DistillationRequest tree_changed_request;
  tree_changed_request.reason = ReadabilityDistillationReason::kTreeChanged;
  distiller.Distill(tree_changed_request);

  distiller.Distill(RedistillRequest());

  EXPECT_EQ(captured_reasons, std::vector<ReadabilityDistillationReason>(
                                  {ReadabilityDistillationReason::kTreeChanged,
                                   ReadabilityDistillationReason::kRedistill}));
}
