// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/viz_perf_test.h"

#include "base/command_line.h"
#include "base/strings/string_number_conversions.h"

namespace viz {
namespace {

constexpr char kPerfTestTimeMillis[] = "perf-test-time-ms";

base::TimeDelta TestTimeLimit() {
  auto* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(kPerfTestTimeMillis)) {
    const std::string delay_millis_string(
        command_line->GetSwitchValueASCII(kPerfTestTimeMillis));
    int delay_millis;
    if (base::StringToInt(delay_millis_string, &delay_millis) &&
        delay_millis > 0) {
      return base::TimeDelta::FromMilliseconds(delay_millis);
    }
  }
  return base::TimeDelta::FromSeconds(3);
}

}  // namespace

VizPerfTest::VizPerfTest()
    : timer_(/*warmup_laps=*/100,
             /*time_limit=*/TestTimeLimit(),
             /*check_interval=*/10) {}

}  // namespace viz
