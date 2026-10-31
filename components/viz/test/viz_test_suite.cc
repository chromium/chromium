// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/test/viz_test_suite.h"

#include <memory>

#include "base/check.h"
#include "base/threading/thread_id_name_manager.h"
#include "components/viz/test/paths.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/gl/test/gl_surface_test_support.h"

namespace viz {

namespace {
VizTestSuite* g_viz_test_suite = nullptr;
}  // namespace

VizTestSuite::VizTestSuite(int argc, char** argv)
    : base::TestSuite(argc, argv) {
  CHECK(!g_viz_test_suite);
  g_viz_test_suite = this;
}

VizTestSuite::~VizTestSuite() {
  CHECK_EQ(g_viz_test_suite, this);
  g_viz_test_suite = nullptr;
}

// static
void VizTestSuite::RunUntilIdle() {
  CHECK(g_viz_test_suite);
  CHECK(g_viz_test_suite->task_environment_);
  g_viz_test_suite->task_environment_->RunUntilIdle();
}

void VizTestSuite::Initialize() {
  base::TestSuite::Initialize();

  // Must be initialized after time outs are initialized in by the TestSuite.
  CHECK(!task_environment_);
  task_environment_ = std::make_unique<base::test::TaskEnvironment>(
      base::test::TaskEnvironment::MainThreadType::UI);

  platform_event_source_ = ui::PlatformEventSource::CreateDefault();

  gl::GLSurfaceTestSupport::InitializeOneOff();
  Paths::RegisterPathProvider();

  base::ThreadIdNameManager::GetInstance()->SetName("Main");

  base::DiscardableMemoryAllocator::SetInstance(&discardable_memory_allocator_);
}

void VizTestSuite::Shutdown() {
  platform_event_source_.reset();
  task_environment_.reset();
  base::TestSuite::Shutdown();
}

}  // namespace viz
