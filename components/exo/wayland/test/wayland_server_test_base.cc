// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/test/wayland_server_test_base.h"

#include <stdlib.h>

#include <wayland-client-core.h>

#include <memory>

#include "base/atomic_sequence_num.h"
#include "base/process/process_handle.h"
#include "base/strings/stringprintf.h"
#include "components/exo/display.h"
#include "components/exo/security_delegate.h"
#include "components/exo/test/exo_test_base_views.h"
#include "components/exo/test/test_security_delegate.h"
#include "components/exo/wayland/server.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace exo {
namespace wayland {
namespace test {
namespace {

base::AtomicSequenceNumber g_next_socket_id;

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
  return CreateServer(std::make_unique<::exo::test::TestSecurityDelegate>());
}

std::unique_ptr<Server> WaylandServerTestBase::CreateServer(
    std::unique_ptr<SecurityDelegate> security_delegate) {
  if (!security_delegate)
    security_delegate = std::make_unique<::exo::test::TestSecurityDelegate>();
  return Server::Create(display_.get(), std::move(security_delegate));
}

}  // namespace test
}  // namespace wayland
}  // namespace exo
