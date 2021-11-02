// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
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

  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override {
    ADD_FAILURE() << "This isn't part of the tests!";
    return nullptr;
  }

  void BrowserContextShutdown(content::BrowserContext* context) override {
    fill_on_shutdown_->push_back(name_);
  }

 private:
  const std::string name_;
  std::vector<std::string>* fill_on_shutdown_;
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
