// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/critical_actions/core/browser/critical_action_history_utils.h"

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "base/time/time.h"
#include "components/critical_actions/core/browser/critical_action_types.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace critical_actions {

namespace {

using ::testing::ElementsAre;
using ::testing::IsEmpty;

const base::Time kNow = base::Time::FromSecondsSinceUnixEpoch(1'700'000'000);

CriticalActionEntry CreateAction(std::string id,
                                 int64_t visit_id,
                                 base::Time timestamp,
                                 ActionType type = ActionType::kFormFill,
                                 std::string conversation_id = "conversation",
                                 std::string actor_task_id = "task") {
  // Populated directly rather than via CriticalActionEntry::Builder, which
  // rejects ActionType::kUnknown; such rows can still come from the database.
  CriticalActionEntry entry;
  entry.critical_action_id = std::move(id);
  entry.visit_id = visit_id;
  entry.timestamp = timestamp;
  entry.action_type = type;
  entry.action_source = ActionSource::kActor;
  entry.conversation_id = std::move(conversation_id);
  entry.actor_task_id = std::move(actor_task_id);
  return entry;
}

std::vector<std::string> IdsForVisit(const CriticalActionsByVisitId& actions,
                                     int64_t visit_id) {
  std::vector<std::string> ids;
  auto it = actions.find(visit_id);
  if (it != actions.end()) {
    for (const CriticalActionEntry& action : it->second) {
      ids.push_back(action.critical_action_id);
    }
  }
  return ids;
}

}  // namespace

TEST(CriticalActionHistoryUtilsTest, GroupEmpty) {
  EXPECT_THAT(GroupCriticalActionsByVisitId({}), IsEmpty());
}

TEST(CriticalActionHistoryUtilsTest, GroupsByVisitIdKeepingOrder) {
  CriticalActionsByVisitId result = GroupCriticalActionsByVisitId({
      CreateAction("a", /*visit_id=*/1, kNow, ActionType::kFormFill),
      CreateAction("b", /*visit_id=*/2, kNow - base::Seconds(1)),
      CreateAction("c", /*visit_id=*/1, kNow - base::Seconds(2),
                   ActionType::kGooglePasswordManager),
  });

  EXPECT_EQ(result.size(), 2u);
  EXPECT_THAT(IdsForVisit(result, 1), ElementsAre("a", "c"));
  EXPECT_THAT(IdsForVisit(result, 2), ElementsAre("b"));
}

TEST(CriticalActionHistoryUtilsTest, DropsUnknownActions) {
  CriticalActionsByVisitId result = GroupCriticalActionsByVisitId({
      CreateAction("unknown", /*visit_id=*/1, kNow, ActionType::kUnknown),
      CreateAction("known", /*visit_id=*/1, kNow - base::Seconds(1)),
  });

  EXPECT_THAT(IdsForVisit(result, 1), ElementsAre("known"));
}

TEST(CriticalActionHistoryUtilsTest, DropsVisitWithOnlyUnknownActions) {
  CriticalActionsByVisitId result = GroupCriticalActionsByVisitId({
      CreateAction("unknown", /*visit_id=*/1, kNow, ActionType::kUnknown),
  });

  EXPECT_THAT(result, IsEmpty());
}

TEST(CriticalActionHistoryUtilsTest, MergesDuplicatesWithinTolerance) {
  CriticalActionsByVisitId result = GroupCriticalActionsByVisitId({
      CreateAction("latest", /*visit_id=*/1, kNow),
      CreateAction("dup", /*visit_id=*/1, kNow - base::Seconds(3)),
      CreateAction("dup_at_edge", /*visit_id=*/1, kNow - base::Seconds(5)),
  });

  EXPECT_THAT(IdsForVisit(result, 1), ElementsAre("latest"));
}

TEST(CriticalActionHistoryUtilsTest, KeepsActionsOutsideTolerance) {
  CriticalActionsByVisitId result = GroupCriticalActionsByVisitId({
      CreateAction("latest", /*visit_id=*/1, kNow),
      CreateAction("just_outside", /*visit_id=*/1,
                   kNow - base::Seconds(5) - base::Milliseconds(1)),
  });

  EXPECT_THAT(IdsForVisit(result, 1), ElementsAre("latest", "just_outside"));
}

// The tolerance window is relative to the most recently *kept* action, so a
// chain of closely spaced logs does not collapse into a single action.
TEST(CriticalActionHistoryUtilsTest, ToleranceIsRelativeToLastKeptAction) {
  CriticalActionsByVisitId result = GroupCriticalActionsByVisitId({
      CreateAction("a", /*visit_id=*/1, kNow),
      CreateAction("b", /*visit_id=*/1, kNow - base::Seconds(4)),
      CreateAction("c", /*visit_id=*/1, kNow - base::Seconds(8)),
  });

  EXPECT_THAT(IdsForVisit(result, 1), ElementsAre("a", "c"));
}

TEST(CriticalActionHistoryUtilsTest, RespectsCustomTolerance) {
  CriticalActionsByVisitId result = GroupCriticalActionsByVisitId(
      {
          CreateAction("a", /*visit_id=*/1, kNow),
          CreateAction("b", /*visit_id=*/1, kNow - base::Seconds(3)),
      },
      /*dedup_tolerance=*/base::Seconds(1));

  EXPECT_THAT(IdsForVisit(result, 1), ElementsAre("a", "b"));
}

TEST(CriticalActionHistoryUtilsTest, DoesNotMergeDifferentKeys) {
  const base::Time t = kNow - base::Seconds(1);
  CriticalActionsByVisitId result = GroupCriticalActionsByVisitId({
      CreateAction("base", /*visit_id=*/1, kNow),
      CreateAction("other_type", /*visit_id=*/1, t,
                   ActionType::kGooglePasswordManager),
      CreateAction("other_conversation", /*visit_id=*/1, t,
                   ActionType::kFormFill, "conversation2"),
      CreateAction("other_task", /*visit_id=*/1, t, ActionType::kFormFill,
                   "conversation", "task2"),
      CreateAction("other_visit", /*visit_id=*/2, t),
  });

  EXPECT_THAT(
      IdsForVisit(result, 1),
      ElementsAre("base", "other_type", "other_conversation", "other_task"));
  EXPECT_THAT(IdsForVisit(result, 2), ElementsAre("other_visit"));
}

}  // namespace critical_actions
