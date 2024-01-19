// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/logging.h"
#include "base/message_loop/message_pump.h"
#include "base/process/launch.h"
#include "base/run_loop.h"
#include "base/task/sequence_manager/sequence_manager.h"
#include "base/threading/thread.h"
#include "codelabs/mojo_examples/mojom/interface.mojom.h"
#include "codelabs/mojo_examples/process_bootstrapper.h"
#include "ipc/ipc_channel_mojo.h"
#include "ipc/ipc_channel_proxy.h"
#include "mojo/core/embedder/embedder.h"
#include "mojo/core/embedder/scoped_ipc_support.h"
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
  // pipe is returned for us to use locally.
  mojo::ScopedMessagePipeHandle ipc_bootstrap_pipe =
      invitation.AttachMessagePipe("ipc_bootstrap_pipe");

  base::LaunchOptions options;
  // This is the relative path to the mock "renderer process" binary. We pass it
  // into `base::LaunchProcess` to run the binary in a new process.
  static const base::CommandLine::CharType* argv[] = {
      FILE_PATH_LITERAL("./04-mojo-renderer")};
  base::CommandLine command_line(1, argv);
  channel.PrepareToPassRemoteEndpoint(&options, &command_line);
  LOG(INFO) << "Browser: " << command_line.GetCommandLineString();
  base::Process child_process = base::LaunchProcess(command_line, options);
  channel.RemoteProcessLaunchAttempted();

  mojo::OutgoingInvitation::Send(std::move(invitation), child_process.Handle(),
                                 channel.TakeLocalEndpoint());
  return ipc_bootstrap_pipe;
}

class BrowserIPCListener : public IPC::Listener {
 public:
  BrowserIPCListener(mojo::ScopedMessagePipeHandle ipc_bootstrap_pipe,
                     scoped_refptr<base::SingleThreadTaskRunner> io_task_runner)
      : IPC::Listener() {
    // See 03-channel-associated-interface-freezing/browser.cc for relevant
    // comments here.

    // 1.) Bootstrap the IPC Channel.
    std::unique_ptr<IPC::ChannelFactory> channel_factory =
        IPC::ChannelMojo::CreateServerFactory(
            std::move(ipc_bootstrap_pipe), io_task_runner,
            base::SingleThreadTaskRunner::GetCurrentDefault());
    channel_proxy_ = IPC::ChannelProxy::Create(
        std::move(channel_factory), this, /*ipc_task_runner=*/io_task_runner,
        /*listener_task_runner=*/
        base::SingleThreadTaskRunner::GetCurrentDefault());

    mojo::AssociatedRemote<codelabs::mojom::AssociatedProcess>
        associated_process_obj;
    channel_proxy_->GetRemoteAssociatedInterface(&associated_process_obj);

    mojo::Remote<codelabs::mojom::Process> process_handler;
    associated_process_obj->SetProcess(
        process_handler.BindNewPipeAndPassReceiver());

    // 2.) Bind and send an IPC to ObjectA.
    mojo::PendingAssociatedRemote<codelabs::mojom::GenericInterface>
        pending_generic;
    process_handler->GetAssociatedInterface(
        "ObjectA", pending_generic.InitWithNewEndpointAndPassReceiver());
    mojo::PendingAssociatedRemote<codelabs::mojom::ObjectA> pending_a(
        pending_generic.PassHandle(), /*version=*/0u);
    mojo::AssociatedRemote<codelabs::mojom::ObjectA> remote_a(
        std::move(pending_a));
    remote_a->DoA();

    // 3.) Bind and send an IPC to ObjectB.
    mojo::PendingAssociatedRemote<codelabs::mojom::GenericInterface>
        pending_generic_2;
    process_handler->GetAssociatedInterface(
        "ObjectB", pending_generic_2.InitWithNewEndpointAndPassReceiver());
    mojo::PendingAssociatedRemote<codelabs::mojom::ObjectB> pending_b(
        pending_generic_2.PassHandle(), /*version=*/0u);
    mojo::AssociatedRemote<codelabs::mojom::ObjectB> remote_b(
        std::move(pending_b));
    remote_b->DoB();
  }

  // IPC::Listener implementation.
  bool OnMessageReceived(const IPC::Message& msg) override {
    CHECK(false) << "The browser should not receive messages";
    return false;
  }
  void OnAssociatedInterfaceRequest(
      const std::string& interface_name,
      mojo::ScopedInterfaceEndpointHandle handle) override {
    CHECK(false)
        << "The browser should not receive associated interface requests";
  }

 private:
  std::unique_ptr<IPC::ChannelProxy> channel_proxy_;
};

int main(int argc, char** argv) {
  LOG(INFO) << "Browser process starting up";
  base::CommandLine::Init(argc, argv);

  ProcessBootstrapper bootstrapper;
  // The IO thread that the `BrowserIPCListener` ChannelProxy listens for
  // messages on *must* be different than the main thread, so in this example
  // (and in the corresponding "renderer.cc") we initialize the main thread with
  // a "DEFAULT" (i.e., non-IO-capable) main thread. This will automatically
  // give us a separate dedicated IO thread for Mojo and the IPC infrastructure
  // to use.
  bootstrapper.InitMainThread(base::MessagePumpType::DEFAULT);
  bootstrapper.InitMojo(/*as_browser_process=*/true);

  mojo::ScopedMessagePipeHandle handle = LaunchAndConnect();

  // Create a new `BrowserIPCListener` to sponsor communication coming from the
  // "browser" process. The rest of the program will execute there.
  std::unique_ptr<BrowserIPCListener> browser_ipc_listener =
      std::make_unique<BrowserIPCListener>(std::move(handle),
                                           bootstrapper.io_task_runner);

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
