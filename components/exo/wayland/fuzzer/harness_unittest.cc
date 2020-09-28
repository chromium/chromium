// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/fuzzer/harness.h"

#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "components/exo/display.h"
#include "components/exo/test/exo_test_base_views.h"
#include "components/exo/wayland/fuzzer/actions.pb.h"
#include "components/exo/wayland/server.h"

namespace exo {
namespace wayland_fuzzer {
namespace {

class WaylandFuzzerTest : public test::ExoTestBaseViews {
 protected:
  WaylandFuzzerTest() = default;
  ~WaylandFuzzerTest() override = default;

  void SetUp() override {
    ASSERT_TRUE(xdg_temp_dir_.CreateUniqueTempDir());
    setenv("XDG_RUNTIME_DIR", xdg_temp_dir_.GetPath().MaybeAsASCII().c_str(),
           1 /* overwrite */);
    test::ExoTestBaseViews::SetUp();
    display_ = std::make_unique<exo::Display>();
    server_ = wayland::Server::Create(display_.get());
  }

  void TearDown() override {
    server_.reset();
    display_.reset();
    test::ExoTestBaseViews::TearDown();
  }

  base::ScopedTempDir xdg_temp_dir_;
  std::unique_ptr<exo::Display> display_;
  std::unique_ptr<wayland::Server> server_;

 private:
  DISALLOW_COPY_AND_ASSIGN(WaylandFuzzerTest);
};

void RunHarness(Harness* harness, base::WaitableEvent* event) {
  actions::actions acts;
  acts.add_acts()->mutable_act_wl_display_get_registry()->set_receiver(
      actions::small_value::ZERO);
  acts.add_acts();
  harness->Run(acts);
  event->Signal();
}

TEST_F(WaylandFuzzerTest, MakeSureItWorks) {
  Harness harness;

  // For this test we are just checking that some globals can be bound.
  EXPECT_TRUE(harness.wl_compositor_globals_.empty());
  EXPECT_TRUE(harness.wl_shm_globals_.empty());

  base::Thread client("client");
  ASSERT_TRUE(client.Start());
  base::WaitableEvent event(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                            base::WaitableEvent::InitialState::NOT_SIGNALED);
  client.task_runner()->PostTask(FROM_HERE,
                                 base::BindOnce(&RunHarness, &harness, &event));
  // For this action sequence we need two dispatches. The first will bind the
  // registry, the second is for the callback.
  server_->Dispatch(base::TimeDelta::FromSeconds(5));
  server_->Dispatch(base::TimeDelta::FromSeconds(5));
  server_->Flush();
  event.Wait();

  EXPECT_FALSE(harness.wl_compositor_globals_.empty());
  EXPECT_FALSE(harness.wl_shm_globals_.empty());
}

}  // namespace
}  // namespace wayland_fuzzer
}  // namespace exo
