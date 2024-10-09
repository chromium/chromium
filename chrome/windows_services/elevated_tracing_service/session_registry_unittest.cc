// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/windows_services/elevated_tracing_service/session_registry.h"

#include <windows.h>

#include <wrl/client.h>
#include <wrl/implements.h>

#include "base/task/sequenced_task_runner.h"
#include "base/test/run_until.h"
#include "base/win/scoped_com_initializer.h"
#include "chrome/windows_services/elevated_tracing_service/with_child_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"

namespace elevated_tracing_service {

namespace {

class DummyUnknown
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          IUnknown> {
 public:
  DummyUnknown() = default;
  DummyUnknown(const DummyUnknown&) = delete;
  DummyUnknown& operator=(const DummyUnknown&) = delete;

  using Microsoft::WRL::RuntimeClass<
      Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
      IUnknown>::CastToUnknown;
};

}  // namespace

class SessionRegistryTest : public WithChildTest {
 protected:
  void SetUp() override { ASSERT_TRUE(scoped_com_initializer_.Succeeded()); }

  SessionRegistry& session_registry() { return *session_registry_; }

 private:
  base::win::ScopedCOMInitializer scoped_com_initializer_;
  scoped_refptr<SessionRegistry> session_registry_{
      base::MakeRefCounted<SessionRegistry>()};
};

// Tests that only a single session may be active at a time.
TEST_F(SessionRegistryTest, OnlyOneActiveSession) {
  auto child_process = SpawnChildWithEventHandles(kExitWhenSignaled);
  ASSERT_TRUE(child_process.IsValid());
  absl::Cleanup child_stopper = [this] { SignalChildTermination(); };

  WaitForChildStart();

  auto dummy = Microsoft::WRL::Make<DummyUnknown>();
  ASSERT_TRUE(bool(dummy));

  auto session = SessionRegistry::RegisterActiveSession(
      dummy->CastToUnknown(), child_process.Duplicate());
  ASSERT_TRUE(session);

  // A second registration should fail.
  ASSERT_FALSE(SessionRegistry::RegisterActiveSession(
      dummy->CastToUnknown(), child_process.Duplicate()));

  // Release the first session.
  session.reset();

  // And now a new one can be registered.
  ASSERT_TRUE(SessionRegistry::RegisterActiveSession(dummy->CastToUnknown(),
                                                     std::move(child_process)));
}

// Tests that termination of the client is handled.
TEST_F(SessionRegistryTest, ClientTerminates) {
  auto child_process = SpawnChildWithEventHandles(kExitWhenSignaled);
  ASSERT_TRUE(child_process.IsValid());
  absl::Cleanup child_stopper = [this] { SignalChildTermination(); };

  WaitForChildStart();

  auto dummy = Microsoft::WRL::Make<DummyUnknown>();
  ASSERT_TRUE(bool(dummy));

  auto session = SessionRegistry::RegisterActiveSession(
      dummy->CastToUnknown(), child_process.Duplicate());
  ASSERT_TRUE(session);

  // Tell the child to terminate.
  std::move(child_stopper).Invoke();

  // Wait for the child to terminate.
  ASSERT_EQ(::WaitForSingleObject(child_process.Handle(), INFINITE),
            WAIT_OBJECT_0);

  // Run tasks until registry notices that the child has terminated.
  ASSERT_TRUE(base::test::RunUntil(
      [this] { return !session_registry().HasActiveSessionForTesting(); }));

  // It should now be possible to register a new session.
  child_process = SpawnChildWithEventHandles(kExitWhenSignaled);
  ASSERT_TRUE(child_process.IsValid());
  absl::Cleanup child_stopper2 = [this] { SignalChildTermination(); };
  ASSERT_TRUE(SessionRegistry::RegisterActiveSession(
      dummy->CastToUnknown(), child_process.Duplicate()));
}

}  // namespace elevated_tracing_service
