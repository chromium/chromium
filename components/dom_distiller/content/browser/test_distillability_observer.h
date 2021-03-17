// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOM_DISTILLER_CONTENT_BROWSER_TEST_DISTILLABILITY_OBSERVER_H_
#define COMPONENTS_DOM_DISTILLER_CONTENT_BROWSER_TEST_DISTILLABILITY_OBSERVER_H_

#include "base/macros.h"
#include "base/run_loop.h"
#include "components/dom_distiller/content/browser/distillable_page_utils.h"
#include "content/public/browser/web_contents.h"

namespace dom_distiller {

// Used to wait for desired distillability results in tests.
class TestDistillabilityObserver : public DistillabilityObserver {
 public:
  explicit TestDistillabilityObserver(content::WebContents* web_contents);

  ~TestDistillabilityObserver() override;

  TestDistillabilityObserver(const TestDistillabilityObserver&) = delete;
  TestDistillabilityObserver& operator=(const TestDistillabilityObserver&) =
      delete;

  // Returns immediately if the result has already happened, otherwise
  // blocks and waits until that result is observed.
  void WaitForResult(const DistillabilityResult& result);

 private:
  void OnResult(const DistillabilityResult& result) override;

  bool WasResultFound(const DistillabilityResult& result);

  content::WebContents* web_contents_;
  std::unique_ptr<base::RunLoop> run_loop_;
  base::Optional<DistillabilityResult> result_to_wait_for_;
  std::vector<DistillabilityResult> results_;
};

}  // namespace dom_distiller

#endif  // COMPONENTS_DOM_DISTILLER_CONTENT_BROWSER_TEST_DISTILLABILITY_OBSERVER_H_
