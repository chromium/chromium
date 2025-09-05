// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/accessibility/ax_client/launcher.h"

#include <windows.h>

#include <utility>

#include "base/base_paths.h"
#include "base/base_switches.h"
#include "base/check.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/test/accessibility/ax_client/ax_client.test-mojom.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "mojo/public/cpp/system/invitation.h"

namespace ax_client {

Launcher::Launcher() = default;

Launcher::~Launcher() {
  if (process_.IsValid()) {
    process_.Terminate(1, /*wait=*/false);
  }
}

mojo::PendingRemote<mojom::AxClient> Launcher::Launch(
    ClientApi client_api,
    base::RepeatingCallback<void(const std::string&)> on_process_error) {
  CHECK(!process_.IsValid());

  mojo::PlatformChannel channel;
  mojo::OutgoingInvitation invitation;
  mojo::ScopedMessagePipeHandle pipe = invitation.AttachMessagePipe(/*name=*/0);

  base::LaunchOptions launch_options;
  launch_options.start_hidden = true;
  launch_options.feedback_cursor_off = true;

  // Share the calling process's stdout and stderr with ax_client.
  CHECK(!logging::DuplicateLogFileHandle());  // TODO: Support log-to-file.
  const HANDLE std_out = ::GetStdHandle(STD_OUTPUT_HANDLE);
  const HANDLE std_err = ::GetStdHandle(STD_ERROR_HANDLE);
  if (std_out != INVALID_HANDLE_VALUE) {
    launch_options.stdout_handle = std_out;
    launch_options.handles_to_inherit.push_back(std_out);
  }
  if (std_err != INVALID_HANDLE_VALUE) {
    launch_options.stderr_handle = std_err;
    if (std_err != std_out) {
      launch_options.handles_to_inherit.push_back(std_err);
    }
  }

  base::CommandLine command_line(
      base::PathService::CheckedGet(base::DIR_EXE)
          .Append(FILE_PATH_LITERAL("ax_client.exe")));

  // Send the calling process's logging options to ax_client.
  command_line.CopySwitchesFrom(*base::CommandLine::ForCurrentProcess(),
                                {switches::kV, switches::kVModule});

  // Tell the client which accessibility API to use.
  command_line.AppendSwitchNative(
      "client-api", base::NumberToWString(static_cast<int>(client_api)));

  channel.PrepareToPassRemoteEndpoint(&launch_options, &command_line);

  process_ = base::LaunchProcess(command_line, launch_options);
  if (!process_.IsValid()) {
    return {};
  }

  on_process_error_ = std::move(on_process_error);

  mojo::OutgoingInvitation::Send(
      std::move(invitation), process_.Handle(), channel.TakeLocalEndpoint(),
      base::BindRepeating(&Launcher::OnProcessError,
                          weak_ptr_factory_.GetWeakPtr()));
  return {std::move(pipe), 0};
}

void Launcher::OnProcessError(const std::string& error) {
  if (on_process_error_) {
    on_process_error_.Run(error);
  }
}

}  // namespace ax_client
