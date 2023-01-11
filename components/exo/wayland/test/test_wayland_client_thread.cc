// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/test/test_wayland_client_thread.h"

#include <utility>

#include <wayland-client-core.h>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/task/current_thread.h"

namespace exo::wayland::test {

TestWaylandClientThread::TestWaylandClientThread(const std::string& name)
    : Thread(name), controller_(FROM_HERE) {}

TestWaylandClientThread::~TestWaylandClientThread() {
  // Stop watching the descriptor here to guarantee that no new events will come
  // during or after the destruction of the display.
  controller_.StopWatchingFileDescriptor();

  task_runner()->PostTask(FROM_HERE,
                          base::BindOnce(&TestWaylandClientThread::DoCleanUp,
                                         base::Unretained(this)));

  // Ensure the task above is run.
  FlushForTesting();
}

bool TestWaylandClientThread::Start(
    base::OnceCallback<std::unique_ptr<TestClient>()> init_callback) {
  base::Thread::Options options;
  options.message_pump_type = base::MessagePumpType::IO;
  CHECK(base::Thread::StartWithOptions(std::move(options)));

  RunAndWait(base::BindOnce(&TestWaylandClientThread::DoInit,
                            base::Unretained(this), std::move(init_callback)));

  return !!client_;
}

void TestWaylandClientThread::RunAndWait(
    base::OnceCallback<void(TestClient*)> callback) {
  base::OnceClosure closure =
      base::BindOnce(std::move(callback), base::Unretained(client_.get()));
  RunAndWait(std::move(closure));
}

void TestWaylandClientThread::RunAndWait(base::OnceClosure closure) {
  base::RunLoop run_loop;
  task_runner()->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&TestWaylandClientThread::DoRun, base::Unretained(this),
                     std::move(closure)),
      run_loop.QuitClosure());
  run_loop.Run();
}

void TestWaylandClientThread::OnFileCanReadWithoutBlocking(int fd) {
  while (wl_display_prepare_read(client_->display()) != 0)
    wl_display_dispatch_pending(client_->display());

  wl_display_read_events(client_->display());
  wl_display_dispatch_pending(client_->display());
}

void TestWaylandClientThread::OnFileCanWriteWithoutBlocking(int fd) {}

void TestWaylandClientThread::DoInit(
    TestWaylandClientThread::InitCallback init_callback) {
  client_ = std::move(init_callback).Run();
  if (!client_)
    return;

  const bool result = base::CurrentIOThread::Get().WatchFileDescriptor(
      wl_display_get_fd(client_->display()), /*persistent=*/true,
      base::MessagePumpLibevent::WATCH_READ, &controller_, this);

  if (!result)
    client_.reset();
}

void TestWaylandClientThread::DoRun(base::OnceClosure closure) {
  std::move(closure).Run();
  wl_display_roundtrip(client_->display());
}

void TestWaylandClientThread::DoCleanUp() {
  client_.reset();
}

}  // namespace exo::wayland::test
