// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "base/no_destructor.h"
#include "components/exo/wayland/fuzzer/actions.pb.h"
#include "components/exo/wayland/fuzzer/actions_fuzzable.pb.h"
#include "components/exo/wayland/fuzzer/harness.h"
#include "components/exo/wayland/fuzzer/server_environment.h"
#include "testing/libfuzzer/proto/lpm_interface.h"

class FuzzerEnvironment {
 public:
  FuzzerEnvironment() : server_environment_() { server_environment_.SetUp(); }

  ~FuzzerEnvironment() { server_environment_.TearDown(); }

 private:
  exo::wayland_fuzzer::ServerEnvironment server_environment_;
};

DEFINE_TEXT_PROTO_FUZZER(
    const fuzzable::exo::wayland_fuzzer::actions::actions& fuzzable_acts) {
  static base::NoDestructor<base::AtExitManager> exit_manager;
  static FuzzerEnvironment environment;

  std::string serialized = fuzzable_acts.SerializeAsString();
  exo::wayland_fuzzer::actions::actions acts;
  // Recursion limits can cause parsing to fail.
  if (!acts.ParseFromString(serialized)) {
    return;
  }

  exo::wayland_fuzzer::Harness().Run(acts);
}
