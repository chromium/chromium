// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "components/viz/test/test_gpu_service_holder.h"
#include "components/viz/test/viz_test_suite.h"
#include "mojo/core/embedder/embedder.h"
#include "skia/ext/event_tracer_impl.h"

int main(int argc, char** argv) {
  viz::VizTestSuite test_suite(argc, argv);

  mojo::core::Init();

  InitSkiaEventTracer();

  viz::TestGpuServiceHolder::DoNotResetOnTestExit();

  // Always run the perf tests serially, to avoid distorting
  // perf measurements with randomness resulting from running
  // in parallel.
  return base::LaunchUnitTestsSerially(
      argc, argv,
      base::BindOnce(&viz::VizTestSuite::Run, base::Unretained(&test_suite)));
}
