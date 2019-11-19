// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/hung_plugin_tab_helper.h"

#include "base/scoped_observer.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/ui/views/infobars/confirm_infobar.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/views/chrome_test_views_delegate.h"
#include "ui/events/event.h"

class HungPluginInfoBarObserver : public infobars::InfoBarManager::Observer {
 public:
  explicit HungPluginInfoBarObserver(infobars::InfoBarManager* manager);

  // infobars::InfoBarManager::Observer:
  void OnInfoBarRemoved(infobars::InfoBar* infobar, bool animate) override;

  bool seen_removal() const { return seen_removal_; }

 private:
  bool seen_removal_ = false;

  ScopedObserver<infobars::InfoBarManager, infobars::InfoBarManager::Observer>
      infobar_observer_{this};
};

HungPluginInfoBarObserver::HungPluginInfoBarObserver(
    infobars::InfoBarManager* manager) {
  infobar_observer_.Add(manager);
}

void HungPluginInfoBarObserver::OnInfoBarRemoved(infobars::InfoBar* infobar,
                                                 bool animate) {
  seen_removal_ = true;
}

class HungPluginMockInfoBar : public ConfirmInfoBar {
 public:
  explicit HungPluginMockInfoBar(
      std::unique_ptr<ConfirmInfoBarDelegate> delegate);

 private:
  // ConfirmInfoBar:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;
};

HungPluginMockInfoBar::HungPluginMockInfoBar(
    std::unique_ptr<ConfirmInfoBarDelegate> delegate)
    : ConfirmInfoBar(std::move(delegate)) {}

void HungPluginMockInfoBar::ButtonPressed(views::Button* sender,
                                          const ui::Event& event) {
  DCHECK(owner());
  HungPluginInfoBarObserver observer(owner());
  if (GetDelegate()->Accept()) {
    ASSERT_FALSE(observer.seen_removal());
    RemoveSelf();
  }
}

class HungPluginMockInfoBarService : public InfoBarService {
 public:
  // Creates a HungPluginMockInfoBarService and attaches it as the
  // InfoBarService for |web_contents|.
  static void CreateForWebContents(content::WebContents* web_contents);

  std::unique_ptr<infobars::InfoBar> CreateConfirmInfoBar(
      std::unique_ptr<ConfirmInfoBarDelegate> delegate) override;

 private:
  using InfoBarService::InfoBarService;
};

void HungPluginMockInfoBarService::CreateForWebContents(
    content::WebContents* web_contents) {
  DCHECK(web_contents);
  const void* user_data_key = UserDataKey();
  DCHECK(!web_contents->GetUserData(user_data_key));
  web_contents->SetUserData(
      user_data_key,
      base::WrapUnique(new HungPluginMockInfoBarService(web_contents)));
}

std::unique_ptr<infobars::InfoBar>
HungPluginMockInfoBarService::CreateConfirmInfoBar(
    std::unique_ptr<ConfirmInfoBarDelegate> delegate) {
  return std::make_unique<HungPluginMockInfoBar>(std::move(delegate));
}

class HungPluginTabHelperTest : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override;

 private:
  ChromeTestViewsDelegate views_delegate_;
};

void HungPluginTabHelperTest::SetUp() {
  ChromeRenderViewHostTestHarness::SetUp();

  HungPluginTabHelper::CreateForWebContents(web_contents());
  HungPluginMockInfoBarService::CreateForWebContents(web_contents());
}

class DummyEvent : public ui::Event {
 public:
  DummyEvent();
};

DummyEvent::DummyEvent() : Event(ui::ET_UNKNOWN, base::TimeTicks(), 0) {}

// Regression test for https://crbug.com/969099 .
TEST_F(HungPluginTabHelperTest, DontRemoveTwice) {
  HungPluginTabHelper::FromWebContents(web_contents())
      ->PluginHungStatusChanged(0, base::FilePath(), true);
  InfoBarService* infobar_service =
      InfoBarService::FromWebContents(web_contents());
  ASSERT_EQ(1u, infobar_service->infobar_count());
  static_cast<InfoBarView*>(infobar_service->infobar_at(0))
      ->ButtonPressed(nullptr, DummyEvent());
  EXPECT_EQ(0u, infobar_service->infobar_count());
}
