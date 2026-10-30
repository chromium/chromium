// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/keyed_service/content/browser_context_dependency_manager.h"

#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/keyed_service/content/refcounted_browser_context_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/keyed_service/core/refcounted_keyed_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "testing/gtest/include/gtest/gtest.h"

class BrowserContextDependencyManagerUnittests : public ::testing::Test {
 protected:
  // To get around class access:
  void DependOn(BrowserContextKeyedServiceFactory* child,
                BrowserContextKeyedServiceFactory* parent) {
    child->DependsOn(parent);
  }

  BrowserContextDependencyManager* manager() { return &dependency_manager_; }

  std::vector<std::string>* shutdown_order() { return &shutdown_order_; }

 private:
  BrowserContextDependencyManager dependency_manager_;

  std::vector<std::string> shutdown_order_;
};

class TestService : public BrowserContextKeyedServiceFactory {
 public:
  TestService(const std::string& name,
              std::vector<std::string>* fill_on_shutdown,
              BrowserContextDependencyManager* manager)
      : BrowserContextKeyedServiceFactory("TestService", manager),
        name_(name),
        fill_on_shutdown_(fill_on_shutdown) {}

  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override {
    ADD_FAILURE() << "This isn't part of the tests!";
    return nullptr;
  }

  void BrowserContextShutdown(content::BrowserContext* context) override {
    fill_on_shutdown_->push_back(name_);
  }

 private:
  const std::string name_;
  raw_ptr<std::vector<std::string>> fill_on_shutdown_;
};

// Tests that we can deal with a single component.
TEST_F(BrowserContextDependencyManagerUnittests, SingleCase) {
  TestService service("service", shutdown_order(), manager());

  manager()->DestroyBrowserContextServices(nullptr);

  ASSERT_EQ(1U, shutdown_order()->size());
  EXPECT_STREQ("service", (*shutdown_order())[0].c_str());
}

// Tests that we get a simple one component depends on the other case.
TEST_F(BrowserContextDependencyManagerUnittests, SimpleDependency) {
  TestService parent("parent", shutdown_order(), manager());
  TestService child("child", shutdown_order(), manager());
  DependOn(&child, &parent);

  manager()->DestroyBrowserContextServices(nullptr);

  ASSERT_EQ(2U, shutdown_order()->size());
  EXPECT_STREQ("child", (*shutdown_order())[0].c_str());
  EXPECT_STREQ("parent", (*shutdown_order())[1].c_str());
}

// Tests two children, one parent
TEST_F(BrowserContextDependencyManagerUnittests, TwoChildrenOneParent) {
  TestService parent("parent", shutdown_order(), manager());
  TestService child1("child1", shutdown_order(), manager());
  TestService child2("child2", shutdown_order(), manager());
  DependOn(&child1, &parent);
  DependOn(&child2, &parent);

  manager()->DestroyBrowserContextServices(nullptr);

  ASSERT_EQ(3U, shutdown_order()->size());
  EXPECT_STREQ("child2", (*shutdown_order())[0].c_str());
  EXPECT_STREQ("child1", (*shutdown_order())[1].c_str());
  EXPECT_STREQ("parent", (*shutdown_order())[2].c_str());
}

// Tests an M configuration
TEST_F(BrowserContextDependencyManagerUnittests, MConfiguration) {
  TestService parent1("parent1", shutdown_order(), manager());
  TestService parent2("parent2", shutdown_order(), manager());

  TestService child_of_1("child_of_1", shutdown_order(), manager());
  DependOn(&child_of_1, &parent1);

  TestService child_of_12("child_of_12", shutdown_order(), manager());
  DependOn(&child_of_12, &parent1);
  DependOn(&child_of_12, &parent2);

  TestService child_of_2("child_of_2", shutdown_order(), manager());
  DependOn(&child_of_2, &parent2);

  manager()->DestroyBrowserContextServices(nullptr);

  ASSERT_EQ(5U, shutdown_order()->size());
  EXPECT_STREQ("child_of_2", (*shutdown_order())[0].c_str());
  EXPECT_STREQ("child_of_12", (*shutdown_order())[1].c_str());
  EXPECT_STREQ("child_of_1", (*shutdown_order())[2].c_str());
  EXPECT_STREQ("parent2", (*shutdown_order())[3].c_str());
  EXPECT_STREQ("parent1", (*shutdown_order())[4].c_str());
}

