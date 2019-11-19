// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/web_contents_close_handler.h"

#include "base/macros.h"
#include "base/test/task_environment.h"
#include "chrome/browser/ui/views/frame/web_contents_close_handler_delegate.h"
#include "testing/gtest/include/gtest/gtest.h"

class MockWebContentsCloseHandlerDelegate
    : public WebContentsCloseHandlerDelegate {
 public:
  MockWebContentsCloseHandlerDelegate()
      : task_environment_(
            base::test::SingleThreadTaskEnvironment::MainThreadType::UI),
        got_clone_(false),
        got_destroy_(false) {}
  ~MockWebContentsCloseHandlerDelegate() override {}

  void Clear() {
    got_clone_ = got_destroy_ = false;
  }

  bool got_clone() const { return got_clone_; }
  void clear_got_clone() { got_clone_ = false; }

  bool got_destroy() const { return got_destroy_; }
  void clear_got_destroy() { got_destroy_ = false; }

  // WebContentsCloseHandlerDelegate:
  void CloneWebContentsLayer() override { got_clone_ = true; }
  void DestroyClonedLayer() override { got_destroy_ = true; }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  bool got_clone_;
  bool got_destroy_;

  DISALLOW_COPY_AND_ASSIGN(MockWebContentsCloseHandlerDelegate);
};

// -----------------------------------------------------------------------------

class WebContentsCloseHandlerTest : public testing::Test {
 public:
  WebContentsCloseHandlerTest() : close_handler_(&close_handler_delegate_) {}
  ~WebContentsCloseHandlerTest() override {}

 protected:
  bool IsTimerRunning() const {
    return close_handler_.timer_.IsRunning();
  }

  MockWebContentsCloseHandlerDelegate close_handler_delegate_;
  WebContentsCloseHandler close_handler_;

 private:
  DISALLOW_COPY_AND_ASSIGN(WebContentsCloseHandlerTest);
};

// Verifies ActiveTabChanged() sends the right functions to the delegate.
TEST_F(WebContentsCloseHandlerTest, ChangingActiveTabDestroys) {
  close_handler_.ActiveTabChanged();
  EXPECT_TRUE(close_handler_delegate_.got_destroy());
  EXPECT_FALSE(close_handler_delegate_.got_clone());
  EXPECT_FALSE(IsTimerRunning());
}

// Verifies ActiveTabChanged() while in a close does nothing.
TEST_F(WebContentsCloseHandlerTest, DontCloneOnChangeWhenClosing) {
  close_handler_.WillCloseAllTabs();
  EXPECT_FALSE(close_handler_delegate_.got_destroy());
  EXPECT_TRUE(close_handler_delegate_.got_clone());
  EXPECT_FALSE(IsTimerRunning());
  close_handler_delegate_.Clear();

  close_handler_.ActiveTabChanged();
  EXPECT_FALSE(close_handler_delegate_.got_destroy());
  EXPECT_FALSE(close_handler_delegate_.got_clone());
  EXPECT_FALSE(IsTimerRunning());
}

// Verifies the timer is started after a close.
TEST_F(WebContentsCloseHandlerTest, DontDestroyImmediatleyAfterCancel) {
  close_handler_.WillCloseAllTabs();
  close_handler_delegate_.Clear();
  close_handler_.CloseAllTabsCanceled();
  EXPECT_FALSE(close_handler_delegate_.got_destroy());
  EXPECT_FALSE(close_handler_delegate_.got_clone());
  EXPECT_TRUE(IsTimerRunning());
}
