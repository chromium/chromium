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
  // Windows named pipe, mach port, Fuchsia channel, etc).
  mojo::PlatformChannel channel;

  mojo::OutgoingInvitation invitation;

  // Attach a message pipe to be extracted by the receiver. The other end of the
  // pipe is returned for us to use locally.
  mojo::ScopedMessagePipeHandle pipe = invitation.AttachMessagePipe("pipe");

  base::LaunchOptions options;
  // This is the relative path to the mock "renderer process" binary. We pass it
  // into `base::LaunchProcess` to run the binary in a new process.
  static const base::CommandLine::CharType* argv[] = {
      FILE_PATH_LITERAL("./02-mojo-renderer")};
  base::CommandLine command_line(1, argv);
  channel.PrepareToPassRemoteEndpoint(&options, &command_line);
  LOG(INFO) << "Browser: " << command_line.GetCommandLineString();
  base::Process child_process = base::LaunchProcess(command_line, options);
  channel.RemoteProcessLaunchAttempted();

  mojo::OutgoingInvitation::Send(std::move(invitation), child_process.Handle(),
                                 channel.TakeLocalEndpoint());
  return pipe;
}

void CreateProcessRemote(mojo::ScopedMessagePipeHandle pipe) {
  // An unassociated remote to the toy "renderer" process. We us this to bind
  // two associated interface requests.
  mojo::PendingRemote<codelabs::mojom::Process> pending_remote(std::move(pipe),
                                                               /*version=*/0u);
  mojo::Remote<codelabs::mojom::Process> remote(std::move(pending_remote));
  remote->SayHello();

  // Make an associated interface request for ObjectA and send an IPC.
  mojo::PendingAssociatedRemote<codelabs::mojom::GenericInterface>
      pending_generic;
  remote->GetAssociatedInterface(
      "ObjectA", pending_generic.InitWithNewEndpointAndPassReceiver());
  mojo::PendingAssociatedRemote<codelabs::mojom::ObjectA> pending_a(
      pending_generic.PassHandle(), /*version=*/0u);
  mojo::AssociatedRemote<codelabs::mojom::ObjectA> remote_a(
      std::move(pending_a));
  LOG(INFO) << "Calling ObjectA::DoA() from the browser";
  remote_a->DoA();

  // Do the same for ObjectB.
  mojo::PendingAssociatedRemote<codelabs::mojom::GenericInterface>
      pending_generic_2;
  remote->GetAssociatedInterface(
      "ObjectB", pending_generic_2.InitWithNewEndpointAndPassReceiver());
  mojo::PendingAssociatedRemote<codelabs::mojom::ObjectB> pending_b(
      pending_generic_2.PassHandle(), /*version=*/0u);
  mojo::AssociatedRemote<codelabs::mojom::ObjectB> remote_b(
      std::move(pending_b));
  LOG(INFO) << "Calling ObjectB::DoB() from the browser";
  remote_b->DoB();
}

int main(int argc, char** argv) {
  LOG(INFO) << "Browser process starting up";
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
  // delivery. This delay is an arbitrary 5 seconds, which just needs to be
  // longer than the renderer's 3 seconds, which is used to show visually via
  // logging, how the ordering of IPCs can be effected by a frozen task queue
  // that gets unfrozen 3 seconds later.
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          [](base::OnceClosure quit_closure) {
            LOG(INFO) << "'Browser process' shutting down";
            std::move(quit_closure).Run();
          },
          run_loop.QuitClosure()),
      base::Seconds(5));
  run_loop.Run();
  return 0;
}
