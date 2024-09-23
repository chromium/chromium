// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/fuzzer/harness.h"

#include "base/files/scoped_temp_dir.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "components/exo/display.h"
#include "components/exo/test/exo_test_base.h"
#include "components/exo/test/test_security_delegate.h"
#include "components/exo/wayland/fuzzer/actions.pb.h"
#include "components/exo/wayland/server.h"

namespace exo {
namespace wayland_fuzzer {
namespace {

// Use ExoTestBase because Server starts to depends on ash::Shell.
using TestBase = test::ExoTestBase;

class WaylandFuzzerTest : public TestBase {
 protected:
  WaylandFuzzerTest() = default;
  WaylandFuzzerTest(const WaylandFuzzerTest&) = delete;
  WaylandFuzzerTest& operator=(const WaylandFuzzerTest&) = delete;
  ~WaylandFuzzerTest() override = default;

  void SetUp() override {
    ASSERT_TRUE(xdg_temp_dir_.CreateUniqueTempDir());
    setenv("XDG_RUNTIME_DIR", xdg_temp_dir_.GetPath().MaybeAsASCII().c_str(),
           1 /* overwrite */);
    TestBase::SetUp();
    display_ = std::make_unique<exo::Display>();
    server_ = wayland::Server::Create(
        display_.get(), std::make_unique<test::TestSecurityDelegate>());
    server_->StartWithDefaultPath(base::DoNothing());
  }

  void TearDown() override {
    server_.reset();
    display_.reset();
    TestBase::TearDown();
  }

  base::ScopedTempDir xdg_temp_dir_;
  std::unique_ptr<exo::Display> display_;
  std::unique_ptr<wayland::Server> server_;
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
  server_->Dispatch(base::Seconds(5));
  server_->Dispatch(base::Seconds(5));
  server_->Flush();
  event.Wait();

  EXPECT_FALSE(harness.wl_compositor_globals_.empty());
  EXPECT_FALSE(harness.wl_shm_globals_.empty());
}

}  // namespace
}  // namespace wayland_fuzzer
}  // namespace exo
