// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/test/wayland_server_test_base.h"

#include <stdlib.h>

#include <wayland-client-core.h>

#include <memory>

#include "base/atomic_sequence_num.h"
#include "base/bind.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/process/process_handle.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/threading/thread.h"
#include "components/exo/display.h"
#include "components/exo/security_delegate.h"
#include "components/exo/test/exo_test_base_views.h"
#include "components/exo/wayland/server.h"
#include "components/exo/wayland/server_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace exo {
namespace wayland {
namespace test {
namespace {

base::AtomicSequenceNumber g_next_socket_id;

class TestSecurityDelegate : public SecurityDelegate {
 public:
  std::string GetSecurityContext() const override { return "test"; }
};

}  // namespace

// static
std::string WaylandServerTestBase::GetUniqueSocketName() {
  return base::StringPrintf("wayland-test-%d-%d", base::GetCurrentProcId(),
                            g_next_socket_id.GetNext());
}

WaylandServerTestBase::WaylandServerTestBase() = default;

WaylandServerTestBase::~WaylandServerTestBase() = default;

void WaylandServerTestBase::SetUp() {
  ASSERT_TRUE(xdg_temp_dir_.CreateUniqueTempDir());
  setenv("XDG_RUNTIME_DIR", xdg_temp_dir_.GetPath().MaybeAsASCII().c_str(),
         1 /* overwrite */);
  TestBase::SetUp();
  display_ = std::make_unique<Display>();
}

void WaylandServerTestBase::TearDown() {
  display_.reset();
  TestBase::TearDown();
}

std::unique_ptr<Server> WaylandServerTestBase::CreateServer() {
  return CreateServer(std::make_unique<TestSecurityDelegate>());
}

std::unique_ptr<Server> WaylandServerTestBase::CreateServer(
    std::unique_ptr<SecurityDelegate> security_delegate) {
  if (!security_delegate)
    security_delegate = std::make_unique<TestSecurityDelegate>();
  return Server::Create(display_.get(), std::move(security_delegate));
}

WaylandClientRunner::WaylandClientRunner(Server* server,
                                         const std::string& name)
    : base::Thread(name),
      server_(server),
      event_(base::WaitableEvent::ResetPolicy::AUTOMATIC,
             base::WaitableEvent::InitialState::NOT_SIGNALED) {
  Start();
}

void WaylandClientRunner::RunAndWait(base::OnceClosure callback) {
  event_.Reset();

  task_runner()->PostTask(
      FROM_HERE,
      (base::BindOnce(
          [](base::OnceClosure callback, base::WaitableEvent* event) {
            std::move(callback).Run();
            event->Signal();
          },
          std::move(callback), &event_)));

  while (!event_.IsSignaled())
    server_->Dispatch(base::Milliseconds(10));
}

}  // namespace test
}  // namespace wayland
}  // namespace exo
