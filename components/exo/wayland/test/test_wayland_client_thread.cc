// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/test/test_wayland_client_thread.h"

#include <utility>

#include <poll.h>

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
  // Guarantee that no new events will come during or after the destruction of
  // the display.
  stopped_ = true;

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
  // TODO(crbug.com/40260645): Use busy loop to workaround RunLoop::Run()
  // erroneously advancing mock time.
  while (!run_loop.AnyQuitCalled()) {
    run_loop.RunUntilIdle();
  }
}

void TestWaylandClientThread::OnFileCanReadWithoutBlocking(int fd) {
  if (stopped_) {
    return;
  }

  if (wl_display_prepare_read(client_->display()) != 0) {
    return;
  }
  // Disconnect can be seen as a read event, and wl_display_prepare_read_queue()
  // is used to prevent read from other thread and does not actually check the
  // `fd`'s state.  Make sure that `fd` has indeed has data to read.
  struct pollfd fds;
  fds.fd = fd;
  fds.events = POLLIN;
  fds.revents = 0;

  auto ret = poll(&fds, 1, -1);

  if (ret != POLLIN) {
    wl_display_cancel_read(client_->display());
    return;
  }

  wl_display_read_events(client_->display());
  wl_display_dispatch_pending(client_->display());
  wl_display_flush(client_->display());
}

void TestWaylandClientThread::OnFileCanWriteWithoutBlocking(int fd) {}

void TestWaylandClientThread::DoInit(
    TestWaylandClientThread::InitCallback init_callback) {
  client_ = std::move(init_callback).Run();
  if (!client_)
    return;

  const bool result = base::CurrentIOThread::Get().WatchFileDescriptor(
      wl_display_get_fd(client_->display()), /*persistent=*/true,
      base::MessagePumpEpoll::WATCH_READ, &controller_, this);

  if (!result)
    client_.reset();
}

void TestWaylandClientThread::DoRun(base::OnceClosure closure) {
  std::move(closure).Run();
  wl_display_flush(client_->display());
  wl_display_roundtrip(client_->display());
}

void TestWaylandClientThread::DoCleanUp() {
  controller_.StopWatchingFileDescriptor();
  client_.reset();
}

}  // namespace exo::wayland::test
