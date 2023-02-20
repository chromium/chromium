// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/test/surface_tree_host_test_util.h"

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"

namespace exo::test {

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
