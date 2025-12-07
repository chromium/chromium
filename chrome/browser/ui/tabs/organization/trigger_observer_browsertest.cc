
// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/organization/trigger_observer.h"

#include <memory>

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/time/time.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/web_contents_tester.h"
#include "ui/base/mojom/window_show_state.mojom.h"

namespace {
class AlwaysTrigger : public TriggerPolicy {
 public:
  AlwaysTrigger() = default;
  bool ShouldTrigger(float score) override { return true; }
};

std::unique_ptr<TabOrganizationTrigger> MakeTestTrigger() {
  return std::make_unique<TabOrganizationTrigger>(
      base::BindLambdaForTesting([](TabStripModel* tab_strip_model) -> float {
        return tab_strip_model->count();
      }),
      2.0f, std::make_unique<AlwaysTrigger>());
}
}  // namespace

class TabOrganizationTriggerObserverTest : public InProcessBrowserTest {
 public:
  TabOrganizationTriggerObserverTest() = default;
  TabOrganizationTriggerObserverTest(
      const TabOrganizationTriggerObserverTest&) = delete;
  TabOrganizationTriggerObserverTest& operator=(
      const TabOrganizationTriggerObserverTest&) = delete;

  void AddTabToBrowser(Browser* browser, int index) {
    std::unique_ptr<content::WebContents> web_contents =
        content::WebContents::Create(
            content::WebContents::CreateParams(browser->profile()));

    browser->tab_strip_model()->AddWebContents(
        std::move(web_contents), index,
        ui::PageTransition::PAGE_TRANSITION_TYPED, AddTabTypes::ADD_ACTIVE);
  }

  TabOrganizationTriggerObserver* trigger_observer() {
    return trigger_observer_.get();
  }

  Browser* triggered_browser() { return triggered_browser_; }

 private:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    trigger_observer_ = std::make_unique<TabOrganizationTriggerObserver>(
        base::BindRepeating(&TabOrganizationTriggerObserverTest::OnTrigger,
                            base::Unretained(this)),
        browser()->profile(), MakeTestTrigger());
  }

  void TearDownOnMainThread() override {
    trigger_observer_.reset();
    triggered_browser_ = nullptr;
    InProcessBrowserTest::TearDownOnMainThread();
  }

  void OnTrigger(const Browser* browser) {
    triggered_browser_ = const_cast<Browser*>(browser);
  }

  raw_ptr<Browser> triggered_browser_ = nullptr;
  std::unique_ptr<TabOrganizationTriggerObserver> trigger_observer_;
};

// Flaky on chromeos.
#if BUILDFLAG(IS_CHROMEOS)
#define MAYBE_TriggersOnTabStripModelChange \
  DISABLED_TriggersOnTabStripModelChange
#else
#define MAYBE_TriggersOnTabStripModelChange TriggersOnTabStripModelChange
#endif
IN_PROC_BROWSER_TEST_F(TabOrganizationTriggerObserverTest,
                       MAYBE_TriggersOnTabStripModelChange) {
  ASSERT_TRUE(triggered_browser() == nullptr);

  AddTabToBrowser(browser(), 0);

  // Flush tasks on the UI thread so the deferred trigger evaluation runs.
  base::RunLoop run_loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();
  EXPECT_TRUE(triggered_browser() != nullptr);
  EXPECT_EQ(triggered_browser(), browser());
}

// Flaky on chromeos.
#if BUILDFLAG(IS_CHROMEOS)
#define MAYBE_DebouncesTabStripModelObserverEvents \
  DISABLED_DebouncesTabStripModelObserverEvents
#else
#define MAYBE_DebouncesTabStripModelObserverEvents \
  DebouncesTabStripModelObserverEvents
#endif
IN_PROC_BROWSER_TEST_F(TabOrganizationTriggerObserverTest,
                       MAYBE_DebouncesTabStripModelObserverEvents) {
  ASSERT_TRUE(triggered_browser() == nullptr);

  AddTabToBrowser(browser(), 0);
  AddTabToBrowser(browser(), 0);

  // Flush tasks on the UI thread so the deferred trigger evaluation runs.
  // It should only run once, even though two tabs were added.
  base::RunLoop run_loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();
  EXPECT_TRUE(triggered_browser() != nullptr);
  EXPECT_EQ(triggered_browser(), browser());
}
