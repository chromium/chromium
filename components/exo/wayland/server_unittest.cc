// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/server.h"

#include <stdlib.h>

#include <memory>

#include <wayland-client-core.h>
#include <wayland-server-core.h>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/run_loop.h"
#include "base/synchronization/lock.h"
#include "base/test/bind.h"
#include "base/threading/thread.h"
#include "components/exo/display.h"
#include "components/exo/security_delegate.h"
#include "components/exo/wayland/server_util.h"
#include "components/exo/wayland/test/wayland_server_test_base.h"
#include "testing/gmock/include/gmock/gmock.h"
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

TEST_F(ServerTest, AddSocket) {
  auto server = CreateServer(SecurityDelegate::GetDefaultSecurityDelegate());
  // Check that calling AddSocket() with a unique socket name succeeds.
  bool rv = server->AddSocket(GetUniqueSocketName());
  EXPECT_TRUE(rv);
}

TEST_F(ServerTest, GetFileDescriptor) {
  auto server = CreateServer(SecurityDelegate::GetDefaultSecurityDelegate());
  bool rv = server->AddSocket(GetUniqueSocketName());
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

TEST_F(ServerTest, CreateAsync) {
  using MockServerFunction =
      testing::MockFunction<void(bool, const base::FilePath&)>;

  base::ScopedTempDir non_xdg_dir;
  ASSERT_TRUE(non_xdg_dir.CreateUniqueTempDir());

  base::RunLoop run_loop;
  base::FilePath server_socket;
  MockServerFunction server_callback;
  EXPECT_CALL(server_callback, Call(testing::_, testing::_))
      .WillOnce(testing::Invoke([&run_loop, &server_socket](
                                    bool success, const base::FilePath& path) {
        EXPECT_TRUE(success);
        server_socket = path;
        run_loop.Quit();
      }));

  auto server = CreateServer();
  server->StartAsync(base::BindOnce(&MockServerFunction::Call,
                                    base::Unretained(&server_callback)));
  run_loop.Run();

  // Should create a directory for the server.
  EXPECT_TRUE(base::DirectoryExists(server_socket.DirName()));
  // Must not be a child of the XDG dir.
  EXPECT_TRUE(base::IsDirectoryEmpty(xdg_temp_dir_.GetPath()));
  // Must be deleted when the helper is removed.
  server.reset();
  EXPECT_FALSE(base::PathExists(server_socket));
}

TEST_F(ServerTest, Dispatch) {
  auto server = CreateServer(SecurityDelegate::GetDefaultSecurityDelegate());

  std::string socket_name = GetUniqueSocketName();
  bool rv = server->AddSocket(socket_name);
  EXPECT_TRUE(rv);

  base::Thread client_thread("client-" + socket_name);
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
        client_display = wl_display_connect(socket_name.c_str());
        connected_to_server = !!client_display;
      }));

  while (!client_creation_listener.notified)
    server->Dispatch(base::Milliseconds(10));

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

  while (!client_destruction_listener.notified)
    server->Dispatch(base::Milliseconds(10));
}

TEST_F(ServerTest, Flush) {
  auto server = CreateServer(SecurityDelegate::GetDefaultSecurityDelegate());

  bool rv = server->AddSocket(GetUniqueSocketName());
  EXPECT_TRUE(rv);

  // Just call Flush to check that it doesn't have any bad side-effects.
  server->Flush();
}

}  // namespace exo::wayland
