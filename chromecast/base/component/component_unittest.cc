// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/base/component/component.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromecast {

class ComponentTest : public ::testing::Test {
 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
};

using ComponentDeathTest = ComponentTest;

class ComponentB;
class ComponentC;
class ComponentA : public Component<ComponentA> {
 public:
  void MakeSelfDependency() {
    a_.reset(new Component<ComponentA>::Dependency(GetRef(), this));
  }

  void MakeCircularDependency(const Component<ComponentB>::WeakRef& b) {
    b_.reset(new Component<ComponentB>::Dependency(b, this));
  }

  void MakeTransitiveCircularDependency(
      const Component<ComponentC>::WeakRef& c) {
    c_.reset(new Component<ComponentC>::Dependency(c, this));
  }

  void OnEnable() override {
    if (!fail_enable_) {
      enabled_ = true;
      Test();
    }
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&ComponentA::OnEnableComplete,
                                  base::Unretained(this), !fail_enable_));
  }

  void OnDisable() override {
    if (enabled_)
      Test();
    enabled_ = false;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&ComponentA::OnDisableComplete, base::Unretained(this)));
  }

  void Test() {
    EXPECT_TRUE(enabled_);
    EXPECT_FALSE(fail_enable_);
  }

  bool enabled() const { return enabled_; }
  void FailEnable() { fail_enable_ = true; }

 private:
  bool enabled_ = false;
  bool fail_enable_ = false;

  std::unique_ptr<Component<ComponentA>::Dependency> a_;
  std::unique_ptr<Component<ComponentB>::Dependency> b_;
  std::unique_ptr<Component<ComponentC>::Dependency> c_;
};

class ComponentB : public Component<ComponentB> {
 public:
  explicit ComponentB(const ComponentA::WeakRef& a) : a_(a, this) {}

  void OnEnable() override {
    if (!fail_enable_) {
      enabled_ = true;
      Test();
    }
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&ComponentB::OnEnableComplete,
                                  base::Unretained(this), !fail_enable_));
  }

  void OnDisable() override {
    if (enabled_)
      Test();
    enabled_ = false;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&ComponentB::OnDisableComplete, base::Unretained(this)));
  }

  void Test() {
    EXPECT_TRUE(enabled_);
    EXPECT_FALSE(fail_enable_);
    a_->Test();
  }

  bool enabled() const { return enabled_; }
  void FailEnable() { fail_enable_ = true; }

 private:
  bool enabled_ = false;
  bool fail_enable_ = false;

  ComponentA::Dependency a_;
};

class ComponentC : public Component<ComponentC> {
 public:
  explicit ComponentC(const ComponentB::WeakRef& b) : b_(b, this) {}

  void OnEnable() override {
    if (!fail_enable_) {
      enabled_ = true;
      Test();
    }
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&ComponentC::OnEnableComplete,
                                  base::Unretained(this), !fail_enable_));
  }

  void OnDisable() override {
    if (enabled_)
      Test();
    enabled_ = false;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&ComponentC::OnDisableComplete, base::Unretained(this)));
  }

  void Test() {
    EXPECT_TRUE(enabled_);
    EXPECT_FALSE(fail_enable_);
    b_->Test();
  }

  bool enabled() const { return enabled_; }
  void FailEnable() { fail_enable_ = true; }

 private:
  bool enabled_ = false;
  bool fail_enable_ = false;

  ComponentB::Dependency b_;
};

std::string DeathRegex(const std::string& regex) {
#if BUILDFLAG(IS_ANDROID)
  return "";
#else
  return regex;
#endif
}

#if (!defined(NDEBUG) || defined(DCHECK_ALWAYS_ON)) && GTEST_HAS_DEATH_TEST
TEST_F(ComponentDeathTest, SelfDependency) {
  GTEST_FLAG_SET(death_test_style, "threadsafe");
  ComponentA a;
  EXPECT_DEATH(a.MakeSelfDependency(), DeathRegex("Circular dependency"));
}

TEST_F(ComponentDeathTest, CircularDependency) {
  GTEST_FLAG_SET(death_test_style, "threadsafe");
  ComponentA a;
  ComponentB b(a.GetRef());
  EXPECT_DEATH(a.MakeCircularDependency(b.GetRef()),
               DeathRegex("Circular dependency"));
}

TEST_F(ComponentDeathTest, TransitiveCircularDependency) {
  GTEST_FLAG_SET(death_test_style, "threadsafe");
  ComponentA a;
  ComponentB b(a.GetRef());
  ComponentC c(b.GetRef());
  EXPECT_DEATH(a.MakeTransitiveCircularDependency(c.GetRef()),
               DeathRegex("Circular dependency"));
}
#endif  // (!defined(NDEBUG) || defined(DCHECK_ALWAYS_ON)) &&
        //     GTEST_HAS_DEATH_TEST

TEST_F(ComponentTest, SimpleEnable) {
  std::unique_ptr<ComponentA> a(new ComponentA());
  a->Enable();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(a->enabled());
  a.release()->Destroy();
}

TEST_F(ComponentTest, TransitiveEnable) {
  std::unique_ptr<ComponentA> a(new ComponentA());
  std::unique_ptr<ComponentB> b(new ComponentB(a->GetRef()));
  std::unique_ptr<ComponentC> c(new ComponentC(b->GetRef()));
  c->Enable();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(a->enabled());
  EXPECT_TRUE(b->enabled());
  EXPECT_TRUE(c->enabled());
  a.release()->Destroy();
  b.release()->Destroy();
  c.release()->Destroy();
}

