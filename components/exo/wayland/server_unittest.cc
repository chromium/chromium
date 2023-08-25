// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/server.h"

#include <stdlib.h>

#include <memory>

#include <wayland-client-core.h>
#include <wayland-server-core.h>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/synchronization/lock.h"
#include "base/test/bind.h"
#include "base/threading/thread.h"
#include "components/exo/display.h"
#include "components/exo/security_delegate.h"
#include "components/exo/wayland/server_util.h"
#include "components/exo/wayland/test/wayland_server_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace exo::wayland {

using ServerTest = test::WaylandServerTestBase;

struct TestListener {
 public:
  TestListener();

  wl_listener listener;
  bool notified = false;
};

TestListener::TestListener() {
  listener.notify = [](wl_listener* listener_ptr, void* data) {
    TestListener* test_listener = wl_container_of(
        listener_ptr, /*sample=*/test_listener, /*member=*/listener);
    test_listener->notified = true;
  };
}

TEST_F(ServerTest, Open) {
  auto server = CreateServer();
  // Check that calling Open() succeeds.
  bool rv = server->Open();
  EXPECT_TRUE(rv);
}

TEST_F(ServerTest, GetFileDescriptor) {
  auto server = CreateServer();
  bool rv = server->Open();
  EXPECT_TRUE(rv);

  // Check that the returned file descriptor is valid.
  int fd = server->GetFileDescriptor();
  DCHECK_GE(fd, 0);
}

TEST_F(ServerTest, SecurityDelegateAssociation) {
  std::unique_ptr<SecurityDelegate> security_delegate =
      SecurityDelegate::GetDefaultSecurityDelegate();
  SecurityDelegate* security_delegate_ptr = security_delegate.get();

  auto server = CreateServer(std::move(security_delegate));

  EXPECT_EQ(GetSecurityDelegate(server->GetWaylandDisplayForTesting()),
            security_delegate_ptr);
}

TEST_F(ServerTest, StartFd) {
  ScopedTempSocket sock;

  auto server = CreateServer();
  base::RunLoop start_loop;
  server->StartWithFdAsync(sock.TakeFd(),
                           base::BindLambdaForTesting([&](bool success) {
                             EXPECT_TRUE(success);
                             start_loop.Quit();
                           }));
  start_loop.Run();

  base::Thread client_thread("client");
  client_thread.Start();

  wl_display* client_display = nullptr;
  base::RunLoop connect_loop;
  client_thread.task_runner()->PostTaskAndReply(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        client_display =
            wl_display_connect(sock.server_path().MaybeAsASCII().c_str());
        int events = wl_display_roundtrip(client_display);
        EXPECT_GE(events, 0);
      }),
      connect_loop.QuitClosure());
  connect_loop.Run();
  EXPECT_NE(client_display, nullptr);

  wl_list* all_clients =
      wl_display_get_client_list(server->GetWaylandDisplayForTesting());
  ASSERT_FALSE(wl_list_empty(all_clients));
  wl_client* client = wl_client_from_link(all_clients->next);

  TestListener client_destruction_listener;
  wl_client_add_destroy_listener(client, &client_destruction_listener.listener);

  client_thread.task_runner()->PostTask(
      FROM_HERE, base::BindLambdaForTesting(
                     [&]() { wl_display_disconnect(client_display); }));

  while (!client_destruction_listener.notified) {
    server->Dispatch(base::Milliseconds(10));
  }
}

TEST_F(ServerTest, Dispatch) {
  auto server = CreateServer();
  bool rv = server->Open();
  EXPECT_TRUE(rv);

  base::Thread client_thread("client");
  client_thread.Start();

  TestListener client_creation_listener;
  wl_display_add_client_created_listener(server->GetWaylandDisplayForTesting(),
                                         &client_creation_listener.listener);

  base::Lock lock;
  wl_display* client_display = nullptr;
  bool connected_to_server = false;

  client_thread.task_runner()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        // As soon as wl_display_connect() is executed, the server side could
        // notify client creation and exit the while-loop. Therefore, the lock
        // is required to ensure `connected_to_server` is set before it is
        // accessed on the main thread.
        base::AutoLock locker(lock);
        client_display = wl_display_connect(nullptr);
        connected_to_server = !!client_display;
      }));

  while (!client_creation_listener.notified) {
    server->Dispatch(base::Milliseconds(10));
  }

  {
    base::AutoLock locker(lock);
    EXPECT_TRUE(connected_to_server);
  }

  wl_list* all_clients =
      wl_display_get_client_list(server->GetWaylandDisplayForTesting());
  ASSERT_FALSE(wl_list_empty(all_clients));
  wl_client* client = wl_client_from_link(all_clients->next);

  TestListener client_destruction_listener;
  wl_client_add_destroy_listener(client, &client_destruction_listener.listener);

  client_thread.task_runner()->PostTask(
      FROM_HERE, base::BindLambdaForTesting(
                     [&]() { wl_display_disconnect(client_display); }));

  while (!client_destruction_listener.notified) {
    server->Dispatch(base::Milliseconds(10));
  }
}

TEST_F(ServerTest, Flush) {
  auto server = CreateServer();
  bool rv = server->Open();
  EXPECT_TRUE(rv);

  // Just call Flush to check that it doesn't have any bad side-effects.
  server->Flush();
}

}  // namespace exo::wayland
