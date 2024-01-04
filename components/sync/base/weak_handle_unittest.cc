// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/base/weak_handle.h"

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/threading/thread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

using ::testing::_;
using ::testing::SaveArg;
using ::testing::StrictMock;

class Base {
 public:
  Base() = default;

  WeakHandle<Base> AsWeakHandle() {
    return MakeWeakHandle(weak_ptr_factory_.GetWeakPtr());
  }

  void Kill() { weak_ptr_factory_.InvalidateWeakPtrs(); }
  MOCK_METHOD(void, Test, (), ());
  MOCK_METHOD(void, Test1, (const int&), ());
  MOCK_METHOD(void, Test2, (const int&, Base*), ());
  MOCK_METHOD(void, Test3, (const int&, Base*, float), ());
  MOCK_METHOD(void, Test4, (const int&, Base*, float, const char*), ());
  MOCK_METHOD(void, TestWithSelf, (const WeakHandle<Base>&), ());

 private:
  base::WeakPtrFactory<Base> weak_ptr_factory_{this};
};

class Derived : public Base {
 public:
  base::WeakPtr<Derived> AsWeakPtr() { return weak_ptr_factory_.GetWeakPtr(); }

 private:
  base::WeakPtrFactory<Derived> weak_ptr_factory_{this};
};

class WeakHandleTest : public ::testing::Test {
 protected:
  void TearDown() override {
    // Process any last-minute posted tasks.
    PumpLoop();
  }

  void PumpLoop() { base::RunLoop().RunUntilIdle(); }

  static void CallTestFromOtherThread(base::Location from_here,
                                      const WeakHandle<Base>& h) {
    base::Thread t("Test thread");
    ASSERT_TRUE(t.Start());
    t.task_runner()->PostTask(
        from_here, base::BindOnce(&WeakHandleTest::CallTest, from_here, h));
  }

 private:
  static void CallTest(base::Location from_here, const WeakHandle<Base>& h) {
    h.Call(from_here, &Base::Test);
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
};

TEST_F(WeakHandleTest, Uninitialized) {
  // Default.
  WeakHandle<int> h;
  EXPECT_FALSE(h.IsInitialized());
  // Copy.
  {
    WeakHandle<int> h2(h);
    EXPECT_FALSE(h2.IsInitialized());
  }
  // Assign.
  {
    WeakHandle<int> h2;
    h2 = h;
    EXPECT_FALSE(h.IsInitialized());
  }
}

TEST_F(WeakHandleTest, InitializedAfterDestroy) {
  WeakHandle<Base> h;
  {
    StrictMock<Base> b;
    h = b.AsWeakHandle();
  }
  EXPECT_TRUE(h.IsInitialized());
  EXPECT_FALSE(h.Get());
}

TEST_F(WeakHandleTest, InitializedAfterInvalidate) {
  StrictMock<Base> b;
  WeakHandle<Base> h = b.AsWeakHandle();
  b.Kill();
  EXPECT_TRUE(h.IsInitialized());
  EXPECT_FALSE(h.Get());
}

TEST_F(WeakHandleTest, Call) {
  StrictMock<Base> b;
  const char test_str[] = "test";
  EXPECT_CALL(b, Test());
  EXPECT_CALL(b, Test1(5));
  EXPECT_CALL(b, Test2(5, &b));
  EXPECT_CALL(b, Test3(5, &b, 5));
  EXPECT_CALL(b, Test4(5, &b, 5, test_str));

  WeakHandle<Base> h = b.AsWeakHandle();
  EXPECT_TRUE(h.IsInitialized());

  // Should run.
  h.Call(FROM_HERE, &Base::Test);
  h.Call(FROM_HERE, &Base::Test1, 5);
  h.Call(FROM_HERE, &Base::Test2, 5, &b);
  h.Call(FROM_HERE, &Base::Test3, 5, &b, 5);
  h.Call(FROM_HERE, &Base::Test4, 5, &b, 5, test_str);
  PumpLoop();
}

TEST_F(WeakHandleTest, CallAfterDestroy) {
  {
    StrictMock<Base> b;
    EXPECT_CALL(b, Test()).Times(0);

    WeakHandle<Base> h = b.AsWeakHandle();
    EXPECT_TRUE(h.IsInitialized());

    // Should not run.
    h.Call(FROM_HERE, &Base::Test);
  }
  PumpLoop();
}

TEST_F(WeakHandleTest, CallAfterInvalidate) {
  StrictMock<Base> b;
  EXPECT_CALL(b, Test()).Times(0);

  WeakHandle<Base> h = b.AsWeakHandle();
  EXPECT_TRUE(h.IsInitialized());

  // Should not run.
  h.Call(FROM_HERE, &Base::Test);

  b.Kill();
  PumpLoop();
}

TEST_F(WeakHandleTest, CallThreaded) {
  StrictMock<Base> b;
  EXPECT_CALL(b, Test());

  WeakHandle<Base> h = b.AsWeakHandle();
  // Should run.
  CallTestFromOtherThread(FROM_HERE, h);
  PumpLoop();
}

TEST_F(WeakHandleTest, CallAfterDestroyThreaded) {
  WeakHandle<Base> h;
  {
    StrictMock<Base> b;
    EXPECT_CALL(b, Test()).Times(0);
    h = b.AsWeakHandle();
  }

  // Should not run.
  CallTestFromOtherThread(FROM_HERE, h);
  PumpLoop();
}

TEST_F(WeakHandleTest, CallAfterInvalidateThreaded) {
  StrictMock<Base> b;
  EXPECT_CALL(b, Test()).Times(0);

  WeakHandle<Base> h = b.AsWeakHandle();
  b.Kill();
  // Should not run.
  CallTestFromOtherThread(FROM_HERE, h);
  PumpLoop();
}

TEST_F(WeakHandleTest, DeleteOnOtherThread) {
  StrictMock<Base> b;
  EXPECT_CALL(b, Test()).Times(0);

  WeakHandle<Base>* h = new WeakHandle<Base>(b.AsWeakHandle());

  {
    base::Thread t("Test thread");
    ASSERT_TRUE(t.Start());
    t.task_runner()->DeleteSoon(FROM_HERE, h);
  }

  PumpLoop();
}

void CallTestWithSelf(const WeakHandle<Base>& b1) {
  StrictMock<Base> b2;
  b1.Call(FROM_HERE, &Base::TestWithSelf, b2.AsWeakHandle());
}

TEST_F(WeakHandleTest, WithDestroyedThread) {
  StrictMock<Base> b1;
  WeakHandle<Base> b2;
  EXPECT_CALL(b1, TestWithSelf).WillOnce(SaveArg<0>(&b2));

  {
    base::Thread t("Test thread");
    ASSERT_TRUE(t.Start());
    t.task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&CallTestWithSelf, b1.AsWeakHandle()));
  }

  // Calls b1.TestWithSelf().
  PumpLoop();

  // Shouldn't do anything, since the thread is gone.
  b2.Call(FROM_HERE, &Base::Test);

  // |b2| shouldn't leak when it's destroyed, even if the original
  // thread is gone.
}