TEST_F(ComponentTest, FailEnable) {
  std::unique_ptr<ComponentA> a(new ComponentA());
  a->FailEnable();
  a->Enable();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(a->enabled());
  a.release()->Destroy();
}

TEST_F(ComponentTest, TransitiveFailEnable) {
  std::unique_ptr<ComponentA> a(new ComponentA());
  std::unique_ptr<ComponentB> b(new ComponentB(a->GetRef()));
  std::unique_ptr<ComponentC> c(new ComponentC(b->GetRef()));
  a->FailEnable();
  c->Enable();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(a->enabled());
  EXPECT_FALSE(b->enabled());
  EXPECT_FALSE(c->enabled());
  a.release()->Destroy();
  b.release()->Destroy();
  c.release()->Destroy();
}

TEST_F(ComponentTest, DisableWhileEnabling) {
  std::unique_ptr<ComponentA> a(new ComponentA());
  a->Enable();
  a->Disable();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(a->enabled());
  a.release()->Destroy();
}

TEST_F(ComponentTest, EnableTwice) {
  std::unique_ptr<ComponentA> a(new ComponentA());
  a->Enable();
  a->Enable();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(a->enabled());
  a.release()->Destroy();
}

TEST_F(ComponentTest, DisableTwice) {
  std::unique_ptr<ComponentA> a(new ComponentA());
  a->Enable();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(a->enabled());
  a->Disable();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(a->enabled());
  a->Disable();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(a->enabled());
  a.release()->Destroy();
}

TEST_F(ComponentTest, DisableAfterFailedEnable) {
  std::unique_ptr<ComponentA> a(new ComponentA());
  a->FailEnable();
  a->Enable();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(a->enabled());
  a->Disable();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(a->enabled());
  a.release()->Destroy();
}

TEST_F(ComponentTest, DisableAfterNeverEnabled) {
  std::unique_ptr<ComponentA> a(new ComponentA());
  a->Disable();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(a->enabled());
  a.release()->Destroy();
}

TEST_F(ComponentTest, DisableDependencyWhileEnabling) {
  std::unique_ptr<ComponentA> a(new ComponentA());
  std::unique_ptr<ComponentB> b(new ComponentB(a->GetRef()));
  std::unique_ptr<ComponentC> c(new ComponentC(b->GetRef()));
  b->Enable();
  base::RunLoop().RunUntilIdle();
  c->Enable();
  a->Disable();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(a->enabled());
  EXPECT_FALSE(b->enabled());
  EXPECT_FALSE(c->enabled());
  a.release()->Destroy();
  b.release()->Destroy();
  c.release()->Destroy();
}

TEST_F(ComponentTest, EnableDisableEnable) {
  std::unique_ptr<ComponentA> a(new ComponentA());
  a->Enable();
  a->Disable();
  a->Enable();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(a->enabled());
  a.release()->Destroy();
}

TEST_F(ComponentTest, DisableEnableDisable) {
  std::unique_ptr<ComponentA> a(new ComponentA());
  a->Enable();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(a->enabled());
  a->Disable();
  a->Enable();
  a->Disable();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(a->enabled());
  a.release()->Destroy();
}

TEST_F(ComponentTest, TransitiveEnableDisableEnable) {
  std::unique_ptr<ComponentA> a(new ComponentA());
  std::unique_ptr<ComponentB> b(new ComponentB(a->GetRef()));
  std::unique_ptr<ComponentC> c(new ComponentC(b->GetRef()));
  a->Enable();
  base::RunLoop().RunUntilIdle();
  c->Enable();
  a->Disable();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(a->enabled());
  EXPECT_FALSE(b->enabled());
  EXPECT_FALSE(c->enabled());
  c->Enable();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(a->enabled());
  EXPECT_TRUE(b->enabled());
  EXPECT_TRUE(c->enabled());
  a.release()->Destroy();
  b.release()->Destroy();
  c.release()->Destroy();
}

TEST_F(ComponentTest, WeakRefs) {
  std::unique_ptr<ComponentA> a(new ComponentA());
  ComponentA::WeakRef weak = a->GetRef();
  EXPECT_FALSE(weak.Try());
  a->Enable();
  EXPECT_FALSE(weak.Try());
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(weak.Try());
  weak.Try()->Test();
  a->Disable();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(weak.Try());
  a.release()->Destroy();
}

TEST_F(ComponentTest, WeakRefsKeepEnabled) {
  std::unique_ptr<ComponentA> a(new ComponentA());
  ComponentA::WeakRef weak = a->GetRef();
  EXPECT_FALSE(weak.Try());
  a->Enable();
  EXPECT_FALSE(weak.Try());
  base::RunLoop().RunUntilIdle();
  {
    auto held_ref = weak.Try();
    EXPECT_TRUE(held_ref);
    held_ref->Test();
    a->Disable();
    base::RunLoop().RunUntilIdle();
    // The held ref keeps |a| enabled until it goes out of scope.
    EXPECT_TRUE(a->enabled());
  }
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(a->enabled());
  EXPECT_FALSE(weak.Try());
  a.release()->Destroy();
}

}  // namespace chromecast
