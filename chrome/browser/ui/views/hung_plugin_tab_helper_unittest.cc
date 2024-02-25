// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/hung_plugin_tab_helper.h"

#include "base/memory/ptr_util.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/views/infobars/confirm_infobar.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/views/chrome_test_views_delegate.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "ui/events/event.h"
#include "ui/events/test/test_event.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/test/button_test_api.h"

class HungPluginInfoBarObserver : public infobars::InfoBarManager::Observer {
 public:
  explicit HungPluginInfoBarObserver(infobars::InfoBarManager* manager);

  // infobars::InfoBarManager::Observer:
  void OnInfoBarRemoved(infobars::InfoBar* infobar, bool animate) override;

  bool seen_removal() const { return seen_removal_; }

 private:
  bool seen_removal_ = false;

  base::ScopedObservation<infobars::InfoBarManager,
                          infobars::InfoBarManager::Observer>
      infobar_observation_{this};
};

HungPluginInfoBarObserver::HungPluginInfoBarObserver(
    infobars::InfoBarManager* manager) {
  infobar_observation_.Observe(manager);
}

void HungPluginInfoBarObserver::OnInfoBarRemoved(infobars::InfoBar* infobar,
                                                 bool animate) {
  seen_removal_ = true;
}

void RemoveOnlyOnce(ConfirmInfoBar* infobar) {
  DCHECK(infobar->owner());
  HungPluginInfoBarObserver observer(infobar->owner());
  if (infobar->GetDelegate()->Accept()) {
    ASSERT_FALSE(observer.seen_removal());
    infobar->RemoveSelf();
  }
}

class HungPluginTabHelperTest : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override;

 private:
  ChromeTestViewsDelegate<> views_delegate_;
};

void HungPluginTabHelperTest::SetUp() {
  ChromeRenderViewHostTestHarness::SetUp();

  HungPluginTabHelper::CreateForWebContents(web_contents());
  infobars::ContentInfoBarManager::CreateForWebContents(web_contents());
}

// Regression test for https://crbug.com/969099 .
TEST_F(HungPluginTabHelperTest, DontRemoveTwice) {
  HungPluginTabHelper::FromWebContents(web_contents())
      ->PluginHungStatusChanged(0, base::FilePath(), true);
  infobars::ContentInfoBarManager* infobar_manager =
      infobars::ContentInfoBarManager::FromWebContents(web_contents());
  ASSERT_TRUE(infobar_manager);
  ASSERT_EQ(1u, infobar_manager->infobars().size());
  auto* infobar = static_cast<ConfirmInfoBar*>(infobar_manager->infobars()[0]);
  views::MdTextButton* ok_button = infobar->ok_button_for_testing();
  ok_button->SetCallback(
      base::BindRepeating(&RemoveOnlyOnce, base::Unretained(infobar)));
  views::test::ButtonTestApi(ok_button).NotifyClick(ui::test::TestEvent());
  EXPECT_EQ(0u, infobar_manager->infobars().size());
}
