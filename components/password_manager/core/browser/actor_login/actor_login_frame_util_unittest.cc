// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/actor_login/actor_login_frame_util.h"

#include "components/password_manager/core/browser/stub_password_manager_driver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace actor_login {
namespace {

using ::testing::Return;
using ::testing::ReturnRef;

class MockPasswordManagerDriver
    : public password_manager::StubPasswordManagerDriver {
 public:
  MockPasswordManagerDriver() = default;
  ~MockPasswordManagerDriver() override = default;

  MOCK_METHOD(bool, IsInPrimaryMainFrame, (), (const, override));
  MOCK_METHOD(bool, IsDirectChildOfPrimaryMainFrame, (), (const, override));
  MOCK_METHOD(bool, IsNestedWithinFencedFrame, (), (const, override));
  MOCK_METHOD(bool, HasCrossOriginAncestor, (), (const, override));
  MOCK_METHOD(const url::Origin&,
              GetLastCommittedOrigin,
              (),
              (const, override));
};

TEST(ActorLoginFrameUtilTest, IsFormOriginSupported) {
  const url::Origin main_origin =
      url::Origin::Create(GURL("https://example.com"));

  EXPECT_TRUE(IsFormOriginSupported(
      url::Origin::Create(GURL("https://example.com")), main_origin));
  EXPECT_TRUE(IsFormOriginSupported(
      url::Origin::Create(GURL("https://login.example.com")), main_origin));
  EXPECT_TRUE(IsFormOriginSupported(
      url::Origin::Create(GURL("https://sub.login.example.com")), main_origin));

  EXPECT_FALSE(IsFormOriginSupported(
      url::Origin::Create(GURL("https://notexample.com")), main_origin));
  EXPECT_FALSE(IsFormOriginSupported(url::Origin(), main_origin));
}

TEST(ActorLoginFrameUtilTest, IsValidFrameAndOriginToFill_ExplicitParams) {
  const url::Origin main_origin =
      url::Origin::Create(GURL("https://example.com"));
  const url::Origin same_domain_origin =
      url::Origin::Create(GURL("https://login.example.com"));
  const url::Origin cross_site_origin =
      url::Origin::Create(GURL("https://other.com"));

  // Primary main frame.
  EXPECT_TRUE(
      IsValidFrameAndOriginToFill(main_origin, main_origin,
                                  /*is_nested_within_fenced_frame=*/false,
                                  /*is_in_primary_main_frame=*/true,
                                  /*is_direct_child=*/false,
                                  /*has_cross_origin_ancestor=*/false));

  // Direct child of primary main frame (supported origin, no cross-origin
  // ancestor).
  EXPECT_TRUE(
      IsValidFrameAndOriginToFill(same_domain_origin, main_origin,
                                  /*is_nested_within_fenced_frame=*/false,
                                  /*is_in_primary_main_frame=*/false,
                                  /*is_direct_child=*/true,
                                  /*has_cross_origin_ancestor=*/false));

  // Direct child of primary main frame with cross-origin ancestor (allowed for
  // direct children).
  EXPECT_TRUE(
      IsValidFrameAndOriginToFill(same_domain_origin, main_origin,
                                  /*is_nested_within_fenced_frame=*/false,
                                  /*is_in_primary_main_frame=*/false,
                                  /*is_direct_child=*/true,
                                  /*has_cross_origin_ancestor=*/true));

  // Nested frame with no cross-origin ancestor.
  EXPECT_TRUE(
      IsValidFrameAndOriginToFill(same_domain_origin, main_origin,
                                  /*is_nested_within_fenced_frame=*/false,
                                  /*is_in_primary_main_frame=*/false,
                                  /*is_direct_child=*/false,
                                  /*has_cross_origin_ancestor=*/false));

  // Nested frame with cross-origin ancestor is rejected.
  EXPECT_FALSE(
      IsValidFrameAndOriginToFill(same_domain_origin, main_origin,
                                  /*is_nested_within_fenced_frame=*/false,
                                  /*is_in_primary_main_frame=*/false,
                                  /*is_direct_child=*/false,
                                  /*has_cross_origin_ancestor=*/true));

  // Fenced frame is rejected.
  EXPECT_FALSE(
      IsValidFrameAndOriginToFill(main_origin, main_origin,
                                  /*is_nested_within_fenced_frame=*/true,
                                  /*is_in_primary_main_frame=*/true,
                                  /*is_direct_child=*/false,
                                  /*has_cross_origin_ancestor=*/false));

  // Unsupported origin is rejected.
  EXPECT_FALSE(
      IsValidFrameAndOriginToFill(cross_site_origin, main_origin,
                                  /*is_nested_within_fenced_frame=*/false,
                                  /*is_in_primary_main_frame=*/true,
                                  /*is_direct_child=*/false,
                                  /*has_cross_origin_ancestor=*/false));
}

TEST(ActorLoginFrameUtilTest, IsValidFrameAndOriginToFill_Driver_NullDriver) {
  const url::Origin main_origin =
      url::Origin::Create(GURL("https://example.com"));
  EXPECT_FALSE(IsValidFrameAndOriginToFill(nullptr, main_origin));
}

TEST(ActorLoginFrameUtilTest,
     IsValidFrameAndOriginToFill_Driver_PrimaryMainFrame) {
  const url::Origin main_origin =
      url::Origin::Create(GURL("https://example.com"));
  MockPasswordManagerDriver mock_driver;
  EXPECT_CALL(mock_driver, IsNestedWithinFencedFrame).WillOnce(Return(false));
  EXPECT_CALL(mock_driver, GetLastCommittedOrigin)
      .WillRepeatedly(ReturnRef(main_origin));
  EXPECT_CALL(mock_driver, IsInPrimaryMainFrame).WillOnce(Return(true));
  EXPECT_CALL(mock_driver, IsDirectChildOfPrimaryMainFrame)
      .WillOnce(Return(false));
  EXPECT_CALL(mock_driver, HasCrossOriginAncestor)
      .WillRepeatedly(Return(false));
  EXPECT_TRUE(IsValidFrameAndOriginToFill(&mock_driver, main_origin));
}

TEST(ActorLoginFrameUtilTest,
     IsValidFrameAndOriginToFill_Driver_FencedFrameRejected) {
  const url::Origin main_origin =
      url::Origin::Create(GURL("https://example.com"));
  MockPasswordManagerDriver mock_driver;
  EXPECT_CALL(mock_driver, IsNestedWithinFencedFrame).WillOnce(Return(true));
  EXPECT_CALL(mock_driver, GetLastCommittedOrigin)
      .WillRepeatedly(ReturnRef(main_origin));
  EXPECT_CALL(mock_driver, IsInPrimaryMainFrame).WillRepeatedly(Return(false));
  EXPECT_CALL(mock_driver, IsDirectChildOfPrimaryMainFrame)
      .WillRepeatedly(Return(false));
  EXPECT_CALL(mock_driver, HasCrossOriginAncestor)
      .WillRepeatedly(Return(false));
  EXPECT_FALSE(IsValidFrameAndOriginToFill(&mock_driver, main_origin));
}

TEST(ActorLoginFrameUtilTest,
     IsValidFrameAndOriginToFill_Driver_UnsupportedOriginRejected) {
  const url::Origin main_origin =
      url::Origin::Create(GURL("https://example.com"));
  const url::Origin cross_site_origin =
      url::Origin::Create(GURL("https://other.com"));
  MockPasswordManagerDriver mock_driver;
  EXPECT_CALL(mock_driver, IsNestedWithinFencedFrame).WillOnce(Return(false));
  EXPECT_CALL(mock_driver, GetLastCommittedOrigin)
      .WillRepeatedly(ReturnRef(cross_site_origin));
  EXPECT_CALL(mock_driver, IsInPrimaryMainFrame).WillRepeatedly(Return(false));
  EXPECT_CALL(mock_driver, IsDirectChildOfPrimaryMainFrame)
      .WillRepeatedly(Return(false));
  EXPECT_CALL(mock_driver, HasCrossOriginAncestor)
      .WillRepeatedly(Return(false));
  EXPECT_FALSE(IsValidFrameAndOriginToFill(&mock_driver, main_origin));
}

TEST(ActorLoginFrameUtilTest,
     IsValidFrameAndOriginToFill_Driver_NestedCrossOriginAncestorRejected) {
  const url::Origin main_origin =
      url::Origin::Create(GURL("https://example.com"));
  const url::Origin same_domain_origin =
      url::Origin::Create(GURL("https://login.example.com"));
  MockPasswordManagerDriver mock_driver;
  EXPECT_CALL(mock_driver, IsNestedWithinFencedFrame).WillOnce(Return(false));
  EXPECT_CALL(mock_driver, GetLastCommittedOrigin)
      .WillRepeatedly(ReturnRef(same_domain_origin));
  EXPECT_CALL(mock_driver, IsInPrimaryMainFrame).WillOnce(Return(false));
  EXPECT_CALL(mock_driver, IsDirectChildOfPrimaryMainFrame)
      .WillOnce(Return(false));
  EXPECT_CALL(mock_driver, HasCrossOriginAncestor).WillRepeatedly(Return(true));
  EXPECT_FALSE(IsValidFrameAndOriginToFill(&mock_driver, main_origin));
}

}  // namespace
}  // namespace actor_login
