// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_VIZ_PERFTEST_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_VIZ_PERFTEST_H_

#include <string>
#include <vector>

#include "base/timer/lap_timer.h"
#include "components/viz/common/quads/compositor_render_pass.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
class FilePath;
}

namespace viz {

struct FrameData;

// Reads the specified JSON file and parses a CompositorRenderPassList from it,
// storing the result in |render_pass_list|.
bool CompositorRenderPassListFromJSON(
    const std::string& tag,
    const std::string& site,
    uint32_t year,
    size_t frame_index,
    CompositorRenderPassList* render_pass_list);

// Unzips the frame data JSON files to a temp directory so they can be read.
std::optional<base::FilePath> UnzipFrameData(const std::string& group,
                                             const std::string& name);

// Reads the specified JSON file and stores the compositor frame data in the
// output parameter `frame_data_list`.
bool FrameDataFromJson(base::FilePath& json_path,
                       std::vector<FrameData>* frame_data_list);

// Viz perf test base class that sets up a lap timer with a specified
// duration.
class VizPerfTest : public testing::Test {
 public:
  VizPerfTest();

 protected:
  // Duration is set by the flag --perf-test-time-ms, defaults to 3 seconds.
  base::LapTimer timer_;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_VIZ_PERFTEST_H_
