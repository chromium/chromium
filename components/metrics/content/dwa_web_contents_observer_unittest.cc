// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/content/dwa_web_contents_observer.h"

#include "base/test/scoped_feature_list.h"
#include "components/metrics/dwa/dwa_entry_builder.h"
#include "components/metrics/dwa/dwa_recorder.h"
#include "content/public/test/test_renderer_host.h"
#include "ui/base/page_transition_types.h"

namespace metrics {

class DwaWebContentsObserverTest : public content::RenderViewHostTestHarness {
 public:
  DwaWebContentsObserverTest() {
    scoped_feature_list_.InitAndEnableFeature(dwa::kDwaFeature);

    recorder_ = dwa::DwaRecorder::Get();
    recorder_->Purge();
    recorder_->EnableRecording();
  }
  ~DwaWebContentsObserverTest() override = default;

  dwa::DwaRecorder* GetRecorder() { return recorder_; }

 protected:
  void SetUp() override {
    RenderViewHostTestHarness::SetUp();
    DwaWebContentsObserver::CreateForWebContents(web_contents());
  }

  void TearDown() override {
    RenderViewHostTestHarness::TearDown();
    recorder_->Purge();
  }

 private:
  raw_ptr<dwa::DwaRecorder> recorder_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(DwaWebContentsObserverTest,
       TestThatDwaRecorderIsTriggeredWhenBrowsingNewPage) {
  // Add Dwa event.
  ::dwa::DwaEntryBuilder builder("Kangaroo.Jumped");
  builder.SetContent("adtech.com");
  builder.SetMetric("Length", 5);
  builder.Record(GetRecorder());

  EXPECT_TRUE(GetRecorder()->HasEntries());
  EXPECT_FALSE(GetRecorder()->HasPageLoadEvents());

  // Browse page by typing into URL bar.
  NavigateAndCommit(GURL("https://www.google.com/"),
                    ui::PageTransition::PAGE_TRANSITION_TYPED);

  EXPECT_FALSE(GetRecorder()->HasEntries());
  EXPECT_TRUE(GetRecorder()->HasPageLoadEvents());
}

TEST_F(DwaWebContentsObserverTest,
       TestThatDwaRecorderIsTriggeredWhenReloadingPage) {
  // Add Dwa event.
  ::dwa::DwaEntryBuilder builder("Kangaroo.Jumped");
  builder.SetContent("adtech.com");
  builder.SetMetric("Length", 5);
  builder.Record(GetRecorder());

  EXPECT_TRUE(GetRecorder()->HasEntries());
  EXPECT_FALSE(GetRecorder()->HasPageLoadEvents());

  // Reload page.
  NavigateAndCommit(GURL("https://www.google.com/"),
                    ui::PageTransition::PAGE_TRANSITION_RELOAD);

  EXPECT_FALSE(GetRecorder()->HasEntries());
  EXPECT_TRUE(GetRecorder()->HasPageLoadEvents());
}

}  // namespace metrics
