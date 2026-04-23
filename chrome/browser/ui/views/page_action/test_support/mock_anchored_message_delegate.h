// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_TEST_SUPPORT_MOCK_ANCHORED_MESSAGE_DELEGATE_H_
#define CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_TEST_SUPPORT_MOCK_ANCHORED_MESSAGE_DELEGATE_H_

#include "chrome/browser/ui/views/page_action/anchored_message_view.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace page_actions {

class MockAnchoredMessageDelegate : public AnchoredMessageBubbleView::Delegate {
 public:
  MockAnchoredMessageDelegate();
  ~MockAnchoredMessageDelegate();

  MOCK_METHOD(void, AnchoredMessageChipClick, (), (override));
  MOCK_METHOD(void, CloseAnchoredMessage, (), (override));
  MOCK_METHOD(void, PauseAnchoredMessageTimeout, (), (override));
  MOCK_METHOD(void, ResumeAnchoredMessageTimeout, (), (override));
};

}  // namespace page_actions

#endif  // CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_TEST_SUPPORT_MOCK_ANCHORED_MESSAGE_DELEGATE_H_
