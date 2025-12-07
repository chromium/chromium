// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/infobars/core/infobar_container.h"

#include <memory>
#include <vector>

#include "base/containers/contains.h"
#include "base/memory/raw_ptr.h"
#include "components/infobars/core/infobar.h"
#include "components/infobars/core/infobar_delegate.h"
#include "components/infobars/core/infobar_manager.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"
#include "url/gurl.h"

namespace infobars {
namespace {

class TestDelegate : public InfoBarDelegate {
 public:
  TestDelegate() = default;
  ~TestDelegate() override = default;
  InfoBarIdentifier GetIdentifier() const override { return TEST_INFOBAR; }
};

class TestInfoBar : public InfoBar {
 public:
  explicit TestInfoBar(std::unique_ptr<InfoBarDelegate> delegate)
      : InfoBar(std::move(delegate)) {}
  ~TestInfoBar() override = default;
};

class TestInfoBarContainer : public InfoBarContainer {
 public:
  class MockDelegate : public InfoBarContainer::Delegate {
   public:
    void InfoBarContainerStateChanged(bool) override {}
  };

  explicit TestInfoBarContainer(Delegate* delegate)
      : InfoBarContainer(delegate) {}
  ~TestInfoBarContainer() override = default;

  size_t add_calls() const { return add_calls_; }
  size_t remove_calls() const { return remove_calls_; }

 protected:
  void PlatformSpecificAddInfoBar(InfoBar* infobar, size_t) override {
    ++add_calls_;
  }

  void PlatformSpecificRemoveInfoBar(InfoBar* infobar) override {
    ++remove_calls_;
  }

 private:
  size_t add_calls_ = 0;
  size_t remove_calls_ = 0;
};

class TestInfoBarManager : public InfoBarManager {
 public:
  TestInfoBarManager() { set_animations_enabled(false); }
  ~TestInfoBarManager() override = default;
  int GetActiveEntryID() override { return 0; }
  void OpenURL(const GURL&, WindowOpenDisposition) override {}

  using InfoBarManager::AddInfoBar;
  using InfoBarManager::RemoveInfoBar;
};

TEST(InfoBarContainerTest, AddsInfoBarsSequentially) {
  TestInfoBarContainer::MockDelegate delegate;
  TestInfoBarContainer container(&delegate);
  TestInfoBarManager manager;
  container.ChangeInfoBarManager(&manager);

  // Add multiple infobars.
  auto* infobar1 = manager.AddInfoBar(
      std::make_unique<TestInfoBar>(std::make_unique<TestDelegate>()));
  auto* infobar2 = manager.AddInfoBar(
      std::make_unique<TestInfoBar>(std::make_unique<TestDelegate>()));
  auto* infobar3 = manager.AddInfoBar(
      std::make_unique<TestInfoBar>(std::make_unique<TestDelegate>()));

  // In the base container, all three should have been added directly to the
  // view without any queuing.
  EXPECT_EQ(3u, container.add_calls());

  // Clean up.
  manager.RemoveInfoBar(infobar1);
  manager.RemoveInfoBar(infobar2);
  manager.RemoveInfoBar(infobar3);
  EXPECT_EQ(3u, container.remove_calls());
}

}  // namespace
}  // namespace infobars
