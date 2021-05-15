// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/viz_perf_test.h"

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "components/viz/common/quads/render_pass_io.h"
#include "components/viz/test/paths.h"

namespace viz {

bool CompositorRenderPassListFromJSON(
    const std::string& tag,
    const std::string& site,
    uint32_t year,
    size_t frame_index,
    CompositorRenderPassList* render_pass_list) {
  base::FilePath json_path;
  if (!base::PathService::Get(Paths::DIR_TEST_DATA, &json_path))
    return false;
  std::string site_year = site + "_" + base::NumberToString(year);
  std::string filename = base::NumberToString(frame_index);
  while (filename.length() < 4)
    filename = "0" + filename;
  filename += ".json";
  json_path = json_path.Append(FILE_PATH_LITERAL("render_pass_data"))
                  .AppendASCII(tag)
                  .AppendASCII(site_year)
                  .AppendASCII(filename);
  if (!base::PathExists(json_path))
    return false;
  std::string json_text;
  if (!base::ReadFileToString(json_path, &json_text))
    return false;
  absl::optional<base::Value> dict = base::JSONReader::Read(json_text);
  if (!dict.has_value())
    return false;
  return CompositorRenderPassListFromDict(dict.value(), render_pass_list);
}

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
