// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/logging.h"
#include "base/process/launch.h"
#include "base/run_loop.h"
#include "codelabs/mojo_examples/mojom/interface.mojom.h"
#include "codelabs/mojo_examples/process_bootstrapper.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "mojo/public/cpp/system/invitation.h"
#include "mojo/public/cpp/system/message_pipe.h"

mojo::ScopedMessagePipeHandle LaunchAndConnect() {
  // Under the hood, this is essentially always an OS pipe (domain socket pair,
  // Windows named pipe, Fuchsia channel, etc).
  mojo::PlatformChannel channel;

  mojo::OutgoingInvitation invitation;

  // Attach a message pipe to be extracted by the receiver. The other end of the
  // pipe is returned for us to use locally. We choose the arbitrary name "pipe"
  // here, which is the same name that the receiver will have to use when
  // plucking this pipe off of the invitation it receives to join our process
  // network.
  mojo::ScopedMessagePipeHandle pipe = invitation.AttachMessagePipe("pipe");

  base::LaunchOptions options;
  // This is the relative path to the mock "renderer process" binary. We pass it
  // into `base::LaunchProcess` to run the binary in a new process.
  static const base::CommandLine::CharType* argv[] = {
      FILE_PATH_LITERAL("./01-mojo-renderer")};
  base::CommandLine command_line(1, argv);

  // Delegating to Mojo to "prepare" the command line will append the
  // `--mojo-platform-channel-handle=N` command line argument, so that the
  // renderer knows which file descriptor name to recover, in order to establish
  // the primordial connection with this process. We log the full command line
  // next, to show what mojo information the renderer will be initiated with.
  channel.PrepareToPassRemoteEndpoint(&options, &command_line);
  LOG(INFO) << "Browser: " << command_line.GetCommandLineString();
  base::Process child_process = base::LaunchProcess(command_line, options);
  channel.RemoteProcessLaunchAttempted();

  mojo::OutgoingInvitation::Send(std::move(invitation), child_process.Handle(),
                                 channel.TakeLocalEndpoint());
  return pipe;
}

void CreateProcessRemote(mojo::ScopedMessagePipeHandle pipe) {
  mojo::PendingRemote<codelabs::mojom::Process> pending_remote(std::move(pipe),
                                                               0u);
  mojo::Remote<codelabs::mojom::Process> remote(std::move(pending_remote));
  LOG(INFO) << "Browser invoking SayHello() on remote pointing to renderer";
  remote->SayHello();
}

int main(int argc, char** argv) {
  LOG(INFO) << "'Browser process' starting up";
  base::CommandLine::Init(argc, argv);
  ProcessBootstrapper bootstrapper;
  bootstrapper.InitMainThread(base::MessagePumpType::IO);
  bootstrapper.InitMojo(/*as_browser_process=*/true);

  mojo::ScopedMessagePipeHandle pipe = LaunchAndConnect();
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&CreateProcessRemote, std::move(pipe)));

  base::RunLoop run_loop;
  // Delay shutdown of the browser process for visual effects, as well as to
  // ensure the browser process doesn't die while the IPC message is still being
  // sent to the target process asynchronously, which would prevent its
  // delivery.
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          [](base::OnceClosure quit_closure) {
            LOG(INFO) << "'Browser process' shutting down";
            std::move(quit_closure).Run();
          },
          run_loop.QuitClosure()),
      base::Seconds(2));
  run_loop.Run();
  return 0;
}
