// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/performance_manager_test_harness.h"

#include "base/bind_helpers.h"
#include "components/performance_manager/performance_manager_tab_helper.h"

namespace performance_manager {

PerformanceManagerTestHarness::PerformanceManagerTestHarness() = default;

PerformanceManagerTestHarness::~PerformanceManagerTestHarness() = default;

void PerformanceManagerTestHarness::SetUp() {
  Super::SetUp();
  perf_man_ = PerformanceManagerImpl::Create(base::DoNothing());
}

void PerformanceManagerTestHarness::TearDown() {
  // Have the performance manager destroy itself.
  PerformanceManagerImpl::Destroy(std::move(perf_man_));
  task_environment()->RunUntilIdle();

  Super::TearDown();
}

std::unique_ptr<content::WebContents>
PerformanceManagerTestHarness::CreateTestWebContents() {
  std::unique_ptr<content::WebContents> contents =
      Super::CreateTestWebContents();
  PerformanceManagerTabHelper::CreateForWebContents(contents.get());
  return contents;
}

}  // namespace performance_manager
