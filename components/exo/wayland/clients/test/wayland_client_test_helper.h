// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_WAYLAND_CLIENTS_TEST_WAYLAND_CLIENT_TEST_HELPER_H_
#define COMPONENTS_EXO_WAYLAND_CLIENTS_TEST_WAYLAND_CLIENT_TEST_HELPER_H_

#include "base/memory/scoped_refptr.h"
#include "base/task/single_thread_task_runner.h"

namespace ash {
class AshTestHelper;
}

namespace base {
class ScopedTempDir;
class WaitableEvent;
}  // namespace base

namespace exo {
namespace wayland {
class Server;
}

class Display;
class WMHelper;

class WaylandClientTestHelper {
 public:
  WaylandClientTestHelper();

  WaylandClientTestHelper(const WaylandClientTestHelper&) = delete;
  WaylandClientTestHelper& operator=(const WaylandClientTestHelper&) = delete;

  virtual ~WaylandClientTestHelper();

  static void SetUIThreadTaskRunner(
      scoped_refptr<base::SingleThreadTaskRunner> ui_thread_task_runner);

  void SetUp();
  void TearDown();

  wayland::Server* wayland_server() { return wayland_server_.get(); }

 protected:
  virtual void SetUpOnUIThread(base::WaitableEvent* event);

 private:
  void TearDownOnUIThread(base::WaitableEvent* event);

  // Below objects can only be accessed from UI thread.
  std::unique_ptr<base::ScopedTempDir> xdg_temp_dir_;
  std::unique_ptr<ash::AshTestHelper> ash_test_helper_;
  std::unique_ptr<WMHelper> wm_helper_;
  std::unique_ptr<Display> display_;
  std::unique_ptr<wayland::Server> wayland_server_;
};

}  // namespace exo

#endif  // COMPONENTS_EXO_WAYLAND_CLIENTS_TEST_WAYLAND_CLIENT_TEST_HELPER_H_
