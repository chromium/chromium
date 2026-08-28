// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_string.h"

#include <string>
#include <utility>

#include "base/test/scoped_feature_list.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "crypto/process_bound_string.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {

namespace {

class PasswordStringTest : public ::testing::TestWithParam<bool> {
 public:
  PasswordStringTest() {
    if (UseProcessBoundBacking()) {
      scoped_feature_list_.InitAndEnableFeature(
          features::kUseProcessBoundPasswordString);
    } else {
      scoped_feature_list_.InitAndDisableFeature(
          features::kUseProcessBoundPasswordString);
    }
  }

  bool UseProcessBoundBacking() const { return GetParam(); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_P(PasswordStringTest, DefaultConstructedIsEmpty) {
  PasswordString ps;
  EXPECT_TRUE(ps.empty());
  EXPECT_EQ(0u, ps.size());
  EXPECT_EQ(std::u16string(), ps.value());
}

TEST_P(PasswordStringTest, ValueRoundTrips) {
  const std::u16string kPassword = u"hunter2";
  PasswordString ps{std::u16string(kPassword)};
  EXPECT_FALSE(ps.empty());
  EXPECT_EQ(kPassword.size(), ps.size());
  EXPECT_EQ(kPassword, ps.value());
}

TEST_P(PasswordStringTest, SecureValueRoundTrips) {
  const std::u16string kPassword = u"correct horse battery staple";
  PasswordString ps{std::u16string(kPassword)};
  crypto::SecureU16String secure = ps.secure_value();
  EXPECT_EQ(kPassword.size(), secure.size());
  EXPECT_EQ(std::u16string(secure.begin(), secure.end()), kPassword);
}

TEST_P(PasswordStringTest, EmptyPasswordRoundTrips) {
  PasswordString ps{std::u16string()};
  EXPECT_TRUE(ps.empty());
  EXPECT_EQ(0u, ps.size());
  EXPECT_EQ(std::u16string(), ps.value());
  EXPECT_TRUE(ps.secure_value().empty());
}

TEST_P(PasswordStringTest, EqualityMatchesForSameValue) {
  PasswordString a(u"same");
  PasswordString b(u"same");
  crypto::SecureU16String c(u"same");
  std::u16string d(u"same");
  EXPECT_EQ(a, b);
  EXPECT_EQ(a, c);
  EXPECT_EQ(a, d);
}

TEST_P(PasswordStringTest, EqualityRejectsDifferentValues) {
  PasswordString a(u"one");
  PasswordString b(u"two");
  EXPECT_FALSE(a == b);
}

// Moving must hand the password over completely: the moved-from object has to
// be left empty and still readable, rather than retaining the password's length
// or a buffer it can no longer decrypt.
TEST_P(PasswordStringTest, MoveConstructionClearsSource) {
  PasswordString source(u"hunter2");

  PasswordString moved(std::move(source));

  EXPECT_EQ(moved, u"hunter2");

  // Intentional use-after-move to validate the password is wiped out on move.
  // NOLINTNEXTLINE(bugprone-use-after-move)
  EXPECT_TRUE(source.empty());
  // NOLINTNEXTLINE(bugprone-use-after-move)
  EXPECT_EQ(0u, source.size());
  // NOLINTNEXTLINE(bugprone-use-after-move)
  EXPECT_EQ(std::u16string(), source.value());
  // NOLINTNEXTLINE(bugprone-use-after-move)
  EXPECT_TRUE(source.secure_value().empty());
}

TEST_P(PasswordStringTest, MoveAssignmentClearsSource) {
  PasswordString source(u"hunter2");
  PasswordString moved(u"placeholder");

  moved = std::move(source);

  EXPECT_EQ(moved, u"hunter2");

  // Intentional use-after-move to validate the password is wiped out on move.
  // NOLINTNEXTLINE(bugprone-use-after-move)
  EXPECT_TRUE(source.empty());
  // NOLINTNEXTLINE(bugprone-use-after-move)
  EXPECT_EQ(0u, source.size());
  // NOLINTNEXTLINE(bugprone-use-after-move)
  EXPECT_EQ(std::u16string(), source.value());
  // NOLINTNEXTLINE(bugprone-use-after-move)
  EXPECT_TRUE(source.secure_value().empty());
}

TEST_P(PasswordStringTest, MovedFromCanBeAssignedAgain) {
  PasswordString source(u"hunter2");
  PasswordString moved(std::move(source));

  // Intentional use-after-move to validate the behavior of the password_value
  // field being reassigned after being moved out of.
  // NOLINTNEXTLINE(bugprone-use-after-move)
  source = PasswordString(u"reused");

  EXPECT_EQ(source, u"reused");
  EXPECT_EQ(6u, source.size());

  EXPECT_EQ(moved, u"hunter2");
}

TEST_P(PasswordStringTest, SelfMoveAssignmentKeepsValue) {
  PasswordString ps(u"hunter2");

  // Indirect through a pointer so this is not diagnosed as a self-move.
  PasswordString* alias = &ps;
  ps = std::move(*alias);

  EXPECT_EQ(ps, u"hunter2");
  EXPECT_FALSE(ps.empty());
}

TEST_P(PasswordStringTest, CopyLeavesSourceIntactAndIsIndependent) {
  PasswordString source(u"hunter2");

  PasswordString copy(source);

  EXPECT_EQ(copy, u"hunter2");
  EXPECT_EQ(source, u"hunter2");

  copy.clear();
  EXPECT_TRUE(copy.empty());
  EXPECT_EQ(source, u"hunter2");
}

INSTANTIATE_TEST_SUITE_P(FlagStates,
                         PasswordStringTest,
                         ::testing::Values(false, true));

}  // namespace

}  // namespace password_manager