// Tests that it can deal with a simple diamond.
TEST_F(BrowserContextDependencyManagerUnittests, DiamondConfiguration) {
  TestService parent("parent", shutdown_order(), manager());

  TestService middle_row_1("middle_row_1", shutdown_order(), manager());
  DependOn(&middle_row_1, &parent);

  TestService middle_row_2("middle_row_2", shutdown_order(), manager());
  DependOn(&middle_row_2, &parent);

  TestService bottom("bottom", shutdown_order(), manager());
  DependOn(&bottom, &middle_row_1);
  DependOn(&bottom, &middle_row_2);

  manager()->DestroyBrowserContextServices(nullptr);

  ASSERT_EQ(4U, shutdown_order()->size());
  EXPECT_STREQ("bottom", (*shutdown_order())[0].c_str());
  EXPECT_STREQ("middle_row_2", (*shutdown_order())[1].c_str());
  EXPECT_STREQ("middle_row_1", (*shutdown_order())[2].c_str());
  EXPECT_STREQ("parent", (*shutdown_order())[3].c_str());
}

// A final test that works with a more complex graph.
TEST_F(BrowserContextDependencyManagerUnittests, ComplexGraph) {
  TestService everything_depends_on_me(
      "everything_depends_on_me", shutdown_order(), manager());

  TestService intermediary_service(
      "intermediary_service", shutdown_order(), manager());
  DependOn(&intermediary_service, &everything_depends_on_me);

  TestService specialized_service(
      "specialized_service", shutdown_order(), manager());
  DependOn(&specialized_service, &everything_depends_on_me);
  DependOn(&specialized_service, &intermediary_service);

  TestService other_root("other_root", shutdown_order(), manager());

  TestService other_intermediary(
      "other_intermediary", shutdown_order(), manager());
  DependOn(&other_intermediary, &other_root);

  TestService bottom("bottom", shutdown_order(), manager());
  DependOn(&bottom, &specialized_service);
  DependOn(&bottom, &other_intermediary);

  manager()->DestroyBrowserContextServices(nullptr);

  ASSERT_EQ(6U, shutdown_order()->size());
  EXPECT_STREQ("bottom", (*shutdown_order())[0].c_str());
  EXPECT_STREQ("specialized_service", (*shutdown_order())[1].c_str());
  EXPECT_STREQ("other_intermediary", (*shutdown_order())[2].c_str());
  EXPECT_STREQ("intermediary_service", (*shutdown_order())[3].c_str());
  EXPECT_STREQ("other_root", (*shutdown_order())[4].c_str());
  EXPECT_STREQ("everything_depends_on_me", (*shutdown_order())[5].c_str());
}

// Tests that AreBrowserContextServicesDisabledByDefault() is true only for the
// contexts passed to DisableBrowserContextServicesByDefault().
TEST_F(BrowserContextDependencyManagerUnittests, ServicesDisabledByDefault) {
  content::BrowserTaskEnvironment task_environment;
  content::TestBrowserContext disabled_context;
  content::TestBrowserContext other_context;

  manager()->DisableBrowserContextServicesByDefault(&disabled_context);

  EXPECT_TRUE(
      manager()->AreBrowserContextServicesDisabledByDefault(&disabled_context));
  EXPECT_FALSE(
      manager()->AreBrowserContextServicesDisabledByDefault(&other_context));
}

