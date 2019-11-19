// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/fuzzer/server_environment.h"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <memory>

#include "base/atomic_sequence_num.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/scoped_temp_dir.h"
#include "base/i18n/rtl.h"
#include "base/message_loop/message_pump_type.h"
#include "base/path_service.h"
#include "base/test/icu_test_util.h"
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
  mojo::core::Init();

  base::CommandLine::Init(0, nullptr);

  base::Thread::Options ui_options(base::MessagePumpType::UI, 0);
  ui_thread_.StartWithOptions(ui_options);
  WaylandClientTestHelper::SetUIThreadTaskRunner(ui_thread_.task_runner());
}

ServerEnvironment::~ServerEnvironment() = default;

void ServerEnvironment::SetUpOnUIThread(base::WaitableEvent* event) {
  base::test::InitializeICUForTesting();

  gl::GLSurfaceTestSupport::InitializeOneOff();

  ui::RegisterPathProvider();

  // Force unittests to run using en-US so if we test against string output,
  // it'll pass regardless of the system language.
  base::i18n::SetICUDefaultLocale("en_US");

  // Load ash test resources and en-US strings; not 'common' (Chrome)
  // resources.
  base::FilePath path;
  base::PathService::Get(base::DIR_MODULE, &path);
  base::FilePath ash_test_strings =
      path.Append(FILE_PATH_LITERAL("ash_test_strings.pak"));
  ui::ResourceBundle::InitSharedInstanceWithPakPath(ash_test_strings);
  if (ui::ResourceBundle::IsScaleFactorSupported(ui::SCALE_FACTOR_100P)) {
    base::FilePath ash_test_resources_100 =
        path.AppendASCII("ash_test_resources_100_percent.pak");
    ui::ResourceBundle::GetSharedInstance().AddDataPackFromPath(
        ash_test_resources_100, ui::SCALE_FACTOR_100P);
  }
  if (ui::ResourceBundle::IsScaleFactorSupported(ui::SCALE_FACTOR_200P)) {
    base::FilePath ash_test_resources_200 =
        path.Append(FILE_PATH_LITERAL("ash_test_resources_200_percent.pak"));
    ui::ResourceBundle::GetSharedInstance().AddDataPackFromPath(
        ash_test_resources_200, ui::SCALE_FACTOR_200P);
  }

  env_ = aura::Env::CreateInstance();
  WaylandClientTestHelper::SetUpOnUIThread(event);
}

}  // namespace wayland_fuzzer
}  // namespace exo
