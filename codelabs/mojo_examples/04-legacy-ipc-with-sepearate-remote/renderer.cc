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
#include "mojo/public/cpp/bindings/self_owned_associated_receiver.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
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

class ProcessImpl : public codelabs::mojom::Process {
 public:
  ProcessImpl(
      scoped_refptr<base::SingleThreadTaskRunner> initially_frozen_task_runner)
      : initially_frozen_task_runner_(initially_frozen_task_runner) {}

  void SayHello() override {}

  void GetAssociatedInterface(
      const std::string& name,
      mojo::PendingAssociatedReceiver<codelabs::mojom::GenericInterface>
          receiver) override {
    LOG(INFO) << "Renderer: GetAssociatedInterface() for " << name;
    if (name == "ObjectA") {
      mojo::PendingAssociatedReceiver<codelabs::mojom::ObjectA> pending_a(
          receiver.PassHandle());
      g_object_a.BindToFrozenTaskRunner(
          std::move(pending_a), std::move(initially_frozen_task_runner_));
    } else if (name == "ObjectB") {
      mojo::PendingAssociatedReceiver<codelabs::mojom::ObjectB> pending_b(
          receiver.PassHandle());
      g_object_b.Bind(std::move(pending_b));
    }
  }

 private:
  scoped_refptr<base::SingleThreadTaskRunner> initially_frozen_task_runner_;
};

class AssociatedProcess : public codelabs::mojom::AssociatedProcess {
 public:
  AssociatedProcess(
      scoped_refptr<base::SingleThreadTaskRunner> initially_frozen_task_runner)
      : initially_frozen_task_runner_(initially_frozen_task_runner) {}

  void SetProcess(
      mojo::PendingReceiver<codelabs::mojom::Process> process) override {
    LOG(INFO) << "Creating process handler";
    MakeSelfOwnedReceiver(
        std::make_unique<ProcessImpl>(initially_frozen_task_runner_),
        std::move(process));
  }

 private:
  scoped_refptr<base::SingleThreadTaskRunner> initially_frozen_task_runner_;
};

class RendererIPCListener : public IPC::Listener {
 public:
  RendererIPCListener(
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> initially_frozen_task_runner)
      : initially_frozen_task_runner_(initially_frozen_task_runner) {
    // See 03-channel-associated-interface-freezing/renderer.cc for relevant
    // comments here.

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

    if (interface_name == "codelabs.mojom.AssociatedProcess") {
      mojo::PendingAssociatedReceiver<codelabs::mojom::AssociatedProcess>
          pending_process(std::move(handle));
      mojo::MakeSelfOwnedAssociatedReceiver(
          std::make_unique<AssociatedProcess>(initially_frozen_task_runner_),
          std::move(pending_process));
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
  // queues do not have their messages delivered. This is where we differ from
  // 03-channel-associated-interface-freezing.
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
  // the `codelabs::mojom::ObjectA` implementation is bound to. This blocks
  // all IPC messages on the interfaces from being processed.
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
