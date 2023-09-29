// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/test/surface_tree_host_test_util.h"

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"

namespace exo::test {

void SetFrameSubmissionFeatureFlags(base::test::ScopedFeatureList* feature_list,
                                    FrameSubmissionType frame_submission) {
  switch (frame_submission) {
    case FrameSubmissionType::kNoReactive: {
      feature_list->InitWithFeatures(
          /*enabled_features=*/{},
          /*disabled_features=*/{kExoReactiveFrameSubmission});
      break;
    }
    case FrameSubmissionType::kReactive_NoAutoNeedsBeginFrame: {
      feature_list->InitWithFeatures(
          /*enabled_features=*/{kExoReactiveFrameSubmission},
          /*disabled_features=*/{kExoAutoNeedsBeginFrame});
      break;
    }
    case FrameSubmissionType::kReactive_AutoNeedsBeginFrame: {
      feature_list->InitWithFeatures(
          /*enabled_features=*/{kExoReactiveFrameSubmission,
                                kExoAutoNeedsBeginFrame},
          /*disabled_features=*/{});
      break;
    }
  }
}

void WaitForLastFrameAck(SurfaceTreeHost* surface_tree_host) {
  CHECK(!surface_tree_host->GetFrameCallbacksForTesting().empty());

  auto& list = surface_tree_host->GetFrameCallbacksForTesting().back();
  base::RunLoop runloop;
  list.push_back(base::BindRepeating(
      [](base::RepeatingClosure callback, base::TimeTicks) { callback.Run(); },
      runloop.QuitClosure()));
  runloop.Run();
}

void WaitForLastFramePresentation(SurfaceTreeHost* surface_tree_host) {
  CHECK(!surface_tree_host->GetActivePresentationCallbacksForTesting().empty());

  auto& list = surface_tree_host->GetActivePresentationCallbacksForTesting()
                   .rbegin()
                   ->second;
  base::RunLoop runloop;
  list.push_back(base::BindRepeating(
      [](base::RepeatingClosure callback, const gfx::PresentationFeedback&) {
        callback.Run();
      },
      runloop.QuitClosure()));
  runloop.Run();
}

}  // namespace exo::test
