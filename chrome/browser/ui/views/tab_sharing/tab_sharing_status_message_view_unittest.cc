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
using TabRole = TabSharingInfoBarDelegate::TabRole;
using content::GlobalRenderFrameHostId;
using ::testing::ElementsAreArray;

std::vector<std::string> GetChildTexts(
    const TabSharingStatusMessageView& info_view) {
  std::vector<std::string> texts;
  for (const views::View* view : info_view.children()) {
    if (std::optional<std::u16string_view> text = GetButtonOrLabelText(*view)) {
      texts.emplace_back(base::UTF16ToUTF8(*text));
    }
  }
  return texts;
}

EndpointInfo kTab1 = EndpointInfo(u"Tab1",
                                  EndpointInfo::TargetType::kCapturedTab,
                                  GlobalRenderFrameHostId(1, 1));
EndpointInfo kTab2 = EndpointInfo(u"Tab2",
                                  EndpointInfo::TargetType::kCapturingTab,
                                  GlobalRenderFrameHostId(2, 2));
EndpointInfo kWithoutId1 = EndpointInfo(u"WithoutId1",
                                        EndpointInfo::TargetType::kCapturedTab,
                                        GlobalRenderFrameHostId());
EndpointInfo kWithoutId2 = EndpointInfo(u"WithoutId2",
                                        EndpointInfo::TargetType::kCapturingTab,
                                        GlobalRenderFrameHostId());
}  // namespace

class TabSharingStatusMessageViewTest : public ::testing::Test {
 private:
  ChromeLayoutProvider layout_provider_;
};

TEST_F(TabSharingStatusMessageViewTest, JustText) {
  TabSharingStatusMessageView view(MessageInfo(u"Just text.", {}));
  EXPECT_THAT(GetChildTexts(view), ElementsAreArray({"Just text."}));
}

TEST_F(TabSharingStatusMessageViewTest, OneButtonOnly) {
  TabSharingStatusMessageView view(MessageInfo(u"$1", {kTab1}));
  EXPECT_THAT(GetChildTexts(view), ElementsAreArray({"Tab1"}));
}

TEST_F(TabSharingStatusMessageViewTest, OneButtonPrefix) {
  TabSharingStatusMessageView view(MessageInfo(u"prefix-$1", {kTab1}));
  EXPECT_THAT(GetChildTexts(view), ElementsAreArray({
                                       "prefix-",
                                       "Tab1",
                                   }));
}

TEST_F(TabSharingStatusMessageViewTest, OneButtonPostfix) {
  TabSharingStatusMessageView view(MessageInfo(u"$1-postfix", {kTab1}));
  EXPECT_THAT(GetChildTexts(view), ElementsAreArray({"Tab1", "-postfix"}));
}

TEST_F(TabSharingStatusMessageViewTest, OneButtonPrefixAndPostfix) {
  TabSharingStatusMessageView view(MessageInfo(u"prefix-$1-postfix", {kTab1}));
  EXPECT_THAT(GetChildTexts(view),
              ElementsAreArray({"prefix-", "Tab1", "-postfix"}));
}

TEST_F(TabSharingStatusMessageViewTest, TwoButtons) {
  TabSharingStatusMessageView view(MessageInfo(u"$1$2", {kTab1, kTab2}));
  EXPECT_THAT(GetChildTexts(view), ElementsAreArray({"Tab1", "Tab2"}));
}

TEST_F(TabSharingStatusMessageViewTest, TwoButtonsPrefix) {
  TabSharingStatusMessageView view(MessageInfo(u"prefix-$1$2", {kTab1, kTab2}));
  EXPECT_THAT(GetChildTexts(view),
              ElementsAreArray({"prefix-", "Tab1", "Tab2"}));
}

TEST_F(TabSharingStatusMessageViewTest, TwoButtonsInfix) {
  TabSharingStatusMessageView view(MessageInfo(u"$1-infix-$2", {kTab1, kTab2}));
  EXPECT_THAT(GetChildTexts(view),
              ElementsAreArray({"Tab1", "-infix-", "Tab2"}));
}

TEST_F(TabSharingStatusMessageViewTest, TwoButtonsPostfix) {
  TabSharingStatusMessageView view(
      MessageInfo(u"$1$2-postfix", {kTab1, kTab2}));
  EXPECT_THAT(GetChildTexts(view),
              ElementsAreArray({"Tab1", "Tab2", "-postfix"}));
}

TEST_F(TabSharingStatusMessageViewTest, TwoButtonsPrefixAndInfix) {
  TabSharingStatusMessageView view(
      MessageInfo(u"prefix-$1-infix-$2", {kTab1, kTab2}));
  EXPECT_THAT(GetChildTexts(view),
              ElementsAreArray({"prefix-", "Tab1", "-infix-", "Tab2"}));
}

TEST_F(TabSharingStatusMessageViewTest, TwoButtonsInfixAndPostfix) {
  TabSharingStatusMessageView view(
      MessageInfo(u"$1-infix-$2-postfix", {kTab1, kTab2}));
  EXPECT_THAT(GetChildTexts(view),
              ElementsAreArray({"Tab1", "-infix-", "Tab2", "-postfix"}));
}

TEST_F(TabSharingStatusMessageViewTest, TwoButtonsPrefixAndPostfix) {
  TabSharingStatusMessageView view(
      MessageInfo(u"prefix-$1$2-postfix", {kTab1, kTab2}));
  EXPECT_THAT(GetChildTexts(view),
              ElementsAreArray({"prefix-", "Tab1", "Tab2", "-postfix"}));
}

