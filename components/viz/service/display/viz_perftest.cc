// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/viz_perftest.h"

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "components/viz/common/quads/render_pass_io.h"
#include "components/viz/test/paths.h"
#include "third_party/zlib/google/zip.h"

namespace viz {

namespace {

// Creates a path to the JSON file in the test data folder with `group` and
// `name` as parent folders, and `frame_index` padded to 4 digits will be the
// filename.
// e.g. <test_data_folder>/render_pass_data/group/name/0001.json
std::optional<base::FilePath> MakeTestDataJsonPath(const std::string& group,
                                                   const std::string& name,
                                                   size_t frame_index) {
  base::FilePath test_data_path;
  if (!base::PathService::Get(Paths::DIR_TEST_DATA, &test_data_path)) {
    return std::nullopt;
  }
  return test_data_path.AppendASCII("render_pass_data")
      .AppendASCII(group)
      .AppendASCII(name)
      .AppendASCII(
          base::StringPrintf("%04d.json", static_cast<int>(frame_index)));
}

std::optional<base::Value> ReadValueFromJson(base::FilePath& json_path) {
  if (!base::PathExists(json_path))
    return std::nullopt;
  std::string json_text;
  if (!base::ReadFileToString(json_path, &json_text))
    return std::nullopt;
  return base::JSONReader::Read(json_text);
}

}  // namespace

bool CompositorRenderPassListFromJSON(
    const std::string& tag,
    const std::string& site,
    uint32_t year,
    size_t frame_index,
    CompositorRenderPassList* render_pass_list) {
  std::string name = site + "_" + base::NumberToString(year);
  std::optional<base::FilePath> json_path =
      MakeTestDataJsonPath(tag, name, frame_index);
  if (!json_path) {
    return false;
  }
  auto dict = ReadValueFromJson(*json_path);
  if (!dict || !dict->is_dict()) {
    return false;
  }
  return CompositorRenderPassListFromDict(dict->GetDict(), render_pass_list);
}

std::optional<base::FilePath> UnzipFrameData(const std::string& group,
                                             const std::string& name) {
  base::FilePath zip_path;
  if (!base::PathService::Get(Paths::DIR_TEST_DATA, &zip_path)) {
    return std::nullopt;
  }
  zip_path = zip_path.AppendASCII("render_pass_data")
                 .AppendASCII(group)
                 .AppendASCII(name)
                 .AppendASCII(name + ".zip");

  base::FilePath out_path;
  if (!base::GetTempDir(&out_path)) {
    return std::nullopt;
  }
  out_path = out_path.AppendASCII("render_pass_data")
                 .AppendASCII(group)
                 .AppendASCII(name);

  // We are dumping the contents of the zip into a folder so we need to clean
  // out this folder.
  base::DeletePathRecursively(out_path);
  if (!zip::Unzip(zip_path, out_path)) {
    LOG(ERROR) << "Failed to unzip frame data from: " << zip_path;
    return std::nullopt;
  }

  return out_path;
}

bool FrameDataFromJson(base::FilePath& json_path,
                       std::vector<FrameData>* frame_data_list) {
  auto list = ReadValueFromJson(json_path);
  if (!list || !list->is_list()) {
    return false;
  }
  return FrameDataFromList(list->GetList(), frame_data_list);
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
      return base::Milliseconds(delay_millis);
    }
  }
  return base::Seconds(3);
}

}  // namespace

VizPerfTest::VizPerfTest()
    : timer_(/*warmup_laps=*/100,
             /*time_limit=*/TestTimeLimit(),
             /*check_interval=*/10) {}

}  // namespace viz
