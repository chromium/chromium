// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/task/sequence_manager/sequence_manager.h"
#include "base/task/sequence_manager/task_queue.h"
#include "codelabs/mojo_examples/mojo_impls.h"
#include "codelabs/mojo_examples/mojom/interface.mojom.h"
#include "codelabs/mojo_examples/process_bootstrapper.h"
#include "ipc/ipc_channel_mojo.h"
#include "ipc/ipc_channel_proxy.h"
#include "ipc/ipc_sync_channel.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "mojo/public/cpp/system/invitation.h"
#include "mojo/public/cpp/system/message_pipe.h"

static ObjectAImpl g_object_a;
static ObjectBImpl g_object_b;

class CustomTaskQueue : public base::RefCounted<CustomTaskQueue> {
 public:
  CustomTaskQueue(base::sequence_manager::SequenceManager& sequence_manager,
                  const base::sequence_manager::TaskQueue::Spec& spec)
      : task_queue_(sequence_manager.CreateTaskQueue(spec)),
        voter_(task_queue_->CreateQueueEnabledVoter()) {}
  void FreezeTaskQueue() { voter_->SetVoteToEnable(false); }

  void UnfreezeTaskQueue() {
    LOG(INFO) << "Unfreezing the task queue that `ObjectAImpl` is bound to.";
    voter_->SetVoteToEnable(true);
  }

  const scoped_refptr<base::SingleThreadTaskRunner>& task_runner() const {
    return task_queue_->task_runner();
  }

 private:
  ~CustomTaskQueue() = default;
  friend class base::RefCounted<CustomTaskQueue>;

  base::sequence_manager::TaskQueue::Handle task_queue_;
  // Used to enable/disable the underlying `TaskQueueImpl`.
  std::unique_ptr<base::sequence_manager::TaskQueue::QueueEnabledVoter> voter_;
};

class RendererIPCListener : public IPC::Listener {
 public:
  RendererIPCListener(
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> initially_frozen_task_runner)
      : initially_frozen_task_runner_(initially_frozen_task_runner) {
    // The sequence of events we'll need to perform are the following:
    //   1.) Create the ChannelProxy (specifically a SyncChannel) for the
    //       receiving end of the IPC communication.
    //   2.) Accept the incoming mojo invitation. From the invitation, we
    //       extract a message pipe that we will feed directly into the
    //       `IPC::ChannelProxy` to initialize it. This bootstraps the
    //       bidirectional IPC channel between browser <=> renderer.

    // 1.) Create a new IPC::ChannelProxy.
    channel_proxy_ = IPC::SyncChannel::Create(
        this, io_task_runner, base::SingleThreadTaskRunner::GetCurrentDefault(),
        &shutdown_event_);

    // 2.) Accept the mojo invitation.
    mojo::IncomingInvitation invitation = mojo::IncomingInvitation::Accept(
        mojo::PlatformChannel::RecoverPassedEndpointFromCommandLine(
            *base::CommandLine::ForCurrentProcess()));
    mojo::ScopedMessagePipeHandle ipc_bootstrap_pipe =
        invitation.ExtractMessagePipe("ipc_bootstrap_pipe");

    // Get ready to receive the invitation from the browser process, which bears
    // a message pipe represented by `ipc_bootstrap_pipe`.
    channel_proxy_->Init(
        IPC::ChannelMojo::CreateClientFactory(
            std::move(ipc_bootstrap_pipe), /*ipc_task_runner=*/io_task_runner,
            /*proxy_task_runner=*/
            base::SingleThreadTaskRunner::GetCurrentDefault()),
        /*create_pipe_now=*/true);
  }

