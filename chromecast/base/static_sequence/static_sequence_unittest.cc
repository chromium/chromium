// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/base/static_sequence/static_sequence.h"

#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace util {
namespace {

struct TestSequence : StaticSequence<TestSequence> {};

struct CustomTraitsProvider {
  static constexpr base::TaskTraits GetTraits() {
    return {base::ThreadPool(), base::TaskPriority::LOWEST,
            base::ThreadPolicy::PREFER_BACKGROUND, base::MayBlock()};
  }
};

struct TestSequenceWithCustomTraits
    : StaticSequence<TestSequenceWithCustomTraits, CustomTraitsProvider> {};

void DoSomething(bool* activated) {
  *activated = true;
}

void DoSomethingWithRequiredSequence(bool* activated,
                                     const TestSequence::Key&) {
  *activated = true;
}

class TestObject {
 public:
  void DoSomething(bool* activated) { *activated = true; }
  void DoSomethingWithRequiredSequence(bool* activated,
                                       const TestSequence::Key&) {
    *activated = true;
  }
};

class ParameterizedObject {
 public:
  explicit ParameterizedObject(int increment_by)
      : increment_by_(increment_by) {}

  void Increment(int* out, const TestSequence::Key&) { *out += increment_by_; }

 private:
  int increment_by_;
};

class HasSideEffectsInConstructor {
 public:
  HasSideEffectsInConstructor(int x, int y, int* r) { *r = x + y; }
};

class HasSideEffectsInDestructor {
 public:
  HasSideEffectsInDestructor(int x, int y, int* r) : r_(r), sum_(x + y) {}
  ~HasSideEffectsInDestructor() { *r_ = sum_; }

 private:
  int* r_;
  int sum_;
};

}  // namespace

TEST(StaticSequenceTest, StaticProperties) {
  static_assert(!std::is_copy_constructible<TestSequence::Key>::value,
                "Keys must not be copyable.");
  static_assert(!std::is_move_constructible<TestSequence::Key>::value,
                "Keys must not be movable.");
}

TEST(StaticSequenceTest, InvokeUnprotectedCallback) {
  base::test::TaskEnvironment env;
  bool activated = false;
  TestSequence::PostTask(base::BindOnce(&DoSomething, &activated));
  EXPECT_FALSE(activated);
  env.RunUntilIdle();
  EXPECT_TRUE(activated);
}

TEST(StaticSequenceTest, InvokeProtectedCallback) {
  base::test::TaskEnvironment env;
  bool activated = false;
  TestSequence::PostTask(
      base::BindOnce(&DoSomethingWithRequiredSequence, &activated));
  EXPECT_FALSE(activated);
  env.RunUntilIdle();
  EXPECT_TRUE(activated);
}

TEST(StaticSequenceTest, InvokeObjectUnprotectedMethod) {
  base::test::TaskEnvironment env;
  bool activated = false;
  TestObject obj;
  TestSequence::PostTask(base::BindOnce(&TestObject::DoSomething,
                                        base::Unretained(&obj), &activated));
  EXPECT_FALSE(activated);
  env.RunUntilIdle();
  EXPECT_TRUE(activated);
}

TEST(StaticSequenceTest, InvokeSequencedObjectUnprotectedMethod) {
  base::test::TaskEnvironment env;
  bool activated = false;
  Sequenced<TestObject, TestSequence> obj;
  obj.Post(FROM_HERE, &TestObject::DoSomething, &activated);
  EXPECT_FALSE(activated);
  env.RunUntilIdle();
  EXPECT_TRUE(activated);
}

TEST(StaticSequenceTest, InvokeSequencedObjectProtectedMethod) {
  base::test::TaskEnvironment env;
  bool activated = false;
  Sequenced<TestObject, TestSequence> obj;
  obj.Post(FROM_HERE, &TestObject::DoSomethingWithRequiredSequence, &activated);
  EXPECT_FALSE(activated);
  env.RunUntilIdle();
  EXPECT_TRUE(activated);
}

TEST(StaticSequenceTest, SequencedConstructorIncludesArguments) {
  base::test::TaskEnvironment env;
  int r = 0;
  Sequenced<ParameterizedObject, TestSequence> obj(2);
  obj.Post(FROM_HERE, &ParameterizedObject::Increment, &r);
  EXPECT_EQ(r, 0);
  env.RunUntilIdle();
  EXPECT_EQ(r, 2);
}

TEST(StaticSequenceTest, UseCustomTraits) {
  base::test::TaskEnvironment env;
  bool r = false;
  Sequenced<TestObject, TestSequenceWithCustomTraits> obj;
  obj.Post(FROM_HERE, &TestObject::DoSomething, &r);
  EXPECT_FALSE(r);
  env.RunUntilIdle();
  EXPECT_TRUE(r);
}

TEST(StaticSequenceTest, ConstructsOnSequence) {
  base::test::TaskEnvironment env;
  int r = 0;
  // The constructor for HasSideEffectsInConstructor will set |r| to the sum of
  // the first two arguments, but should only run on the sequence.
  Sequenced<HasSideEffectsInConstructor, TestSequence> obj(1, 2, &r);
  EXPECT_EQ(r, 0);
  env.RunUntilIdle();
  EXPECT_EQ(r, 3);
}

TEST(StaticSequenceTest, DestructOnSequence) {
  base::test::TaskEnvironment env;
  int r = 0;
  {
    // The destructor for HasSideEffectsInDestructor will set |r| to the sum of
    // the first two constructor arguments, but should only run on the sequence.
    Sequenced<HasSideEffectsInDestructor, TestSequence> obj(2, 3, &r);
    env.RunUntilIdle();
    EXPECT_EQ(r, 0);
  }
  EXPECT_EQ(r, 0);
  env.RunUntilIdle();
  EXPECT_EQ(r, 5);
}

TEST(StaticSequenceTest, PostUnprotectedMemberFunction) {
  base::test::TaskEnvironment env;
  TestObject x;
  bool r = false;
  TestSequence::Post(FROM_HERE, &TestObject::DoSomething, base::Unretained(&x),
                     &r);
  EXPECT_FALSE(r);
  env.RunUntilIdle();
  EXPECT_TRUE(r);
}

TEST(StaticSequenceTest, PostProtectedMemberFunction) {
  base::test::TaskEnvironment env;
  TestObject x;
  bool r = false;
  TestSequence::Post(FROM_HERE, &TestObject::DoSomethingWithRequiredSequence,
                     base::Unretained(&x), &r);
  EXPECT_FALSE(r);
  env.RunUntilIdle();
  EXPECT_TRUE(r);
}

TEST(StaticSequenceTest, PostUnprotectedFreeFunction) {
  base::test::TaskEnvironment env;
  bool r = false;
  TestSequence::Post(FROM_HERE, &DoSomething, &r);
  EXPECT_FALSE(r);
  env.RunUntilIdle();
  EXPECT_TRUE(r);
}

TEST(StaticSequenceTest, PostProtectedFreeFunction) {
  base::test::TaskEnvironment env;
  bool r = false;
  TestSequence::Post(FROM_HERE, &DoSomethingWithRequiredSequence, &r);
  EXPECT_FALSE(r);
  env.RunUntilIdle();
  EXPECT_TRUE(r);
}

}  // namespace util
