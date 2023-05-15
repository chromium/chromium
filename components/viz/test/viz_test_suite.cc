// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/test/viz_test_suite.h"

#include "base/threading/thread_id_name_manager.h"
#include "components/viz/test/paths.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/gl/test/gl_surface_test_support.h"

namespace viz {

std::unique_ptr<base::test::TaskEnvironment> VizTestSuite::task_environment_;

VizTestSuite::VizTestSuite(int argc, char** argv)
    : base::TestSuite(argc, argv) {}

VizTestSuite::~VizTestSuite() = default;

// static
void VizTestSuite::RunUntilIdle() {
  CHECK(task_environment_);
  task_environment_->RunUntilIdle();
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
