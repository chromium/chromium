// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/tracing/common/etw_stack_sampling_win.h"

#include <windows.h>

#include <dbghelp.h>

#include <string>
#include <utility>

#include "absl/container/flat_hash_map.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "base/profiler/module_cache.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/pe_image.h"
#include "third_party/perfetto/include/perfetto/tracing/core/data_source_config.h"
#include "third_party/perfetto/include/perfetto/tracing/core/trace_config.h"
#include "third_party/perfetto/protos/perfetto/config/etw/etw_config.gen.h"

namespace tracing {
namespace {

// Images whose functions should be symbolized in ETW call stacks, and the
// directory where each is expected to be.
constexpr std::pair<const wchar_t*, int> kImagesOfInterest[] = {
    {L"chrome.exe", base::DIR_EXE},        {L"chrome.dll", base::DIR_MODULE},
    {L"dxcompiler.dll", base::DIR_MODULE}, {L"dxil.dll", base::DIR_MODULE},
    {L"libEGL.dll", base::DIR_MODULE},     {L"libGLESv2.dll", base::DIR_MODULE},
    {L"vulkan-1.dll", base::DIR_MODULE},   {L"advapi32.dll", base::DIR_SYSTEM},
    {L"apphelp.dll", base::DIR_SYSTEM},    {L"dbghelp.dll", base::DIR_SYSTEM},
    {L"kernel32.dll", base::DIR_SYSTEM},   {L"ntdll.dll", base::DIR_SYSTEM},
    {L"ntoskrnl.exe", base::DIR_SYSTEM},   {L"user32.dll", base::DIR_SYSTEM}};

// Retrieves the full path and debug ID for each image in `kImagesOfInterest`,
// from memory or from disk if needed, and returns as a [path, debug ID] map.
absl::flat_hash_map<std::string, std::string> GetDebugIdsOfInterest() {
  absl::flat_hash_map<std::string, std::string> images;
  for (const auto& [name, dir] : kImagesOfInterest) {
    std::string path;
    HMODULE module = ::GetModuleHandle(name);
    if (module) {
      // The image is loaded, so get its details from memory.
      std::wstring buffer(MAX_PATH, L'\0');
      const DWORD length = ::GetModuleFileName(module, &buffer[0], MAX_PATH);
      if (length && length < MAX_PATH) {
        buffer.resize(length);
        path = base::WideToUTF8(buffer);
        GUID guid;
        DWORD age;
        if (base::win::PEImage(module).GetDebugId(&guid, &age, nullptr,
                                                  nullptr)) {
          images[path] = base::AsBuildId(guid, age);
          continue;
        }
      }
    }
    if (path.empty()) {
      // The image is not loaded, so try using its expected location on disk.
      base::FilePath dir_path;
      if (!base::PathService::Get(dir, &dir_path)) {
        continue;
      }
      path = base::WideToUTF8(dir_path.Append(name).value());
    }
    SYMSRV_INDEX_INFO info = {};
    info.sizeofstruct = sizeof(info);
    if (::SymSrvGetFileIndexInfo(path.c_str(), &info, 0)) {
      images[path] = base::AsBuildId(info.guid, info.age);
    }
  }
  return images;
}

}  // namespace

void AddEtwStackSamplingDebugIds(perfetto::DataSourceConfig* config) {
  // Do nothing if this is a child process; only the browser should query
  // information about local files.
  if (!base::CommandLine::ForCurrentProcess()
           ->GetSwitchValueASCII("type")
           .empty()) {
    return;
  }
  perfetto::protos::gen::EtwConfig etw_config;
  if (!config->etw_config_raw().empty()) {
    etw_config.ParseFromString(config->etw_config_raw());
  }

  // Do nothing if stack sampling isn't enabled; the debug IDs won't be used.
  if (etw_config.stack_sampling_events().empty()) {
    return;
  }

  // Don't overwrite debug IDs if they're already set (e.g., in a config
  // file).
  if (!etw_config.stack_sampling_debug_ids().empty()) {
    return;
  }

  // Retrieve the debug IDs once and cache them for subsequent calls (i.e.,
  // subsequent tracing sessions) to reduce disk I/O.
  static const base::NoDestructor<absl::flat_hash_map<std::string, std::string>>
      kDebugIdsOfInterest(GetDebugIdsOfInterest());

  for (const auto& [path, debug_id] : *kDebugIdsOfInterest) {
    auto* entry = etw_config.add_stack_sampling_debug_ids();
    entry->set_path(path);
    entry->set_debug_id(debug_id);
  }
  config->set_etw_config_raw(etw_config.SerializeAsString());
}

void AddEtwStackSamplingDebugIds(perfetto::TraceConfig& perfetto_config) {
  for (auto& data_source_config : *perfetto_config.mutable_data_sources()) {
    if (data_source_config.config().name() == "org.chromium.etw_system") {
      AddEtwStackSamplingDebugIds(data_source_config.mutable_config());
    }
  }
}

}  // namespace tracing
