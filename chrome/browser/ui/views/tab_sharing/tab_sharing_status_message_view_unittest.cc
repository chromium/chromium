// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tab_sharing/tab_sharing_status_message_view.h"

#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/tab_sharing/tab_sharing_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
using EndpointInfo = TabSharingStatusMessageView::EndpointInfo;
using MessageInfo = TabSharingStatusMessageView::MessageInfo;
using content::GlobalRenderFrameHostId;

using ::testing::ElementsAreArray;

std::vector<std::u16string_view> GetChildTexts(
    const TabSharingStatusMessageView& info_view) {
  std::vector<std::u16string_view> texts;
  for (const views::View* button_or_label : info_view.children()) {
    texts.emplace_back(GetButtonOrLabelText(*button_or_label));
  }
  return texts;
}

EndpointInfo kTab1 = EndpointInfo(u"Tab1", GlobalRenderFrameHostId(1, 1));
EndpointInfo kTab2 = EndpointInfo(u"Tab2", GlobalRenderFrameHostId(2, 2));
}  // namespace

class TabSharingStatusMessageViewTest : public ::testing::Test {
 private:
  ChromeLayoutProvider layout_provider_;
};

TEST_F(TabSharingStatusMessageViewTest, TestJustText) {
  TabSharingStatusMessageView view(
      MessageInfo(u"Sharing something with something.", {}));
  EXPECT_THAT(GetChildTexts(view),
              ElementsAreArray({u"Sharing something with something."}));
}

TEST_F(TabSharingStatusMessageViewTest, TestOneButton) {
  TabSharingStatusMessageView view(
      MessageInfo(u"Sharing $1 with this tab.", {kTab1}));
  EXPECT_THAT(GetChildTexts(view),
              ElementsAreArray({u"Sharing ", u"Tab1", u" with this tab."}));
}

TEST_F(TabSharingStatusMessageViewTest, TestOneButtonNoPrefix) {
  TabSharingStatusMessageView view(
      MessageInfo(u"$1 is shared with this tab.", {kTab1}));
  EXPECT_THAT(GetChildTexts(view),
              ElementsAreArray({u"Tab1", u" is shared with this tab."}));
}

TEST_F(TabSharingStatusMessageViewTest, TestOneButtonNoSuffix) {
  TabSharingStatusMessageView view(
      MessageInfo(u"This tab is capturing $1", {kTab1}));
  EXPECT_THAT(GetChildTexts(view),
              ElementsAreArray({u"This tab is capturing ", u"Tab1"}));
}

TEST_F(TabSharingStatusMessageViewTest, TestTwoButtons) {
  TabSharingStatusMessageView view(
      MessageInfo(u"Sharing $1 with $2.", {kTab1, kTab2}));
  EXPECT_THAT(
      GetChildTexts(view),
      ElementsAreArray({u"Sharing ", u"Tab1", u" with ", u"Tab2", u"."}));
}

TEST_F(TabSharingStatusMessageViewTest, TestTwoButtonsReversed) {
  TabSharingStatusMessageView view(
      MessageInfo(u"$2 is capturing $1.", {kTab1, kTab2}));
  EXPECT_THAT(GetChildTexts(view),
              ElementsAreArray({u"Tab2", u" is capturing ", u"Tab1", u"."}));
}
