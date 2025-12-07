// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "codelabs/mojo_examples/mojom/interface.mojom.h"
#include "codelabs/mojo_examples/process_bootstrapper.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "mojo/public/cpp/system/invitation.h"
#include "mojo/public/cpp/system/message_pipe.h"

class ProcessImpl : public codelabs::mojom::Process {
 public:
  explicit ProcessImpl(
      mojo::PendingReceiver<codelabs::mojom::Process> pending_receiver) {
    receiver_.Bind(std::move(pending_receiver));
  }

 private:
  // codelabs::mojo::Process
  void GetAssociatedInterface(
      const std::string&,
      mojo::PendingAssociatedReceiver<codelabs::mojom::GenericInterface>)
      override {
    NOTREACHED();
  }
  void SayHello() override { LOG(INFO) << "Hello!"; }

  mojo::Receiver<codelabs::mojom::Process> receiver_{this};
};
ProcessImpl* g_process_impl = nullptr;

void BindProcessImpl(mojo::ScopedMessagePipeHandle pipe) {
  // Create a receiver
  mojo::PendingReceiver<codelabs::mojom::Process> pending_receiver(
      std::move(pipe));
  g_process_impl = new ProcessImpl(std::move(pending_receiver));
}

int main(int argc, char** argv) {
  base::CommandLine::Init(argc, argv);
  // Logging the entire command line shows all of the information that the
  // browser seeded the renderer with. Notably, this includes the mojo platform
  // channel handle that the renderer will use to bootstrap its primordial
  // connection with the browser process.
  LOG(INFO) << "Renderer: "
            << base::CommandLine::ForCurrentProcess()->GetCommandLineString();

  ProcessBootstrapper bootstrapper;
  bootstrapper.InitMainThread(base::MessagePumpType::IO);
  bootstrapper.InitMojo(/*as_browser_process=*/false);

  // Accept an invitation.
  //
  // `RecoverPassedEndpointFromCommandLine()` is what makes use of the mojo
  // platform channel handle that gets printed in the above `LOG()`; this is the
  // file descriptor of the first connection that this process shares with the
  // browser.
  mojo::IncomingInvitation invitation = mojo::IncomingInvitation::Accept(
      mojo::PlatformChannel::RecoverPassedEndpointFromCommandLine(
          *base::CommandLine::ForCurrentProcess()));
  // Extract one end of the first pipe by the name that the browser process
  // added this pipe to the invitation by.
  mojo::ScopedMessagePipeHandle pipe = invitation.ExtractMessagePipe("pipe");

  base::RunLoop run_loop;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&BindProcessImpl, std::move(pipe)));
  run_loop.Run();
  return 0;
}
