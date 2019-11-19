// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_VIEWS_CHROME_VIEWS_TEST_BASE_H_
#define CHROME_TEST_VIEWS_CHROME_VIEWS_TEST_BASE_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/traits_bag.h"
#include "content/public/test/browser_task_environment.h"
#include "ui/views/test/views_test_base.h"

// A base class for Chrome views unit tests. Changes the dependencies when they
// need to be different than non-Chrome views.
class ChromeViewsTestBase : public views::ViewsTestBase {
 public:
  // Constructs a ChromeViewsTestBase with |traits| being forwarded to its
  // BrowserTaskEnvironment. MainThreadType always defaults to UI and must not
  // be specified. TimeSource defaults to MOCK_TIME but can be specified to
  // override.
  template <typename... TaskEnvironmentTraits>
  NOINLINE explicit ChromeViewsTestBase(TaskEnvironmentTraits... traits)
      : views::ViewsTestBase(
            views::ViewsTestBase::SubclassManagesTaskEnvironment()),
        task_environment_(
            content::BrowserTaskEnvironment::MainThreadType::UI,
            base::trait_helpers::GetEnum<
                content::BrowserTaskEnvironment::TimeSource,
                content::BrowserTaskEnvironment::TimeSource::MOCK_TIME>(
                traits...),
            base::trait_helpers::
                Exclude<content::BrowserTaskEnvironment::TimeSource>::Filter(
                    traits)...) {}
  ~ChromeViewsTestBase() override;

  // views::ViewsTestBase:
  void SetUp() override;

 protected:
  // Use this protected member directly to drive tasks posted within a
  // ChromeViewsTestBase-based test.
  content::BrowserTaskEnvironment task_environment_;

  DISALLOW_COPY_AND_ASSIGN(ChromeViewsTestBase);
};

#endif  // CHROME_TEST_VIEWS_CHROME_VIEWS_TEST_BASE_H_
