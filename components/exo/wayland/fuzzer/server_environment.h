// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_WAYLAND_FUZZER_SERVER_ENVIRONMENT_H_
#define COMPONENTS_EXO_WAYLAND_FUZZER_SERVER_ENVIRONMENT_H_

#include "base/test/task_environment.h"
#include "base/threading/thread.h"
#include "components/exo/wayland/clients/test/wayland_client_test_helper.h"
#include "ui/aura/env.h"

namespace exo {
namespace wayland_fuzzer {

// The wayland fuzzer is pretending to be a client, and the ServerEnvironment is
// used to bring up that client's server. This sets up the major components of
// the test environment, including initializing the display, running the server
// in a thread, etc.
//
// For performance reasons, the server should be retained between runs of the
// fuzzer, though this has the unfortunate consequence that fuzzer runs retain
// state which may cause non-reproducible crashes.
class ServerEnvironment : public WaylandClientTestHelper {
 public:
  ServerEnvironment();

  ~ServerEnvironment() override;

  void SetUpOnUIThread(base::WaitableEvent* event) override;

 private:
  std::unique_ptr<aura::Env> env_;
  base::test::TaskEnvironment task_environment_;
  base::Thread ui_thread_;
};

}  // namespace wayland_fuzzer
}  // namespace exo

#endif  // COMPONENTS_EXO_WAYLAND_FUZZER_SERVER_ENVIRONMENT_H_
