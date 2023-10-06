// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/test_support/performance_manager_test_harness.h"

#include <memory>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/test/bind.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"

namespace performance_manager {

PerformanceManagerTestHarness::PerformanceManagerTestHarness() {
  // Ensure the helper is available at construction so that graph features and
  // callbacks can be configured.
  helper_ = std::make_unique<PerformanceManagerTestHarnessHelper>();
}

PerformanceManagerTestHarness::~PerformanceManagerTestHarness() = default;

void PerformanceManagerTestHarness::SetUp() {
  DCHECK(helper_);
  Super::SetUp();
  helper_->SetGraphImplCallback(base::BindOnce(
      &PerformanceManagerTestHarness::OnGraphCreated, base::Unretained(this)));
  helper_->SetUp();
  helper_->OnBrowserContextAdded(GetBrowserContext());
}

void PerformanceManagerTestHarness::TearDown() {
  if (helper_) {
    TearDownNow();
  }
  Super::TearDown();
}

std::unique_ptr<content::WebContents>
PerformanceManagerTestHarness::CreateTestWebContents() {
  DCHECK(helper_);
  std::unique_ptr<content::WebContents> contents =
      Super::CreateTestWebContents();
  helper_->OnWebContentsCreated(contents.get());
  return contents;
}

void PerformanceManagerTestHarness::TearDownNow() {
  helper_->OnBrowserContextRemoved(GetBrowserContext());
  helper_->TearDown();
  helper_.reset();
}

}  // namespace performance_manager
