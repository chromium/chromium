// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/renovations/page_renovator.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/test/mock_callback.h"
#include "base/values.h"
#include "components/offline_pages/core/renovations/script_injector.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace offline_pages {

class PageRenovatorTest : public testing::Test {
 public:
  PageRenovatorTest();
  ~PageRenovatorTest() override;

 protected:
  // ScriptInjector for testing PageRenovator. When Inject is called,
  // it sets |script_injector_inject_called_| in the fixture to true,
  // then calls the callback.
  class FakeScriptInjector : public ScriptInjector {
   public:
    FakeScriptInjector(PageRenovatorTest* fixture);
    ~FakeScriptInjector() override = default;

    void Inject(base::string16 script, ResultCallback callback) override;

   private:
    PageRenovatorTest* fixture_;
  };

  // Creates a PageRenovator with simple defaults for testing.
  void CreatePageRenovator(const GURL& url);

  PageRenovationLoader page_renovation_loader_;
  bool script_injector_inject_called_ = false;

  // PageRenovator under test.
  std::unique_ptr<PageRenovator> page_renovator_;
};

PageRenovatorTest::FakeScriptInjector::FakeScriptInjector(
    PageRenovatorTest* fixture)
    : fixture_(fixture) {}

void PageRenovatorTest::FakeScriptInjector::Inject(base::string16 script,
                                                   ResultCallback callback) {
  if (callback)
    std::move(callback).Run(base::Value());
  fixture_->script_injector_inject_called_ = true;
}

PageRenovatorTest::PageRenovatorTest() {
  // Set PageRenovationLoader to have empty script and Renovation list.
  page_renovation_loader_.SetSourceForTest(base::string16());
  page_renovation_loader_.SetRenovationsForTest(
      std::vector<std::unique_ptr<PageRenovation>>());

  page_renovator_ = std::make_unique<PageRenovator>(
      &page_renovation_loader_, std::make_unique<FakeScriptInjector>(this),
      GURL("example.com"));
}

PageRenovatorTest::~PageRenovatorTest() {}

TEST_F(PageRenovatorTest, InjectsScript) {
  EXPECT_FALSE(script_injector_inject_called_);
  page_renovator_->RunRenovations(base::NullCallback());
  EXPECT_TRUE(script_injector_inject_called_);
}

TEST_F(PageRenovatorTest, CallsCallback) {
  base::MockCallback<base::OnceClosure> mock_callback;
  EXPECT_CALL(mock_callback, Run()).Times(1);

  page_renovator_->RunRenovations(mock_callback.Get());
}

}  // namespace offline_pages