TEST_F(WeakHandleTest, InitializedAcrossCopyAssign) {
  StrictMock<Base> b;
  EXPECT_CALL(b, Test()).Times(3);

  EXPECT_TRUE(b.AsWeakHandle().IsInitialized());
  b.AsWeakHandle().Call(FROM_HERE, &Base::Test);

  {
    WeakHandle<Base> h(b.AsWeakHandle());
    EXPECT_TRUE(h.IsInitialized());
    h.Call(FROM_HERE, &Base::Test);
    h.Reset();
    EXPECT_FALSE(h.IsInitialized());
  }

  {
    WeakHandle<Base> h;
    h = b.AsWeakHandle();
    EXPECT_TRUE(h.IsInitialized());
    h.Call(FROM_HERE, &Base::Test);
    h.Reset();
    EXPECT_FALSE(h.IsInitialized());
  }

  PumpLoop();
}

TEST_F(WeakHandleTest, TypeConversionConstructor) {
  StrictMock<Derived> d;
  EXPECT_CALL(d, Test()).Times(2);

  const WeakHandle<Derived> weak_handle = MakeWeakHandle(d.AsWeakPtr());

  // Should trigger type conversion constructor.
  const WeakHandle<Base> base_weak_handle(weak_handle);
  // Should trigger regular copy constructor.
  const WeakHandle<Derived> derived_weak_handle(weak_handle);

  EXPECT_TRUE(base_weak_handle.IsInitialized());
  base_weak_handle.Call(FROM_HERE, &Base::Test);

  EXPECT_TRUE(derived_weak_handle.IsInitialized());
  // Copy constructor shouldn't construct a new |core_|.
  EXPECT_EQ(weak_handle.core_.get(), derived_weak_handle.core_.get());
  derived_weak_handle.Call(FROM_HERE, &Base::Test);

  PumpLoop();
}

TEST_F(WeakHandleTest, TypeConversionConstructorMakeWeakHandle) {
  const base::WeakPtr<Derived> weak_ptr;

  // Should trigger type conversion constructor after MakeWeakHandle.
  WeakHandle<Base> base_weak_handle(MakeWeakHandle(weak_ptr));
  // Should trigger regular copy constructor after MakeWeakHandle.
  const WeakHandle<Derived> derived_weak_handle(MakeWeakHandle(weak_ptr));

  EXPECT_TRUE(base_weak_handle.IsInitialized());
  EXPECT_TRUE(derived_weak_handle.IsInitialized());
}

TEST_F(WeakHandleTest, TypeConversionConstructorAssignment) {
  const WeakHandle<Derived> weak_handle = MakeWeakHandle(Derived().AsWeakPtr());

  // Should trigger type conversion constructor before the assignment.
  WeakHandle<Base> base_weak_handle;
  base_weak_handle = weak_handle;
  // Should trigger regular copy constructor before the assignment.
  WeakHandle<Derived> derived_weak_handle;
  derived_weak_handle = weak_handle;

  EXPECT_TRUE(base_weak_handle.IsInitialized());
  EXPECT_TRUE(derived_weak_handle.IsInitialized());
  // Copy constructor shouldn't construct a new |core_|.
  EXPECT_EQ(weak_handle.core_.get(), derived_weak_handle.core_.get());
}

TEST_F(WeakHandleTest, TypeConversionConstructorUninitialized) {
  const WeakHandle<Base> base_weak_handle = WeakHandle<Derived>();
  EXPECT_FALSE(base_weak_handle.IsInitialized());
}

TEST_F(WeakHandleTest, TypeConversionConstructorUninitializedAssignment) {
  WeakHandle<Base> base_weak_handle;
  base_weak_handle = WeakHandle<Derived>();
  EXPECT_FALSE(base_weak_handle.IsInitialized());
}

}  // namespace syncer
