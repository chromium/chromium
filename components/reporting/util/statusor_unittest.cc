// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reporting/util/statusor.h"

#include <errno.h>

#include <algorithm>
#include <memory>
#include <tuple>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"

#include "testing/gtest/include/gtest/gtest-death-test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace reporting {
namespace {

class Base1 {
 public:
  virtual ~Base1() = default;
  int pad;
};

class Base2 {
 public:
  virtual ~Base2() = default;
  int yetotherpad;
};

class Derived : public Base1, public Base2 {
 public:
  ~Derived() override = default;
  int evenmorepad;
};

class CopyNoAssign {
 public:
  explicit CopyNoAssign(int value) : foo(value) {}
  CopyNoAssign(const CopyNoAssign& other) : foo(other.foo) {}
  int foo;

 private:
  const CopyNoAssign& operator=(const CopyNoAssign&);
};

TEST(StatusOr, TestDefaultCtor) {
  StatusOr<int> thing;
  EXPECT_FALSE(thing.ok());
  EXPECT_EQ(error::UNKNOWN, thing.status().code());
}

TEST(StatusOr, TestStatusCtor) {
  StatusOr<int> thing(Status(error::CANCELLED, ""));
  EXPECT_FALSE(thing.ok());
  EXPECT_EQ(Status(error::CANCELLED, ""), thing.status());
}

TEST(StatusOr, TestValueCtor) {
  const int kI = 4;
  StatusOr<int> thing(kI);
  EXPECT_TRUE(thing.ok());
  EXPECT_EQ(kI, thing.ValueOrDie());
}

TEST(StatusOr, TestCopyCtorStatusOk) {
  const int kI = 4;
  StatusOr<int> original(kI);
  StatusOr<int> copy(original);
  EXPECT_EQ(original.status(), copy.status());
  EXPECT_EQ(original.ValueOrDie(), copy.ValueOrDie());
}

TEST(StatusOr, TestCopyCtorStatusNotOk) {
  StatusOr<int> original(Status(error::CANCELLED, ""));
  StatusOr<int> copy(original);
  EXPECT_EQ(original.status(), copy.status());
}

TEST(StatusOr, TestCopyCtorStatusOKConverting) {
  const int kI = 4;
  StatusOr<int> original(kI);
  StatusOr<double> copy(original);
  EXPECT_EQ(original.status(), copy.status());
  EXPECT_EQ(original.ValueOrDie(), copy.ValueOrDie());
}

TEST(StatusOr, TestCopyCtorStatusNotOkConverting) {
  StatusOr<int> original(Status(error::CANCELLED, ""));
  StatusOr<double> copy(original);
  EXPECT_EQ(original.status(), copy.status());
}

TEST(StatusOr, TestAssignmentStatusOk) {
  const int kI = 4;
  StatusOr<int> source(kI);
  StatusOr<int> target;
  target = source;
  EXPECT_EQ(source.status(), target.status());
  EXPECT_EQ(source.ValueOrDie(), target.ValueOrDie());
}

TEST(StatusOr, TestAssignmentStatusNotOk) {
  StatusOr<int> source(Status(error::CANCELLED, ""));
  StatusOr<int> target;
  target = source;
  EXPECT_EQ(source.status(), target.status());
}

TEST(StatusOr, TestAssignmentStatusOKConverting) {
  const int kI = 4;
  StatusOr<int> source(kI);
  StatusOr<double> target;
  target = source;
  EXPECT_EQ(source.status(), target.status());
  EXPECT_DOUBLE_EQ(source.ValueOrDie(), target.ValueOrDie());
}

TEST(StatusOr, TestAssignmentStatusNotOkConverting) {
  StatusOr<int> source(Status(error::CANCELLED, ""));
  StatusOr<double> target;
  target = source;
  EXPECT_EQ(source.status(), target.status());
}

TEST(StatusOr, TestStatus) {
  StatusOr<int> good(4);
  EXPECT_TRUE(good.ok());
  StatusOr<int> bad(Status(error::CANCELLED, ""));
  EXPECT_FALSE(bad.ok());
  EXPECT_EQ(Status(error::CANCELLED, ""), bad.status());
}

TEST(StatusOr, TestValueConst) {
  const int kI = 4;
  const StatusOr<int> thing(kI);
  EXPECT_EQ(kI, thing.ValueOrDie());
}

TEST(StatusOr, TestPointerDefaultCtor) {
  StatusOr<int*> thing;
  EXPECT_FALSE(thing.ok());
  EXPECT_EQ(error::UNKNOWN, thing.status().code());
}

TEST(StatusOr, TestPointerStatusCtor) {
  StatusOr<int*> thing(Status(error::CANCELLED, ""));
  EXPECT_FALSE(thing.ok());
  EXPECT_EQ(Status(error::CANCELLED, ""), thing.status());
}

TEST(StatusOr, TestPointerValueCtor) {
  const int kI = 4;
  StatusOr<const int*> thing(&kI);
  EXPECT_TRUE(thing.ok());
  EXPECT_EQ(&kI, thing.ValueOrDie());
}

TEST(StatusOr, TestPointerCopyCtorStatusOk) {
  const int kI = 0;
  StatusOr<const int*> original(&kI);
  StatusOr<const int*> copy(original);
  EXPECT_EQ(original.status(), copy.status());
  EXPECT_EQ(original.ValueOrDie(), copy.ValueOrDie());
}

TEST(StatusOr, TestPointerCopyCtorStatusNotOk) {
  StatusOr<int*> original(Status(error::CANCELLED, ""));
  StatusOr<int*> copy(original);
  EXPECT_EQ(original.status(), copy.status());
}

TEST(StatusOr, TestPointerCopyCtorStatusOKConverting) {
  Derived derived;
  StatusOr<Derived*> original(&derived);
  StatusOr<Base2*> copy(original);
  EXPECT_EQ(original.status(), copy.status());
  EXPECT_EQ(static_cast<const Base2*>(original.ValueOrDie()),
            copy.ValueOrDie());
}

TEST(StatusOr, TestPointerCopyCtorStatusNotOkConverting) {
  StatusOr<Derived*> original(Status(error::CANCELLED, ""));
  StatusOr<Base2*> copy(original);
  EXPECT_EQ(original.status(), copy.status());
}

TEST(StatusOr, TestPointerAssignmentStatusOk) {
  const int kI = 0;
  StatusOr<const int*> source(&kI);
  StatusOr<const int*> target;
  target = source;
  EXPECT_EQ(source.status(), target.status());
  EXPECT_EQ(source.ValueOrDie(), target.ValueOrDie());
}

TEST(StatusOr, TestPointerAssignmentStatusNotOk) {
  StatusOr<int*> source(Status(error::CANCELLED, ""));
  StatusOr<int*> target;
  target = source;
  EXPECT_EQ(source.status(), target.status());
}

TEST(StatusOr, TestPointerAssignmentStatusOKConverting) {
  Derived derived;
  StatusOr<Derived*> source(&derived);
  StatusOr<Base2*> target;
  target = source;
  EXPECT_EQ(source.status(), target.status());
  EXPECT_EQ(static_cast<const Base2*>(source.ValueOrDie()),
            target.ValueOrDie());
}

TEST(StatusOr, TestPointerAssignmentStatusNotOkConverting) {
  StatusOr<Derived*> source(Status(error::CANCELLED, ""));
  StatusOr<Base2*> target;
  target = source;
  EXPECT_EQ(source.status(), target.status());
}

TEST(StatusOr, TestPointerStatus) {
  const int kI = 0;
  StatusOr<const int*> good(&kI);
  EXPECT_TRUE(good.ok());
  StatusOr<const int*> bad(Status(error::CANCELLED, ""));
  EXPECT_EQ(Status(error::CANCELLED, ""), bad.status());
}

TEST(StatusOr, TestPointerValue) {
  const int kI = 0;
  StatusOr<const int*> thing(&kI);
  EXPECT_EQ(&kI, thing.ValueOrDie());
}

TEST(StatusOr, TestPointerValueConst) {
  const int kI = 0;
  const StatusOr<const int*> thing(&kI);
  EXPECT_EQ(&kI, thing.ValueOrDie());
}

TEST(StatusOr, TestMoveStatusOr) {
  const int kI = 0;
  StatusOr<std::unique_ptr<int>> thing(std::make_unique<int>(kI));
  EXPECT_OK(thing.status());
  StatusOr<std::unique_ptr<int>> moved = std::move(thing);
  EXPECT_EQ(error::UNKNOWN, thing.status().code());
  EXPECT_TRUE(moved.ok());
  EXPECT_EQ(kI, *moved.ValueOrDie());
}

TEST(StatusOr, TestBinding) {
  class RefCountedValue : public base::RefCounted<RefCountedValue> {
   public:
    explicit RefCountedValue(StatusOr<int> value) : value_(value) {}
    Status status() const { return value_.status(); }
    int value() const { return value_.ValueOrDie(); }

   private:
    friend class base::RefCounted<RefCountedValue>;
    ~RefCountedValue() = default;
    const StatusOr<int> value_;
  };
  const int kI = 0;
  base::OnceCallback<int(StatusOr<scoped_refptr<RefCountedValue>>)> callback =
      base::BindOnce([](StatusOr<scoped_refptr<RefCountedValue>> val) {
        return val.ValueOrDie()->value();
      });
  const int result =
      std::move(callback).Run(base::MakeRefCounted<RefCountedValue>(kI));
  EXPECT_EQ(kI, result);
}

TEST(StatusOr, TestAbort) {
  StatusOr<int> thing1(Status(error::UNKNOWN, "Unknown"));
  EXPECT_DEATH_IF_SUPPORTED(std::ignore = thing1.ValueOrDie(), "");

  StatusOr<std::unique_ptr<int>> thing2(Status(error::UNKNOWN, "Unknown"));
  EXPECT_DEATH_IF_SUPPORTED(std::ignore = std::move(thing2.ValueOrDie()), "");
}
}  // namespace
}  // namespace reporting
