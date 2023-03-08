// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/fuzzer/server_environment.h"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <memory>

#include "base/atomic_sequence_num.h"
#include "base/command_line.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/i18n/rtl.h"
#include "base/message_loop/message_pump_type.h"
#include "base/path_service.h"
#include "base/test/icu_test_util.h"
#include "chromeos/ash/components/test/ash_test_suite.h"
#include "components/exo/display.h"
#include "components/exo/wayland/server.h"
#include "mojo/core/embedder/embedder.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_paths.h"
#include "ui/base/ui_base_switches.h"
#include "ui/gl/test/gl_surface_test_support.h"

namespace exo {
namespace wayland_fuzzer {

ServerEnvironment::ServerEnvironment()
    : WaylandClientTestHelper(), ui_thread_("ui") {
  base::CommandLine::Init(0, nullptr);

  base::Thread::Options ui_options(base::MessagePumpType::UI, 0);
  ui_thread_.StartWithOptions(std::move(ui_options));
  WaylandClientTestHelper::SetUIThreadTaskRunner(ui_thread_.task_runner());

  RunOnUiThreadBlocking(base::BindOnce(
      &ServerEnvironment::OneTimeSetupOnUiThread, base::Unretained(this)));
}

ServerEnvironment::~ServerEnvironment() = default;

void ServerEnvironment::RunOnUiThread(base::OnceClosure task) {
  ui_thread_.task_runner()->PostTask(FROM_HERE, std::move(task));
}

void ServerEnvironment::RunOnUiThreadBlocking(base::OnceClosure task) {
  base::RunLoop loop;
  ui_thread_.task_runner()->PostTaskAndReply(FROM_HERE, std::move(task),
                                             loop.QuitClosure());
  loop.Run();
}

void ServerEnvironment::OneTimeSetupOnUiThread() {
  mojo::core::Init();

  base::test::InitializeICUForTesting();

  gl::GLSurfaceTestSupport::InitializeOneOff();

  ui::RegisterPathProvider();

  // Force unittests to run using en-US so if we test against string output,
  // it'll pass regardless of the system language.
  base::i18n::SetICUDefaultLocale("en_US");

  ash::AshTestSuite::LoadTestResources();

  env_ = aura::Env::CreateInstance();
}

}  // namespace wayland_fuzzer
}  // namespace exo
