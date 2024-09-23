// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/test/chromedriver/net/pipe_builder.h"

#include <cstdlib>
#include <memory>
#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/files/platform_file.h"
#include "base/strings/string_number_conversions.h"
#include "build/build_config.h"
#include "chrome/test/chromedriver/chrome/status.h"
#include "chrome/test/chromedriver/net/pipe_connection.h"
#include "chrome/test/chromedriver/net/sync_websocket.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>

#include "base/win/win_util.h"
#endif

namespace {
#if BUILDFLAG(IS_POSIX)
// The values for kReadFD and kWriteFD come from
// content/browser/devtools/devtools_pipe_handler.cc
constexpr int kReadFD = 3;
constexpr int kWriteFD = 4;
#endif
}  // namespace

const char PipeBuilder::kAsciizProtocolMode[] = "asciiz";
const char PipeBuilder::kCborProtocolMode[] = "cbor";

bool PipeBuilder::PlatformIsSupported() {
#if BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_WIN)
  return true;
#else
  return false;
#endif
}

PipeBuilder::PipeBuilder() = default;

PipeBuilder::~PipeBuilder() = default;

std::unique_ptr<SyncWebSocket> PipeBuilder::TakeSocket() {
  return std::unique_ptr<SyncWebSocket>(connection_.release());
}

void PipeBuilder::SetProtocolMode(std::string mode) {
  if (mode.empty()) {
    mode = kAsciizProtocolMode;
  }
  protocol_mode_ = mode;
}

Status PipeBuilder::BuildSocket() {
  if (protocol_mode_ != kAsciizProtocolMode) {
    return Status{kUnknownError, "only ASCIIZ protocol mode is supported"};
  }
#if BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_WIN)
  if (!read_file_.is_valid() || !write_file_.is_valid()) {
    return Status{kUnknownError, "pipes are not initialized"};
  }
  connection_ = std::make_unique<PipeConnection>(std::move(read_file_),
                                                 std::move(write_file_));
  return Status{kOk};
#else
  return Status{kUnknownError, "pipes are not supported on this platform"};
#endif
}

Status PipeBuilder::CloseChildEndpoints() {
#if BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_WIN)
  for (base::ScopedPlatformFile& file : child_ends_) {
    file = base::ScopedPlatformFile();
  }
  return Status{kOk};
#else
  return Status{kUnknownError, "pipes are not supported on this platform"};
#endif
}

Status PipeBuilder::SetUpPipes(base::LaunchOptions* options,
                               base::CommandLine* command) {
  if (protocol_mode_ != kAsciizProtocolMode) {
    return Status{kUnknownError, "only ASCIIZ protocol mode is supported"};
  }
#if BUILDFLAG(IS_POSIX)
  base::ScopedFD parent_read;
  base::ScopedFD child_write;
  base::ScopedFD child_read;
  base::ScopedFD parent_write;

  if (!CreatePipe(&parent_read, &child_write, false) ||
      !CreatePipe(&child_read, &parent_write, false) ||
      // the local ends must be closed in the child process
      !base::SetCloseOnExec(parent_read.get()) ||
      !base::SetCloseOnExec(parent_write.get())) {
    return Status{kUnknownError, "unable to setup a pipe"};
  }

  options->fds_to_remap.emplace_back(child_read.get(), kReadFD);
  options->fds_to_remap.emplace_back(child_write.get(), kWriteFD);

  read_file_ = std::move(parent_read);
  write_file_ = std::move(parent_write);
  child_ends_[0] = std::move(child_read);
  child_ends_[1] = std::move(child_write);

  return Status{kOk};
#elif BUILDFLAG(IS_WIN)
  HANDLE child_read_handle;
  HANDLE parent_write_handle;
  HANDLE parent_read_handle;
  HANDLE child_write_handle;
  if (!CreatePipe(&child_read_handle, &parent_write_handle, nullptr, 0)) {
    return Status{kUnknownError, "unable to setup a pipe"};
  }
  base::win::ScopedHandle child_read(child_read_handle);
  base::win::ScopedHandle parent_write(parent_write_handle);
  if (!CreatePipe(&parent_read_handle, &child_write_handle, nullptr, 0)) {
    return Status{kUnknownError, "unable to setup a pipe"};
  }
  base::win::ScopedHandle parent_read(parent_read_handle);
  base::win::ScopedHandle child_write(child_write_handle);

  std::string in_pipe_name =
      base::NumberToString(base::win::HandleToUint32(child_read.get()));
  std::string out_pipe_name =
      base::NumberToString(base::win::HandleToUint32(child_write.get()));
  // We use the fact that inherited handles in the child process have the same
  // value and access rights as in the parent process.
  // See:
  // https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-createprocessa
  command->AppendSwitchASCII("remote-debugging-io-pipes",
                             in_pipe_name + "," + out_pipe_name);
  options->handles_to_inherit.push_back(child_read.get());
  options->handles_to_inherit.push_back(child_write.get());

  read_file_ = std::move(parent_read);
  write_file_ = std::move(parent_write);
  child_ends_[0] = std::move(child_read);
  child_ends_[1] = std::move(child_write);

  return Status{kOk};
#else
  return Status{kUnknownError, "pipes are not supported on this platform"};
#endif
}
