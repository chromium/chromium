// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/content/browser/test_distillability_observer.h"

#include "components/dom_distiller/content/browser/distillability_driver.h"
#include "components/dom_distiller/content/browser/distillable_page_utils.h"

namespace dom_distiller {

TestDistillabilityObserver::TestDistillabilityObserver(
    content::WebContents* web_contents)
    : web_contents_(web_contents),
      run_loop_(std::make_unique<base::RunLoop>()) {
  AddObserver(web_contents_, this);
}

TestDistillabilityObserver::~TestDistillabilityObserver() {
  if (web_contents_ && DistillabilityObserver::IsInObserverList())
    RemoveObserver(web_contents_, this);
}

void TestDistillabilityObserver::WaitForResult(
    const DistillabilityResult& result) {
  if (WasResultFound(result)) {
    results_.clear();
    return;
  }

  result_to_wait_for_ = result;
  run_loop_->Run();
  run_loop_ = std::make_unique<base::RunLoop>();
  results_.clear();
}

void TestDistillabilityObserver::OnResult(const DistillabilityResult& result) {
  results_.push_back(result);
  // If we aren't waiting for anything yet, return early.
  if (!result_to_wait_for_.has_value())
    return;
  // Check if this is the one we were waiting for, and if so, stop the loop.
  if (result == result_to_wait_for_.value())
    run_loop_->Quit();
}

bool TestDistillabilityObserver::WasResultFound(
    const DistillabilityResult& result) {
  for (auto& elem : results_) {
    if (result == elem)
      return true;
  }
  return false;
}

bool TestDistillabilityObserver::IsDistillabilityDriverTimerRunning() {
  DistillabilityDriver::CreateForWebContents(web_contents_);
  DistillabilityDriver* driver =
      DistillabilityDriver::FromWebContents(web_contents_);
  return driver->GetTimer().HasStarted();
}

}  // namespace dom_distiller
