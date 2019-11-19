// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/search_result_ranker/frecency_store.h"

#include "chrome/browser/ui/app_list/search/search_result_ranker/app_launch_predictor_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/protobuf/src/google/protobuf/stubs/mathutil.h"

using testing::_;
using testing::Contains;
using testing::ElementsAre;
using testing::Ne;
using testing::Pair;
using testing::UnorderedElementsAre;

namespace app_list {
namespace {

FrecencyStoreProto MakeTestingProto() {
  FrecencyStoreProto proto;

  proto.set_value_limit(70);
  proto.set_decay_coeff(0.5f);
  proto.set_num_updates(5);
  proto.set_next_id(4);

  auto* proto_values = proto.mutable_values();

  FrecencyStoreProto::ValueData value_data;
  value_data.set_id(0);
  value_data.set_last_score(0.53125f);
  value_data.set_last_num_updates(5);
  (*proto_values)["A"] = value_data;

  value_data = FrecencyStoreProto::ValueData();
  value_data.set_id(1);
  value_data.set_last_score(0.5f);
  value_data.set_last_num_updates(2);
  (*proto_values)["B"] = value_data;

  value_data = FrecencyStoreProto::ValueData();
  value_data.set_id(2);
  value_data.set_last_score(0.5f);
  value_data.set_last_num_updates(3);
  (*proto_values)["C"] = value_data;

  return proto;
}

MATCHER_P(ScoreEq, score, "") {
  return google::protobuf::MathUtil::AlmostEquals(arg.last_score, score);
}

MATCHER_P(IdNe, id, "") {
  return arg.id != id;
}

MATCHER_P(IdEq, id, "") {
  return arg.id == id;
}

}  // namespace

TEST(FrecencyStoreTest, AddOneValueOnce) {
  FrecencyStore store(100, 0.5f);
  store.Update("testing");

  EXPECT_THAT(store.GetAll(), ElementsAre(Pair("testing", ScoreEq(0.5f))));
}

TEST(FrecencyStoreTest, AddOneValueManyTimes) {
  FrecencyStore store(100, 0.5f);
  store.Update("testing");
  store.Update("testing");
  store.Update("testing");
  store.Update("testing");

  EXPECT_THAT(store.GetAll(), ElementsAre(Pair("testing", ScoreEq(0.9375f))));
}

TEST(FrecencyStoreTest, AddManyValues) {
  FrecencyStore store(100, 0.5f);
  store.Update("first");
  store.Update("second");
  store.Update("first");
  store.Update("second");

  // Ensure IDs are unique.
  EXPECT_NE(store.GetId("first"), store.GetId("second"));
  EXPECT_THAT(store.GetAll(),
              UnorderedElementsAre(Pair("first", ScoreEq(0.3125f)),
                                   Pair("second", ScoreEq(0.625f))));
}

TEST(FrecencyStoreTest, OneValueDecays) {
  FrecencyStore store(100, 0.9f);
  store.Update("decaying");
  for (int i = 0; i < 5; i++)
    store.Update("updated");

  // The value's score should have decreased, but is still above the threshold
  // so should not have been cleaned up.
  EXPECT_THAT(store.GetAll(), Contains(Pair("decaying", ScoreEq(0.059049f))));
}

TEST(FrecencyStoreTest, DecayedValuesAreRemoved) {
  FrecencyStore store(100, 0.1f);

  store.Update("decaying A");
  store.Update("decaying B");

  for (int i = 0; i < 50; i++) {
    store.Update("updating A");
    store.Update("updating B");
  }

  // The store should have cleaned up the decaying values.
  EXPECT_THAT(store.GetAll(), UnorderedElementsAre(Pair("updating A", _),
                                                   Pair("updating B", _)));

  // Updating the decayed value should return it to the results.
  store.Update("decaying A");
  EXPECT_THAT(store.GetAll(),
              UnorderedElementsAre(Pair("updating A", _), Pair("updating B", _),
                                   Pair("decaying A", _)));
}

TEST(FrecencyStoreTest, CleanupOnOverflow) {
  FrecencyStore store(5, 0.9999f);

  // |value_limit_| is 5, so cleanups should occur at 10, 20, ..., 50 values.
  for (int i = 0; i <= 50; i++) {
    store.Update(std::to_string(i));
  }

  // A cleanup just happened, so we should have only 45-50 stored. This is six
  // values because the cleanup happens before inserting the new value.
  EXPECT_THAT(store.GetAll(), UnorderedElementsAre(
                                  Pair("45", _), Pair("46", _), Pair("47", _),
                                  Pair("48", _), Pair("49", _), Pair("50", _)));
}

TEST(FrecencyStoreTest, RenameValue) {
  FrecencyStore store(100, 0.5f);

  // Value to be renamed.
  store.Update("original");
  unsigned int id_before_rename = store.GetId("original").value();

  // Some extra values to check they don't get overridden.
  store.Update("unused A");
  store.Update("unused B");

  // Rename the first value.
  store.Rename("original", "intermediate");
  store.Rename("intermediate", "renamed");
  unsigned int id_after_rename = store.GetId("renamed").value();

  // Check the rename didn't change the ID.
  EXPECT_EQ(id_before_rename, id_after_rename);

  // Check the rename worked, didn't touch other values, and kept the same
  // score.
  EXPECT_THAT(store.GetAll(),
              UnorderedElementsAre(Pair("renamed", _), Pair("unused A", _),
                                   Pair("unused B", _)));
}

TEST(FrecencyStoreTest, DuplicateRenameOverridesDestination) {
  FrecencyStore store(100, 0.5f);

  store.Update("A");
  unsigned int id_before_rename = store.GetId("A").value();

  store.Update("B");
  store.Update("B");
  store.Update("B");
  store.Rename("A", "B");
  unsigned int id_after_rename = store.GetId("B").value();

  EXPECT_EQ(id_before_rename, id_after_rename);
  EXPECT_THAT(store.GetAll(), ElementsAre(Pair("B", ScoreEq(0.0625f))));
}

TEST(FrecencyStoreTest, NonexistentRenameDoesNothing) {
  FrecencyStore store(100, 0.5f);
  store.Rename("A", "B");
  EXPECT_TRUE(store.GetAll().empty());
}

TEST(FrecencyStoreTest, RemoveValue) {
  FrecencyStore store(100, 0.5f);

  // Value to remove.
  store.Update("deleteme");
  unsigned int deleted_id = store.GetId("deleteme").value();

  // Some extra values to check they don't get changed.
  store.Update("unused A");
  store.Update("unused B");

  store.Remove("deleteme");

  EXPECT_THAT(store.GetAll(),
              UnorderedElementsAre(Pair("unused A", IdNe(deleted_id)),
                                   Pair("unused B", IdNe(deleted_id))));

  // Add new values and ensure the old ID is not used.
  store.Update("new A");
  store.Update("new B");
  store.Update("new C");
  EXPECT_NE(store.GetId("new A").value(), deleted_id);
  EXPECT_NE(store.GetId("new B").value(), deleted_id);
  EXPECT_NE(store.GetId("new C").value(), deleted_id);
}

TEST(FrecencyStoreTest, NonexistentRemoveDoesNothing) {
  FrecencyStore store(100, 0.5f);
  store.Remove("A");
  EXPECT_TRUE(store.GetAll().empty());
}

TEST(FrecencyStoreTest, GetIdGetsCorrectId) {
  FrecencyStore store(100, 0.5f);
  store.Update("A");
  store.Update("B");
  store.Update("C");
  store.Update("D");
  store.Update("E");
  EXPECT_EQ(store.GetId("C").value(), 2U);
}

TEST(FrecencyStoreTest, InvalidGetIdReturnsNullopt) {
  FrecencyStore store(100, 0.5f);
  EXPECT_EQ(store.GetId("not found"), base::nullopt);
}

TEST(FrecencyStoreTest, GetAllGetsAll) {
  FrecencyStore store(100, 0.5f);
  store.Update("A");
  store.Update("B");
  store.Update("C");
  store.Update("D");
  store.Update("E");

  EXPECT_THAT(store.GetAll(),
              UnorderedElementsAre(Pair("A", IdEq(0U)), Pair("B", IdEq(1U)),
                                   Pair("C", IdEq(2U)), Pair("D", IdEq(3U)),
                                   Pair("E", IdEq(4U))));
}

TEST(FrecencyStoreTest, GetAllDoesValidCleanup) {
  FrecencyStore store(100, 0.01f);

  // We update enough times that the scores for these should fall to the
  // threshold and be cleaned up on the GetAll call.
  store.Update("clean me 1");
  store.Update("clean me 2");
  store.Update("clean me 3");

  for (int i = 0; i < 100; ++i)
    store.Update("keep me");

  EXPECT_THAT(store.GetAll(), UnorderedElementsAre(Pair("keep me", _)));
}

TEST(FrecencyStoreTest, ToProto) {
  FrecencyStore store(70, 0.5f);
  store.Update("D");
  store.Update("B");
  store.Update("C");
  store.Update("A");
  store.Update("D");
  store.Remove("A");
  store.Rename("D", "A");

  FrecencyStoreProto proto;
  store.ToProto(&proto);
  EXPECT_TRUE(EquivToProtoLite(proto, MakeTestingProto()));
}

TEST(FrecencyStoreTest, FromProtoAndToProto) {
  // Check FromProto by doing a round-trip: call FromProto and ToProto, and
  // check the output is the same as the input. We do this because
  // |FrecencyStore| doesn't expose its internals any other way.

  FrecencyStore store(100, 0.5f);
  FrecencyStoreProto input = MakeTestingProto();
  store.FromProto(input);
  FrecencyStoreProto output;
  store.ToProto(&output);
  EXPECT_TRUE(EquivToProtoLite(output, input));
}

}  // namespace app_list