 private:
  // IPC::Listener implementation.
  bool OnMessageReceived(const IPC::Message& msg) override {
    LOG(WARNING) << "The renderer received a message";
    return true;
  }
  void OnAssociatedInterfaceRequest(
      const std::string& interface_name,
      mojo::ScopedInterfaceEndpointHandle handle) override {
    std::string tmp_name = interface_name;
    LOG(WARNING) << "The renderer received an associated interface request for "
                 << tmp_name.c_str();
    if (interface_name == "codelabs.mojom.ObjectA") {
      // Amazingly enough, if you comment out all of this code, which causes the
      // `ObjectA` interface to not get bound and therefore the `DoA()` message
      // to never be delivered, the `DoB()` message still gets delivered and
      // invoked on `ObjectB`. This is because channel-associated interface
      // messages are dispatched very differently than non-channel-associated
      // ones, because we can't block at all.
      mojo::PendingAssociatedReceiver<codelabs::mojom::ObjectA> pending_a(
          std::move(handle));
      g_object_a.BindToFrozenTaskRunner(
          std::move(pending_a), std::move(initially_frozen_task_runner_));
    } else if (interface_name == "codelabs.mojom.ObjectB") {
      mojo::PendingAssociatedReceiver<codelabs::mojom::ObjectB> pending_b(
          std::move(handle));
      g_object_b.Bind(std::move(pending_b));
    }
  }

  std::unique_ptr<IPC::SyncChannel> channel_proxy_;
  scoped_refptr<base::SingleThreadTaskRunner> initially_frozen_tq_;
  base::WaitableEvent shutdown_event_{
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED};

  scoped_refptr<base::SingleThreadTaskRunner> initially_frozen_task_runner_;
};

int main(int argc, char** argv) {
  base::AtExitManager exit_manager;
  base::CommandLine::Init(argc, argv);
  LOG(INFO) << "Renderer: "
            << base::CommandLine::ForCurrentProcess()->GetCommandLineString();

  ProcessBootstrapper bootstrapper;
  // See the documentation above the corresponding "browser.cc".
  bootstrapper.InitMainThread(base::MessagePumpType::DEFAULT);
  bootstrapper.InitMojo(/*as_browser_process=*/false);

  // This is the task queue that `ObjectAImpl`'s receiver will be bound to. We
  // freeze it to demonstrate that channel-associated interfaces bound to frozen
  // queues *still* have their messages delivered.
  scoped_refptr<CustomTaskQueue> initially_frozen_tq =
      base::MakeRefCounted<CustomTaskQueue>(
          *bootstrapper.sequence_manager.get(),
          base::sequence_manager::TaskQueue::Spec(
              base::sequence_manager::QueueName::TEST_TQ));
  initially_frozen_tq->FreezeTaskQueue();

  // The rest of the magic happens in this object.
  std::unique_ptr<RendererIPCListener> renderer_ipc_listener =
      std::make_unique<RendererIPCListener>(
          /*io_task_runner=*/bootstrapper.io_task_runner,
          initially_frozen_tq->task_runner());

  // Post a task for 3 seconds from now that will unfreeze the TaskRunner that
  // the `codelabs::mojom::ObjectA` implementation is bound to. This would
  // normally block all messages from going to their corresponding
  // implementations (i.e., messages bound for ObjectA would be blocked, and
  // necessarily subsequent messages bound for ObjectB would *also* be blocked),
  // however since the associated interfaces here are specifically
  // *channel*-associated, we do not support blocking messages, so they're all
  // delivered immediately.
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          [](scoped_refptr<CustomTaskQueue> initially_frozen_tq) {
            LOG(INFO) << "Unfreezing frozen TaskRunner";
            initially_frozen_tq->UnfreezeTaskQueue();
          },
          initially_frozen_tq),
      base::Seconds(3));

  // This task is posted first, but will not run until the task runner is
  // unfrozen in ~3 seconds.
  initially_frozen_tq->task_runner()->PostTask(
      FROM_HERE, base::BindOnce([]() {
        LOG(WARNING) << "Renderer: This is the first task posted to the frozen "
                        "TaskRunner. It shouldn't run within the first 2 "
                        "seconds of the program";
      }));

  base::RunLoop run_loop;
  run_loop.Run();

  return 0;
}
