// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/pdf/browser/pdf_first_content_paint_registry.h"

#include "base/functional/bind.h"
#include "base/test/bind.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace pdf {

namespace {

// The registry only ever passes the pointer through, so the tests use tagged
// sentinels rather than building real WebContents.
content::WebContents* FakeContents(uintptr_t tag) {
  return reinterpret_cast<content::WebContents*>(tag);
}

}  // namespace

TEST(PdfFirstContentPaintRegistryTest, NotifiesSubscriber) {
  content::WebContents* observed = nullptr;
  base::TimeTicks observed_time;
  int calls = 0;

  base::CallbackListSubscription subscription =
      RegisterPdfFirstContentPaintCallback(base::BindLambdaForTesting(
          [&](content::WebContents* embedder, base::TimeTicks paint_time) {
            observed = embedder;
            observed_time = paint_time;
            ++calls;
          }));

  const base::TimeTicks paint_time =
      base::TimeTicks() + base::Milliseconds(1234);
  NotifyPdfFirstContentPainted(FakeContents(0x1), paint_time);

  EXPECT_EQ(1, calls);
  EXPECT_EQ(FakeContents(0x1), observed);
  EXPECT_EQ(paint_time, observed_time);
}

TEST(PdfFirstContentPaintRegistryTest, NotifiesEverySubscriber) {
  int first_calls = 0;
  int second_calls = 0;

  base::CallbackListSubscription first =
      RegisterPdfFirstContentPaintCallback(base::BindLambdaForTesting(
          [&](content::WebContents*, base::TimeTicks) { ++first_calls; }));
  base::CallbackListSubscription second =
      RegisterPdfFirstContentPaintCallback(base::BindLambdaForTesting(
          [&](content::WebContents*, base::TimeTicks) { ++second_calls; }));

  NotifyPdfFirstContentPainted(FakeContents(0x1), base::TimeTicks::Now());

  EXPECT_EQ(1, first_calls);
  EXPECT_EQ(1, second_calls);
}

// The startup profiler binds itself into the registry with base::Unretained,
// which is only safe because dropping the subscription unsubscribes. If this
// regressed, that binding would become a use-after-free.
TEST(PdfFirstContentPaintRegistryTest, ReleasingSubscriptionUnsubscribes) {
  int calls = 0;

  {
    base::CallbackListSubscription subscription =
        RegisterPdfFirstContentPaintCallback(base::BindLambdaForTesting(
            [&](content::WebContents*, base::TimeTicks) { ++calls; }));

    NotifyPdfFirstContentPainted(FakeContents(0x1), base::TimeTicks::Now());
    EXPECT_EQ(1, calls);
  }

  NotifyPdfFirstContentPainted(FakeContents(0x1), base::TimeTicks::Now());

  EXPECT_EQ(1, calls);
}

TEST(PdfFirstContentPaintRegistryTest, NotifyWithNoSubscribersIsSafe) {
  NotifyPdfFirstContentPainted(FakeContents(0x1), base::TimeTicks::Now());
}

}  // namespace pdf
