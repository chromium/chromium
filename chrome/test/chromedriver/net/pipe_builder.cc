// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/net/pipe_builder.h"

#include <stdio.h>
#include <cstdlib>
#include <memory>
#include <string>
#include <utility>

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "build/build_config.h"
#include "chrome/test/chromedriver/chrome/status.h"
#include "chrome/test/chromedriver/net/pipe_connection.h"
#include "chrome/test/chromedriver/net/sync_websocket.h"
#include "url/gurl.h"

namespace {
#if BUILDFLAG(IS_POSIX)
// The values for kReadFD and kWriteFD come from
// content/browser/devtools/devtools_pipe_handler.cc
constexpr int kReadFD = 3;
constexpr int kWriteFD = 4;
#endif
}  // namespace
   //
const char PipeBuilder::kAsciizProtocolMode[] = "asciiz";
const char PipeBuilder::kCborProtocolMode[] = "cbor";

bool PipeBuilder::PlatformIsSupported() {
#if BUILDFLAG(IS_POSIX)
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
#if BUILDFLAG(IS_POSIX)
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
#if BUILDFLAG(IS_POSIX)
  for (base::File& file : child_ends_) {
    file.Close();
  }
  return Status{kOk};
#else
  return Status{kUnknownError, "pipes are not supported on this platform"};
#endif
}

Status PipeBuilder::SetUpPipes(base::LaunchOptions* options) {
  if (protocol_mode_ != kAsciizProtocolMode) {
    return Status{kUnknownError, "only ASCIIZ protocol mode is supported"};
  }
#if BUILDFLAG(IS_POSIX)
  base::ScopedFD chrome_to_driver_read_fd;
  base::ScopedFD chrome_to_driver_write_fd;
  base::ScopedFD driver_to_chrome_read_fd;
  base::ScopedFD driver_to_chrome_write_fd;

  if (!CreatePipe(&chrome_to_driver_read_fd, &chrome_to_driver_write_fd,
                  false) ||
      !CreatePipe(&driver_to_chrome_read_fd, &driver_to_chrome_write_fd,
                  false) ||
      !base::SetCloseOnExec(chrome_to_driver_read_fd.get()) ||
      !base::SetCloseOnExec(driver_to_chrome_write_fd.get())) {
    return Status{kUnknownError, "unable to setup a pipe"};
  }

  options->fds_to_remap.emplace_back(driver_to_chrome_read_fd.get(), kReadFD);
  options->fds_to_remap.emplace_back(chrome_to_driver_write_fd.get(), kWriteFD);
  child_ends_[0] = base::File(driver_to_chrome_read_fd.release());
  child_ends_[1] = base::File(chrome_to_driver_write_fd.release());
  read_file_ = std::move(chrome_to_driver_read_fd);
  write_file_ = std::move(driver_to_chrome_write_fd);
  return Status{kOk};
#else
  return Status{kUnknownError, "pipes are not supported on this platform"};
#endif
}
