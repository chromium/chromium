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

TEST_F(TabSharingStatusMessageViewTest, JustText) {
  TabSharingStatusMessageView view(MessageInfo(u"Just text.", {}));
  EXPECT_THAT(GetChildTexts(view), ElementsAreArray({u"Just text."}));
}

TEST_F(TabSharingStatusMessageViewTest, OneButtonOnly) {
  TabSharingStatusMessageView view(MessageInfo(u"$1", {kTab1}));
  EXPECT_THAT(GetChildTexts(view), ElementsAreArray({u"Tab1"}));
}

TEST_F(TabSharingStatusMessageViewTest, OneButtonPrefix) {
  TabSharingStatusMessageView view(MessageInfo(u"prefix-$1", {kTab1}));
  EXPECT_THAT(GetChildTexts(view), ElementsAreArray({
                                       u"prefix-",
                                       u"Tab1",
                                   }));
}

TEST_F(TabSharingStatusMessageViewTest, OneButtonPostfix) {
  TabSharingStatusMessageView view(MessageInfo(u"$1-postfix", {kTab1}));
  EXPECT_THAT(GetChildTexts(view), ElementsAreArray({u"Tab1", u"-postfix"}));
}

TEST_F(TabSharingStatusMessageViewTest, OneButtonPrefixAndPostfix) {
  TabSharingStatusMessageView view(MessageInfo(u"prefix-$1-postfix", {kTab1}));
  EXPECT_THAT(GetChildTexts(view),
              ElementsAreArray({u"prefix-", u"Tab1", u"-postfix"}));
}

TEST_F(TabSharingStatusMessageViewTest, TwoButtons) {
  TabSharingStatusMessageView view(MessageInfo(u"$1$2", {kTab1, kTab2}));
  EXPECT_THAT(GetChildTexts(view), ElementsAreArray({u"Tab1", u"Tab2"}));
}

TEST_F(TabSharingStatusMessageViewTest, TwoButtonsPrefix) {
  TabSharingStatusMessageView view(MessageInfo(u"prefix-$1$2", {kTab1, kTab2}));
  EXPECT_THAT(GetChildTexts(view),
              ElementsAreArray({u"prefix-", u"Tab1", u"Tab2"}));
}

TEST_F(TabSharingStatusMessageViewTest, TwoButtonsInfix) {
  TabSharingStatusMessageView view(MessageInfo(u"$1-infix-$2", {kTab1, kTab2}));
  EXPECT_THAT(GetChildTexts(view),
              ElementsAreArray({u"Tab1", u"-infix-", u"Tab2"}));
}

TEST_F(TabSharingStatusMessageViewTest, TwoButtonsPostfix) {
  TabSharingStatusMessageView view(
      MessageInfo(u"$1$2-postfix", {kTab1, kTab2}));
  EXPECT_THAT(GetChildTexts(view),
              ElementsAreArray({u"Tab1", u"Tab2", u"-postfix"}));
}

TEST_F(TabSharingStatusMessageViewTest, TwoButtonsPrefixAndInfix) {
  TabSharingStatusMessageView view(
      MessageInfo(u"prefix-$1-infix-$2", {kTab1, kTab2}));
  EXPECT_THAT(GetChildTexts(view),
              ElementsAreArray({u"prefix-", u"Tab1", u"-infix-", u"Tab2"}));
}

TEST_F(TabSharingStatusMessageViewTest, TwoButtonsInfixAndPostfix) {
  TabSharingStatusMessageView view(
      MessageInfo(u"$1-infix-$2-postfix", {kTab1, kTab2}));
  EXPECT_THAT(GetChildTexts(view),
              ElementsAreArray({u"Tab1", u"-infix-", u"Tab2", u"-postfix"}));
}

TEST_F(TabSharingStatusMessageViewTest, TwoButtonsPrefixAndPostfix) {
  TabSharingStatusMessageView view(
      MessageInfo(u"prefix-$1$2-postfix", {kTab1, kTab2}));
  EXPECT_THAT(GetChildTexts(view),
              ElementsAreArray({u"prefix-", u"Tab1", u"Tab2", u"-postfix"}));
}

