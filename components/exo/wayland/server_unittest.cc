// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/server.h"

#include <stdlib.h>

#include <wayland-client-core.h>

#include <memory>

#include "base/atomic_sequence_num.h"
#include "base/bind.h"
#include "base/files/scoped_temp_dir.h"
#include "base/process/process_handle.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread.h"
#include "components/exo/display.h"
#include "components/exo/test/exo_test_base_views.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace exo {
namespace wayland {
namespace {

base::AtomicSequenceNumber g_next_socket_id;

std::string GetUniqueSocketName() {
  return base::StringPrintf("wayland-test-%d-%d", base::GetCurrentProcId(),
                            g_next_socket_id.GetNext());
}

class ServerTest : public test::ExoTestBaseViews {
 public:
  ServerTest() {}
  ~ServerTest() override {}

  void SetUp() override {
    ASSERT_TRUE(xdg_temp_dir_.CreateUniqueTempDir());
    setenv("XDG_RUNTIME_DIR", xdg_temp_dir_.GetPath().MaybeAsASCII().c_str(),
           1 /* overwrite */);
    test::ExoTestBaseViews::SetUp();
  }

 private:
  base::ScopedTempDir xdg_temp_dir_;

  DISALLOW_COPY_AND_ASSIGN(ServerTest);
};

TEST_F(ServerTest, AddSocket) {
  std::unique_ptr<Display> display(new Display);
  std::unique_ptr<Server> server(new Server(display.get()));

  // Check that calling AddSocket() with a unique socket name succeeds.
  bool rv = server->AddSocket(GetUniqueSocketName());
  EXPECT_TRUE(rv);
}

TEST_F(ServerTest, GetFileDescriptor) {
  std::unique_ptr<Display> display(new Display);
  std::unique_ptr<Server> server(new Server(display.get()));

  bool rv = server->AddSocket(GetUniqueSocketName());
  EXPECT_TRUE(rv);

  // Check that the returned file descriptor is valid.
  int fd = server->GetFileDescriptor();
  DCHECK_GE(fd, 0);
}

void ConnectToServer(const std::string socket_name,
                     bool* connected_to_server,
                     base::WaitableEvent* event) {
  wl_display* display = wl_display_connect(socket_name.c_str());
  *connected_to_server = !!display;
  event->Signal();
  wl_display_disconnect(display);
}

TEST_F(ServerTest, Dispatch) {
  std::unique_ptr<Display> display(new Display);
  std::unique_ptr<Server> server(new Server(display.get()));

  std::string socket_name = GetUniqueSocketName();
  bool rv = server->AddSocket(socket_name);
  EXPECT_TRUE(rv);

  base::Thread client("client-" + socket_name);
  ASSERT_TRUE(client.Start());

  // Post a task that connects server on the created thread.
  bool connected_to_server = false;
  base::WaitableEvent event(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                            base::WaitableEvent::InitialState::NOT_SIGNALED);
  client.task_runner()->PostTask(FROM_HERE,
                                 base::BindOnce(&ConnectToServer, socket_name,
                                                &connected_to_server, &event));

  // Call Dispatch() with a 5 second timeout.
  server->Dispatch(base::TimeDelta::FromSeconds(5));

  // Check if client thread managed to connect to server.
  event.Wait();
  EXPECT_TRUE(connected_to_server);
}

TEST_F(ServerTest, Flush) {
  std::unique_ptr<Display> display(new Display);
  std::unique_ptr<Server> server(new Server(display.get()));

  bool rv = server->AddSocket(GetUniqueSocketName());
  EXPECT_TRUE(rv);

  // Just call Flush to check that it doesn't have any bad side-effects.
  server->Flush();
}

}  // namespace
}  // namespace wayland
}  // namespace exo
