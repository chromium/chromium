// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/clients/test/wayland_client_test_helper.h"

#include <stdlib.h>

#include <memory>

#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/test/ash_test_helper.h"
#include "base/command_line.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/current_thread.h"
#include "base/task/single_thread_task_runner.h"
#include "components/exo/data_exchange_delegate.h"
#include "components/exo/display.h"
#include "components/exo/input_method_surface_manager.h"
#include "components/exo/notification_surface_manager.h"
#include "components/exo/test/test_security_delegate.h"
#include "components/exo/toast_surface_manager.h"
#include "components/exo/wayland/server.h"
#include "components/exo/wm_helper.h"
#include "ui/aura/window_tree_host.h"
#include "ui/wm/core/cursor_manager.h"
#include "ui/wm/core/wm_core_switches.h"

namespace exo {

// The ui message loop for running the wayland server. If it is not provided, we
// will use external wayland server.
scoped_refptr<base::SingleThreadTaskRunner> ui_thread_task_runner_ = nullptr;

// static
void WaylandClientTestHelper::SetUIThreadTaskRunner(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  DCHECK_NE(!!task_runner, !!ui_thread_task_runner_);
  ui_thread_task_runner_ = std::move(task_runner);
}

WaylandClientTestHelper::WaylandClientTestHelper() = default;
WaylandClientTestHelper::~WaylandClientTestHelper() = default;

void WaylandClientTestHelper::SetUp() {
  if (!ui_thread_task_runner_) {
    return;
  }

  DCHECK(!ui_thread_task_runner_->BelongsToCurrentThread());

  base::WaitableEvent event(base::WaitableEvent::ResetPolicy::MANUAL,
                            base::WaitableEvent::InitialState::NOT_SIGNALED);
  ui_thread_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&WaylandClientTestHelper::SetUpOnUIThread,
                                base::Unretained(this), &event));
  event.Wait();
}

void WaylandClientTestHelper::TearDown() {
  if (!ui_thread_task_runner_) {
    return;
  }

  DCHECK(ui_thread_task_runner_);
  DCHECK(!ui_thread_task_runner_->BelongsToCurrentThread());

  base::WaitableEvent event(base::WaitableEvent::ResetPolicy::MANUAL,
                            base::WaitableEvent::InitialState::NOT_SIGNALED);
  ui_thread_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&WaylandClientTestHelper::TearDownOnUIThread,
                                base::Unretained(this), &event));
  event.Wait();
}

void WaylandClientTestHelper::SetUpOnUIThread(base::WaitableEvent* event) {
  xdg_temp_dir_ = std::make_unique<base::ScopedTempDir>();
  CHECK(xdg_temp_dir_->CreateUniqueTempDir());
  setenv("XDG_RUNTIME_DIR", xdg_temp_dir_->GetPath().MaybeAsASCII().c_str(),
         /*overwrite=*/1);

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  // Disable window animation when running tests.
  command_line->AppendSwitch(wm::switches::kWindowAnimationsDisabled);

  ash_test_helper_ = std::make_unique<ash::AshTestHelper>();
  ash_test_helper_->SetUp();

  wm_helper_ = std::make_unique<WMHelper>();
  display_ = std::make_unique<Display>(nullptr, nullptr, nullptr, nullptr);

  wayland_server_ = exo::wayland::Server::Create(
      display_.get(), std::make_unique<test::TestSecurityDelegate>());
  DCHECK(wayland_server_);
  wayland_server_->StartWithDefaultPath(base::BindOnce(
      [](base::WaitableEvent* event, bool success) {
        DCHECK(success);
        event->Signal();
      },
      event));
}

void WaylandClientTestHelper::TearDownOnUIThread(base::WaitableEvent* event) {
  wayland_server_.reset();
  display_.reset();
  wm_helper_.reset();

  ash::Shell::Get()->session_controller()->NotifyChromeTerminating();
  ash_test_helper_.reset();
  xdg_temp_dir_ = nullptr;
  event->Signal();
}

}  // namespace exo
