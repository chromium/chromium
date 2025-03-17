// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/collaboration/public/messaging/util.h"

#include "base/i18n/number_formatting.h"
#include "base/strings/utf_string_conversions.h"
#include "components/strings/grit/components_strings.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace collaboration::messaging {

// Add tests for GetRemovedCollaborationsSummary.
namespace {

// Creates a PersistentMessage for testing.
PersistentMessage CreateMessage(
    PersistentNotificationType type,
    CollaborationEvent collaboration_event,
    std::optional<std::string> last_known_title = std::nullopt) {
  PersistentMessage message;
  message.type = type;
  message.collaboration_event = collaboration_event;
  if (last_known_title.has_value()) {
    message.attribution.tab_group_metadata = TabGroupMessageMetadata();
    message.attribution.tab_group_metadata->last_known_title = last_known_title;
  }
  return message;
}

// Creates a PersistentMessage about a tab update.
PersistentMessage TabUpdateMessage() {
  return CreateMessage(PersistentNotificationType::DIRTY_TAB,
                       CollaborationEvent::TAB_UPDATED);
}

// Creates a PersistentMessage about a tab group update.
PersistentMessage TabGroupUpdateMessage() {
  return CreateMessage(PersistentNotificationType::DIRTY_TAB_GROUP,
                       CollaborationEvent::TAB_GROUP_NAME_UPDATED);
}

// Creates a PersistentMessage about a tab group membership update.
PersistentMessage CollaborationUpdateMessage() {
  return CreateMessage(PersistentNotificationType::DIRTY_TAB_GROUP,
                       CollaborationEvent::COLLABORATION_MEMBER_ADDED);
}

// Creates a PersistentMessage about a removed tab group.
PersistentMessage TabGroupRemovedMessage(std::string last_known_title) {
  return CreateMessage(PersistentNotificationType::TOMBSTONED,
                       CollaborationEvent::TAB_GROUP_REMOVED, last_known_title);
}

}  // namespace

// Checks the summary is not provided when there are no messages.
TEST(CollaborationMessagingUtilTest,
     GetRemovedCollaborationsSummary_NoMessages) {
  std::vector<PersistentMessage> messages;
  EXPECT_EQ(GetRemovedCollaborationsSummary(messages), std::nullopt);
}

// Checks the summary is not provided when there are no removed collaboration
// messages, only a message related to a tab change.
TEST(CollaborationMessagingUtilTest,
     GetRemovedCollaborationsSummary_NoRemovedCollaborationMessage) {
  std::vector<PersistentMessage> messages;
  messages.push_back(TabUpdateMessage());
  messages.push_back(TabGroupUpdateMessage());
  messages.push_back(CollaborationUpdateMessage());
  EXPECT_EQ(GetRemovedCollaborationsSummary(messages), std::nullopt);
}

// Checks the summary is provided and correct when there is one removed
// collaboration message about a group with a last known title.
TEST(CollaborationMessagingUtilTest,
     GetRemovedCollaborationsSummary_OneRemovedNamedCollaborationMessage) {
  std::vector<PersistentMessage> messages;
  messages.push_back(TabUpdateMessage());
  messages.push_back(TabGroupRemovedMessage("title1"));
  messages.push_back(TabGroupUpdateMessage());
  messages.push_back(CollaborationUpdateMessage());
  EXPECT_EQ(GetRemovedCollaborationsSummary(messages),
            l10n_util::GetStringFUTF8(
                IDS_COLLABORATION_ONE_GROUP_REMOVED_NOTIFICATION, u"title1"));
}

