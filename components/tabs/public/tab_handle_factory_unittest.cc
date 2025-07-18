// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/tabs/public/tab_handle_factory.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace tabs {

namespace {

// For convenience.
using TestHandle = SupportsHandles<SessionMappedTabHandleFactory>::Handle;

// A test class that uses the session-mapped handle factory. This avoids needing
// to include tab_interface.h and mock out its many pure virtual methods.
class TestSupportsTabHandles : public SupportsTabHandles {
 public:
  TestSupportsTabHandles() = default;
  ~TestSupportsTabHandles() override = default;

  // Expose the protected SetSessionId for testing.
  void SetSessionId(int32_t session_id) {
    SupportsTabHandles::SetSessionId(session_id);
  }

  // Expose the protected ClearSessionId for testing.
  void ClearSessionId() { SupportsTabHandles::ClearSessionId(); }
};

using SessionMappedTabHandleFactoryTest = testing::Test;

TEST_F(SessionMappedTabHandleFactoryTest, GetHandleForSessionId) {
  TestSupportsTabHandles tab;
  const int32_t session_id = 1;
  tab.SetSessionId(session_id);

  auto* const factory = &SessionMappedTabHandleFactory::GetInstance();
  TestHandle handle = tab.GetHandle();

  EXPECT_NE(handle.raw_value(), TestHandle::NullValue);
  EXPECT_EQ(handle.raw_value(), factory->GetHandleForSessionId(session_id));
}

TEST_F(SessionMappedTabHandleFactoryTest, GetHandleForDestroyedObject) {
  const int32_t session_id = 1;
  auto* const factory = &SessionMappedTabHandleFactory::GetInstance();

  {
    TestSupportsTabHandles tab;
    tab.SetSessionId(session_id);
    // `tab` is destroyed when this scope exits. The factory's mapping should
    // be cleared in the `SupportsHandles` destructor.
  }

  EXPECT_EQ(TestHandle::NullValue, factory->GetHandleForSessionId(session_id));
}

TEST_F(SessionMappedTabHandleFactoryTest, RemapSessionIdToNewHandle) {
  TestSupportsTabHandles tab1;
  TestSupportsTabHandles tab2;
  const int32_t session_id = 1;
  auto* const factory = &SessionMappedTabHandleFactory::GetInstance();

  // Map session ID to tab1's handle.
  tab1.SetSessionId(session_id);
  TestHandle handle1 = tab1.GetHandle();
  EXPECT_EQ(handle1.raw_value(), factory->GetHandleForSessionId(session_id));

  // Now, map the same session ID to tab2's handle. This should overwrite the
  // previous mapping.
  tab2.SetSessionId(session_id);
  TestHandle handle2 = tab2.GetHandle();
  EXPECT_EQ(handle2.raw_value(), factory->GetHandleForSessionId(session_id));
}

TEST_F(SessionMappedTabHandleFactoryTest, RemapHandleToNewSessionId) {
  TestSupportsTabHandles tab;
  const int32_t session_id1 = 1;
  const int32_t session_id2 = 2;
  auto* const factory = &SessionMappedTabHandleFactory::GetInstance();

  // Map the tab's handle to the first session ID.
  tab.SetSessionId(session_id1);
  TestHandle handle = tab.GetHandle();
  EXPECT_EQ(handle.raw_value(), factory->GetHandleForSessionId(session_id1));

  // Now, map the same handle to the second session ID.
  tab.SetSessionId(session_id2);
  EXPECT_EQ(handle.raw_value(), factory->GetHandleForSessionId(session_id2));

  // The old session ID should no longer map to this handle.
  EXPECT_EQ(TestHandle::NullValue, factory->GetHandleForSessionId(session_id1));
}

TEST_F(SessionMappedTabHandleFactoryTest, GetSessionIdForHandle) {
  TestSupportsTabHandles tab;
  const int32_t session_id = 1;
  tab.SetSessionId(session_id);

  auto* const factory = &SessionMappedTabHandleFactory::GetInstance();
  TestHandle handle = tab.GetHandle();

  EXPECT_NE(handle.raw_value(), TestHandle::NullValue);
  EXPECT_EQ(session_id, factory->GetSessionIdForHandle(handle.raw_value()));
}

TEST_F(SessionMappedTabHandleFactoryTest, ClearSessionId) {
  TestSupportsTabHandles tab;
  const int32_t session_id = 1;
  tab.SetSessionId(session_id);

  auto* const factory = &SessionMappedTabHandleFactory::GetInstance();
  TestHandle handle = tab.GetHandle();

  // Verify the mappings exist before we clear them.
  EXPECT_EQ(handle.raw_value(), factory->GetHandleForSessionId(session_id));
  EXPECT_EQ(session_id, factory->GetSessionIdForHandle(handle.raw_value()));

  // Clear the session ID and verify the mappings are gone.
  tab.ClearSessionId();
  EXPECT_EQ(TestHandle::NullValue, factory->GetHandleForSessionId(session_id));
  EXPECT_FALSE(factory->GetSessionIdForHandle(handle.raw_value()).has_value());
}

}  // namespace
}  // namespace tabs