TEST_F(TabSharingStatusMessageViewTest, TwoButtonsPrefixAndInfixAndPostfix) {
  TabSharingStatusMessageView view(
      MessageInfo(u"prefix-$1-infix-$2-postfix", {kTab1, kTab2}));
  EXPECT_THAT(GetChildTexts(view),
              ElementsAreArray(
                  {u"prefix-", u"Tab1", u"-infix-", u"Tab2", u"-postfix"}));
}

TEST_F(TabSharingStatusMessageViewTest, ReversedTwoButtons) {
  TabSharingStatusMessageView view(MessageInfo(u"$2$1", {kTab1, kTab2}));
  EXPECT_THAT(GetChildTexts(view), ElementsAreArray({u"Tab2", u"Tab1"}));
}

TEST_F(TabSharingStatusMessageViewTest, ReversedTwoButtonsPrefix) {
  TabSharingStatusMessageView view(MessageInfo(u"prefix-$2$1", {kTab1, kTab2}));
  EXPECT_THAT(GetChildTexts(view),
              ElementsAreArray({u"prefix-", u"Tab2", u"Tab1"}));
}

TEST_F(TabSharingStatusMessageViewTest, ReversedTwoButtonsInfix) {
  TabSharingStatusMessageView view(MessageInfo(u"$2-infix-$1", {kTab1, kTab2}));
  EXPECT_THAT(GetChildTexts(view),
              ElementsAreArray({u"Tab2", u"-infix-", u"Tab1"}));
}

TEST_F(TabSharingStatusMessageViewTest, ReversedTwoButtonsPostfix) {
  TabSharingStatusMessageView view(
      MessageInfo(u"$2$1-postfix", {kTab1, kTab2}));
  EXPECT_THAT(GetChildTexts(view),
              ElementsAreArray({u"Tab2", u"Tab1", u"-postfix"}));
}

TEST_F(TabSharingStatusMessageViewTest, ReversedTwoButtonsPrefixAndInfix) {
  TabSharingStatusMessageView view(
      MessageInfo(u"prefix-$2-infix-$1", {kTab1, kTab2}));
  EXPECT_THAT(GetChildTexts(view),
              ElementsAreArray({u"prefix-", u"Tab2", u"-infix-", u"Tab1"}));
}

TEST_F(TabSharingStatusMessageViewTest, ReversedTwoButtonsInfixAndPostfix) {
  TabSharingStatusMessageView view(
      MessageInfo(u"$2-infix-$1-postfix", {kTab1, kTab2}));
  EXPECT_THAT(GetChildTexts(view),
              ElementsAreArray({u"Tab2", u"-infix-", u"Tab1", u"-postfix"}));
}

TEST_F(TabSharingStatusMessageViewTest, ReversedTwoButtonsPrefixAndPostfix) {
  TabSharingStatusMessageView view(
      MessageInfo(u"prefix-$2$1-postfix", {kTab1, kTab2}));
  EXPECT_THAT(GetChildTexts(view),
              ElementsAreArray({u"prefix-", u"Tab2", u"Tab1", u"-postfix"}));
}

TEST_F(TabSharingStatusMessageViewTest,
       ReversedTwoButtonsPrefixAndInfixAndPostfix) {
  TabSharingStatusMessageView view(
      MessageInfo(u"prefix-$2-infix-$1-postfix", {kTab1, kTab2}));
  EXPECT_THAT(GetChildTexts(view),
              ElementsAreArray(
                  {u"prefix-", u"Tab2", u"-infix-", u"Tab1", u"-postfix"}));
}

TEST_F(TabSharingStatusMessageViewTest, SpacesPrefix) {
  TabSharingStatusMessageView view(MessageInfo(u"   $1", {kTab1}));
  EXPECT_THAT(GetChildTexts(view), ElementsAreArray({u"   ", u"Tab1"}));
}

TEST_F(TabSharingStatusMessageViewTest, SpacesInfix) {
  TabSharingStatusMessageView view(MessageInfo(u"$1   $2", {kTab1, kTab2}));
  EXPECT_THAT(GetChildTexts(view),
              ElementsAreArray({u"Tab1", u"   ", u"Tab2"}));
}

TEST_F(TabSharingStatusMessageViewTest, SpacesPostfix) {
  TabSharingStatusMessageView view(MessageInfo(u"$1   ", {kTab1}));
  EXPECT_THAT(GetChildTexts(view), ElementsAreArray({u"Tab1", u"   "}));
}