namespace {

enum class CreationTiming {
  kOnDemand,
  kWithContext,
  kAfterStartup,
};

// A factory whose creation timing is configurable and that records when its
// service was built.
class CreationTimingTestFactory : public BrowserContextKeyedServiceFactory {
 public:
  CreationTimingTestFactory(CreationTiming timing,
                            BrowserContextDependencyManager* manager,
                            const char* name = "CreationTimingTestFactory",
                            std::vector<std::string>* build_order = nullptr)
      : BrowserContextKeyedServiceFactory(name, manager),
        timing_(timing),
        name_(name),
        build_order_(build_order) {}

  int build_count() const { return build_count_; }

  KeyedService* GetForContext(content::BrowserContext* context) {
    return GetServiceForBrowserContext(context, /*create=*/true);
  }

  void set_dependency_to_request_on_build(
      CreationTimingTestFactory* dependency) {
    dependency_to_request_on_build_ = dependency;
  }

 private:
  // BrowserContextKeyedServiceFactory:
  bool ServiceIsCreatedWithBrowserContext() const override {
    return timing_ == CreationTiming::kWithContext;
  }
  bool ServiceIsCreatedAfterStartup() const override {
    return timing_ == CreationTiming::kAfterStartup;
  }
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override {
    return context;
  }
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override {
    if (dependency_to_request_on_build_) {
      EXPECT_TRUE(dependency_to_request_on_build_->GetForContext(context));
    }
    ++build_count_;
    if (build_order_) {
      build_order_->push_back(name_);
    }
    return std::make_unique<KeyedService>();
  }

  const CreationTiming timing_;
  const std::string name_;
  raw_ptr<std::vector<std::string>> build_order_;
  raw_ptr<CreationTimingTestFactory> dependency_to_request_on_build_ = nullptr;
  mutable int build_count_ = 0;
};

class TestRefcountedService : public RefcountedKeyedService {
 public:
  TestRefcountedService() = default;
  void ShutdownOnUIThread() override {}

 private:
  ~TestRefcountedService() override = default;
};

class RefcountedAfterStartupTestFactory
    : public RefcountedBrowserContextKeyedServiceFactory {
 public:
  explicit RefcountedAfterStartupTestFactory(
      BrowserContextDependencyManager* manager)
      : RefcountedBrowserContextKeyedServiceFactory(
            "RefcountedAfterStartupTestFactory",
            manager) {}

  int build_count() const { return build_count_; }

 private:
  // RefcountedBrowserContextKeyedServiceFactory:
  bool ServiceIsCreatedAfterStartup() const override { return true; }
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override {
    return context;
  }
  scoped_refptr<RefcountedKeyedService> BuildServiceInstanceFor(
      content::BrowserContext* context) const override {
    ++build_count_;
    return base::MakeRefCounted<TestRefcountedService>();
  }

  mutable int build_count_ = 0;
};

}  // namespace