TEST_F(TabSharingStatusMessageViewTest, TwoButtonsPrefixAndInfixAndPostfix) {
  TabSharingStatusMessageView view(
      MessageInfo(u"prefix-$1-infix-$2-postfix", {kTab1, kTab2}));
  EXPECT_THAT(
      GetChildTexts(view),
      ElementsAreArray({"prefix-", "Tab1", "-infix-", "Tab2", "-postfix"}));
}

TEST_F(TabSharingStatusMessageViewTest, ReversedTwoButtons) {
  TabSharingStatusMessageView view(MessageInfo(u"$2$1", {kTab1, kTab2}));
  EXPECT_THAT(GetChildTexts(view), ElementsAreArray({"Tab2", "Tab1"}));
}

TEST_F(TabSharingStatusMessageViewTest, ReversedTwoButtonsPrefix) {
  TabSharingStatusMessageView view(MessageInfo(u"prefix-$2$1", {kTab1, kTab2}));
  EXPECT_THAT(GetChildTexts(view),
              ElementsAreArray({"prefix-", "Tab2", "Tab1"}));
}

TEST_F(TabSharingStatusMessageViewTest, ReversedTwoButtonsInfix) {
  TabSharingStatusMessageView view(MessageInfo(u"$2-infix-$1", {kTab1, kTab2}));
  EXPECT_THAT(GetChildTexts(view),
              ElementsAreArray({"Tab2", "-infix-", "Tab1"}));
}

TEST_F(TabSharingStatusMessageViewTest, ReversedTwoButtonsPostfix) {
  TabSharingStatusMessageView view(
      MessageInfo(u"$2$1-postfix", {kTab1, kTab2}));
  EXPECT_THAT(GetChildTexts(view),
              ElementsAreArray({"Tab2", "Tab1", "-postfix"}));
}

TEST_F(TabSharingStatusMessageViewTest, ReversedTwoButtonsPrefixAndInfix) {
  TabSharingStatusMessageView view(
      MessageInfo(u"prefix-$2-infix-$1", {kTab1, kTab2}));
  EXPECT_THAT(GetChildTexts(view),
              ElementsAreArray({"prefix-", "Tab2", "-infix-", "Tab1"}));
}

TEST_F(TabSharingStatusMessageViewTest, ReversedTwoButtonsInfixAndPostfix) {
  TabSharingStatusMessageView view(
      MessageInfo(u"$2-infix-$1-postfix", {kTab1, kTab2}));
  EXPECT_THAT(GetChildTexts(view),
              ElementsAreArray({"Tab2", "-infix-", "Tab1", "-postfix"}));
}

TEST_F(TabSharingStatusMessageViewTest, ReversedTwoButtonsPrefixAndPostfix) {
  TabSharingStatusMessageView view(
      MessageInfo(u"prefix-$2$1-postfix", {kTab1, kTab2}));
  EXPECT_THAT(GetChildTexts(view),
              ElementsAreArray({"prefix-", "Tab2", "Tab1", "-postfix"}));
}

TEST_F(TabSharingStatusMessageViewTest,
       ReversedTwoButtonsPrefixAndInfixAndPostfix) {
  TabSharingStatusMessageView view(
      MessageInfo(u"prefix-$2-infix-$1-postfix", {kTab1, kTab2}));
  EXPECT_THAT(
      GetChildTexts(view),
      ElementsAreArray({"prefix-", "Tab2", "-infix-", "Tab1", "-postfix"}));
}

TEST_F(TabSharingStatusMessageViewTest, SpacesPrefix) {
  TabSharingStatusMessageView view(MessageInfo(u"   $1", {kTab1}));
  EXPECT_THAT(GetChildTexts(view), ElementsAreArray({"   ", "Tab1"}));
}

TEST_F(TabSharingStatusMessageViewTest, SpacesInfix) {
  TabSharingStatusMessageView view(MessageInfo(u"$1   $2", {kTab1, kTab2}));
  EXPECT_THAT(GetChildTexts(view), ElementsAreArray({"Tab1", "   ", "Tab2"}));
}

TEST_F(TabSharingStatusMessageViewTest, SpacesPostfix) {
  TabSharingStatusMessageView view(MessageInfo(u"$1   ", {kTab1}));
  EXPECT_THAT(GetChildTexts(view), ElementsAreArray({"Tab1", "   "}));
}

TEST_F(TabSharingStatusMessageViewTest, TwoEndpointsWithoutId) {
  TabSharingStatusMessageView view(
      MessageInfo(u"$1$2", {kWithoutId1, kWithoutId2}));
  EXPECT_THAT(GetChildTexts(view), ElementsAreArray({"WithoutId1WithoutId2"}));
}

TEST_F(TabSharingStatusMessageViewTest,
       TwoEndpointsWithoutIdPrefixAndInfixAndPostfix) {
  TabSharingStatusMessageView view(
      MessageInfo(u"prefix-$1-infix-$2-postfix", {kWithoutId1, kWithoutId2}));
  EXPECT_THAT(GetChildTexts(view),
              ElementsAreArray({"prefix-WithoutId1-infix-WithoutId2-postfix"}));
}

TEST_F(TabSharingStatusMessageViewTest, ReversedTwoEndpointsWithoutId) {
  TabSharingStatusMessageView view(
      MessageInfo(u"$2$1", {kWithoutId1, kWithoutId2}));
  EXPECT_THAT(GetChildTexts(view), ElementsAreArray({"WithoutId2WithoutId1"}));
}

TEST_F(TabSharingStatusMessageViewTest,
       ReversedTwoEndpointsWithoutIdPrefixAndInfixAndPostfix) {
  TabSharingStatusMessageView view(
      MessageInfo(u"prefix-$2-infix-$1-postfix", {kWithoutId1, kWithoutId2}));
  EXPECT_THAT(GetChildTexts(view),
              ElementsAreArray({"prefix-WithoutId2-infix-WithoutId1-postfix"}));
}