// Checks the summary is provided and correct when there is one removed
// collaboration message about a group with an empty last known title.
TEST(CollaborationMessagingUtilTest,
     GetRemovedCollaborationsSummary_OneRemovedUnnamedCollaborationMessage) {
  std::vector<PersistentMessage> messages;
  messages.push_back(TabUpdateMessage());
  messages.push_back(TabGroupRemovedMessage(""));
  messages.push_back(TabGroupUpdateMessage());
  messages.push_back(CollaborationUpdateMessage());
  EXPECT_EQ(GetRemovedCollaborationsSummary(messages),
            l10n_util::GetStringUTF8(
                IDS_COLLABORATION_ONE_UNNAMED_GROUP_REMOVED_NOTIFICATION));
}

// Checks the summary is provided and correct when there are two removed
// collaboration messages about groups with last known titles.
TEST(CollaborationMessagingUtilTest,
     GetRemovedCollaborationsSummary_TwoMessagesNamedGroups) {
  std::vector<PersistentMessage> messages;
  messages.push_back(TabUpdateMessage());
  messages.push_back(TabGroupRemovedMessage("title1"));
  messages.push_back(TabGroupUpdateMessage());
  messages.push_back(TabGroupRemovedMessage("title2"));
  messages.push_back(CollaborationUpdateMessage());
  EXPECT_EQ(GetRemovedCollaborationsSummary(messages),
            l10n_util::GetStringFUTF8(
                IDS_COLLABORATION_TWO_GROUPS_REMOVED_NOTIFICATION, u"title1",
                u"title2"));
}

// Checks the summary is provided and correct when there are two removed
// collaboration messages about groups with one with no last known title.
TEST(CollaborationMessagingUtilTest,
     GetRemovedCollaborationsSummary_TwoMessagesOneNamedOneUnnamedGroups) {
  std::vector<PersistentMessage> messages;
  messages.push_back(TabUpdateMessage());
  messages.push_back(TabGroupRemovedMessage("title1"));
  messages.push_back(TabGroupUpdateMessage());
  messages.push_back(TabGroupRemovedMessage(""));
  messages.push_back(CollaborationUpdateMessage());
  EXPECT_EQ(GetRemovedCollaborationsSummary(messages),
            l10n_util::GetStringFUTF8(
                IDS_COLLABORATION_SEVERAL_GROUPS_REMOVED_NOTIFICATION,
                base::FormatNumber(2)));
}

// Checks the summary is provided and correct when there are two removed
// collaboration messages about groups with no last known titles.
TEST(CollaborationMessagingUtilTest,
     GetRemovedCollaborationsSummary_TwoMessagesUnnamedGroups) {
  std::vector<PersistentMessage> messages;
  messages.push_back(TabUpdateMessage());
  messages.push_back(TabGroupRemovedMessage(""));
  messages.push_back(TabGroupUpdateMessage());
  messages.push_back(TabGroupRemovedMessage(""));
  messages.push_back(CollaborationUpdateMessage());
  EXPECT_EQ(GetRemovedCollaborationsSummary(messages),
            l10n_util::GetStringFUTF8(
                IDS_COLLABORATION_SEVERAL_GROUPS_REMOVED_NOTIFICATION,
                base::FormatNumber(2)));
}

// Checks the summary is provided and correct when there are more than two
// removed collaboration messages.
TEST(CollaborationMessagingUtilTest,
     GetRemovedCollaborationsSummary_SeveralMessages) {
  std::vector<PersistentMessage> messages;
  messages.push_back(TabUpdateMessage());
  messages.push_back(TabGroupRemovedMessage("title1"));
  messages.push_back(TabGroupUpdateMessage());
  messages.push_back(TabUpdateMessage());
  messages.push_back(TabGroupRemovedMessage("title2"));
  messages.push_back(CollaborationUpdateMessage());
  messages.push_back(TabGroupRemovedMessage("title3"));
  messages.push_back(CollaborationUpdateMessage());
  EXPECT_EQ(GetRemovedCollaborationsSummary(messages),
            l10n_util::GetStringFUTF8(
                IDS_COLLABORATION_SEVERAL_GROUPS_REMOVED_NOTIFICATION,
                base::FormatNumber(3)));
}

}  // namespace collaboration::messaging