// Reuses BrowserContextDependencyManagerUnittests for its (friend-constructed)
// BrowserContextDependencyManager and adds a real BrowserContext.
class BrowserContextDependencyManagerCreationPolicyTest
    : public BrowserContextDependencyManagerUnittests {
 protected:
  content::BrowserContext* context() { return &browser_context_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  content::TestBrowserContext browser_context_;
};

// Tests that ServiceIsCreatedWithBrowserContext() and
// ServiceIsCreatedAfterStartup() are honored by CreateBrowserContextServices()
// and CreateBrowserContextServicesAfterStartup().
TEST_F(BrowserContextDependencyManagerCreationPolicyTest, PolicyIsHonored) {
  CreationTimingTestFactory on_demand(CreationTiming::kOnDemand, manager());
  CreationTimingTestFactory with_context(CreationTiming::kWithContext,
                                         manager());
  CreationTimingTestFactory after_startup(CreationTiming::kAfterStartup,
                                          manager());
  RefcountedAfterStartupTestFactory refcounted_after_startup(manager());

  manager()->MarkBrowserContextLive(context());
  manager()->CreateBrowserContextServices(context());

  EXPECT_EQ(0, on_demand.build_count());
  EXPECT_EQ(1, with_context.build_count());
  EXPECT_EQ(0, after_startup.build_count());
  EXPECT_EQ(0, refcounted_after_startup.build_count());

  manager()->CreateBrowserContextServicesAfterStartup(context());

  EXPECT_EQ(0, on_demand.build_count());
  EXPECT_EQ(1, with_context.build_count());
  EXPECT_EQ(1, after_startup.build_count());
  EXPECT_EQ(1, refcounted_after_startup.build_count());

  manager()->DestroyBrowserContextServices(context());
}

// Tests that a ServiceIsCreatedAfterStartup() service already created on demand
// is not built a second time by CreateBrowserContextServicesAfterStartup().
TEST_F(BrowserContextDependencyManagerCreationPolicyTest,
       AfterStartupServiceCreatedOnDemandFirst) {
  CreationTimingTestFactory after_startup(CreationTiming::kAfterStartup,
                                          manager());

  manager()->MarkBrowserContextLive(context());
  manager()->CreateBrowserContextServices(context());
  EXPECT_EQ(0, after_startup.build_count());

  KeyedService* service = after_startup.GetForContext(context());
  ASSERT_TRUE(service);
  EXPECT_EQ(1, after_startup.build_count());

  manager()->CreateBrowserContextServicesAfterStartup(context());
  EXPECT_EQ(1, after_startup.build_count());
  EXPECT_EQ(service, after_startup.GetForContext(context()));

  manager()->DestroyBrowserContextServices(context());
}

// Tests that CreateBrowserContextServicesAfterStartup() builds services in
// dependency order: when A DependsOn(B), B is built before A.
TEST_F(BrowserContextDependencyManagerCreationPolicyTest,
       AfterStartupDependencyOrder) {
  std::vector<std::string> build_order;
  CreationTimingTestFactory service_a(CreationTiming::kAfterStartup, manager(),
                                      "A", &build_order);
  CreationTimingTestFactory service_b(CreationTiming::kAfterStartup, manager(),
                                      "B", &build_order);
  DependOn(&service_a, &service_b);

  manager()->MarkBrowserContextLive(context());
  manager()->CreateBrowserContextServices(context());
  EXPECT_TRUE(build_order.empty());

  manager()->CreateBrowserContextServicesAfterStartup(context());
  ASSERT_EQ(2u, build_order.size());
  EXPECT_EQ("B", build_order[0]);
  EXPECT_EQ("A", build_order[1]);

  manager()->DestroyBrowserContextServices(context());
}

// Tests that a ServiceIsCreatedWithBrowserContext() service that DependsOn() a
// ServiceIsCreatedAfterStartup() service and calls GetForContext() at build
// time pulls the dependency in during CreateBrowserContextServices(), and that
// CreateBrowserContextServicesAfterStartup() does not build it a second time.
TEST_F(BrowserContextDependencyManagerCreationPolicyTest,
       AfterStartupDependencyPulledInEarlyByWithContextService) {
  std::vector<std::string> build_order;
  CreationTimingTestFactory after_startup_dep(CreationTiming::kAfterStartup,
                                              manager(), "after_startup_dep",
                                              &build_order);
  CreationTimingTestFactory with_context_service(
      CreationTiming::kWithContext, manager(), "with_context_service",
      &build_order);
  DependOn(&with_context_service, &after_startup_dep);
  with_context_service.set_dependency_to_request_on_build(&after_startup_dep);

  manager()->MarkBrowserContextLive(context());
  manager()->CreateBrowserContextServices(context());

  EXPECT_EQ(1, after_startup_dep.build_count());
  EXPECT_EQ(1, with_context_service.build_count());
  ASSERT_EQ(2u, build_order.size());
  EXPECT_EQ("after_startup_dep", build_order[0]);
  EXPECT_EQ("with_context_service", build_order[1]);

  manager()->CreateBrowserContextServicesAfterStartup(context());
  EXPECT_EQ(1, after_startup_dep.build_count());
  EXPECT_EQ(1, with_context_service.build_count());

  manager()->DestroyBrowserContextServices(context());
}
