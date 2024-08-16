// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <mach-o/getsect.h>
#include <mach-o/ldsyms.h>
#include <mach-o/loader.h>

#include <cstdint>
#include <memory>
#include <string>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/process/launch.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/util/util.h"
#include "third_party/zlib/google/zip_reader.h"

namespace updater {

namespace {

int Unzip(const std::string& archive, const base::FilePath& dest) {
  zip::ZipReader reader;
  if (!reader.OpenFromString(archive)) {
    VLOG(0) << "Error opening updater resource.";
    return kErrorUnpackingResource;
  }

  while (const zip::ZipReader::Entry* const entry = reader.Next()) {
    if (entry->is_directory) {
      if (!base::CreateDirectory(dest.Append(entry->path))) {
        VLOG(0) << "Failed to mkdir " << entry->path;
        return kErrorUnpackingResource;
      }
      continue;
    }

    auto writer =
        std::make_unique<zip::FilePathWriterDelegate>(dest.Append(entry->path));
    if (!writer || !reader.ExtractCurrentEntry(writer.get())) {
      VLOG(0) << "Cannot extract " << entry->path;
      return kErrorUnpackingResource;
    }
  }

  if (!reader.ok()) {
    return kErrorUnpackingResource;
  }

  return kErrorOk;
}

int Run(const base::FilePath& extract_path) {
  base::CommandLine command_line(*base::CommandLine::ForCurrentProcess());
  command_line.SetProgram(extract_path.Append(GetExecutableRelativePath()));
  int exit_code = 0;
  std::string out;
  base::GetAppOutputWithExitCode(command_line, &out, &exit_code);
  VLOG(1) << out;
  return exit_code;
}

int Main() {
  unsigned long size = 0;
  uint8_t* data =
      getsectiondata(&_mh_execute_header, SEG_DATA, "__updater_zip", &size);
  VLOG(1) << "The zip resource is " << size << " bytes long.";

  base::ScopedTempDir extract_path;
  if (!extract_path.CreateUniqueTempDir()) {
    VLOG(0) << "Failed to create temp dir.";
    return kErrorCreatingTempDir;
  }

  int result = Unzip({data, data + size}, extract_path.GetPath());
  if (result != kErrorOk) {
    return result;
  }

  return Run(extract_path.GetPath());
}

}  // namespace
}  // namespace updater

int main(int argc, const char* argv[]) {
  base::CommandLine::Init(argc, argv);
  return updater::Main();
}
