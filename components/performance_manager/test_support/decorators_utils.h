// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_TEST_SUPPORT_DECORATORS_UTILS_H_
#define COMPONENTS_PERFORMANCE_MANAGER_TEST_SUPPORT_DECORATORS_UTILS_H_

#include <utility>

#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "components/performance_manager/public/performance_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager {

namespace testing {

// Helper function that allows testing that a decorator class property has the
// expected value. This function should be called from the main thread and be
// passed the WebContents pointer associated with the PageNode to check.
template <class T>
void TestPageNodePropertyOnPMSequence(content::WebContents* contents,
                                      bool (T::*getter)() const,
                                      bool expected_value);

// Helper function that simulates a change in a property of a page node
// decorator and test if the property gets update.
template <class T>
void EndToEndBooleanPropertyTest(content::WebContents* contents,
                                 bool (T::*pm_getter)() const,
                                 void (*ui_thread_setter)(content::WebContents*,
                                                          bool),
                                 bool default_state = false);

// Implementation details:

template <class T>
void TestPageNodePropertyOnPMSequence(content::WebContents* contents,
                                      bool (T::*getter)() const,
                                      bool expected_value) {
  base::RunLoop run_loop;
  auto quit_closure = run_loop.QuitClosure();

  base::WeakPtr<PageNode> node =
      PerformanceManager::GetPageNodeForWebContents(contents);

  PerformanceManager::CallOnGraph(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        EXPECT_TRUE(node);
        auto* data = T::GetOrCreateForTesting(node.get());
        EXPECT_TRUE(data);
        EXPECT_EQ((data->*getter)(), expected_value);
        std::move(quit_closure).Run();
      }));
  run_loop.Run();
}

template <class T>
void EndToEndBooleanPropertyTest(content::WebContents* contents,
                                 bool (T::*pm_getter)() const,
                                 void (*ui_thread_setter)(content::WebContents*,
                                                          bool),
                                 bool default_state) {
  // By default all properties are set to the default value.
  TestPageNodePropertyOnPMSequence(contents, pm_getter, default_state);

  // Pretend that the property changed and make sure that the PageNode data gets
  // updated.
  (*ui_thread_setter)(contents, !default_state);
  TestPageNodePropertyOnPMSequence(contents, pm_getter, !default_state);

  // Switch back to the default state.
  (*ui_thread_setter)(contents, default_state);
  TestPageNodePropertyOnPMSequence(contents, pm_getter, default_state);
}

}  // namespace testing

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_TEST_SUPPORT_DECORATORS_UTILS_H_
