// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/task/sequence_manager/task_queue.h"
#include "codelabs/mojo_examples/mojo_impls.h"
#include "codelabs/mojo_examples/mojom/interface.mojom.h"
#include "codelabs/mojo_examples/process_bootstrapper.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "mojo/public/cpp/system/invitation.h"
#include "mojo/public/cpp/system/message_pipe.h"

static ObjectAImpl g_object_a;
static ObjectBImpl g_object_b;

class ProcessImpl : public codelabs::mojom::Process {
 public:
  ProcessImpl(mojo::PendingReceiver<codelabs::mojom::Process> pending_receiver,
              scoped_refptr<base::SingleThreadTaskRunner> freezable_tq_runner) {
    receiver_.Bind(std::move(pending_receiver));
    freezable_tq_runner_ = std::move(freezable_tq_runner);
  }

 private:
  // codelabs::mojo::Process
  void SayHello() override {
    LOG(INFO) << "Hello! (invoked in the renderer, from the browser)";
  }
  void GetAssociatedInterface(
      const std::string& name,
      mojo::PendingAssociatedReceiver<codelabs::mojom::GenericInterface>
          receiver) override {
    LOG(INFO) << "Renderer: GetAssociatedInterface() for " << name;
    if (name == "ObjectA") {
      mojo::PendingAssociatedReceiver<codelabs::mojom::ObjectA> pending_a(
          receiver.PassHandle());
      g_object_a.BindToFrozenTaskRunner(std::move(pending_a),
                                        std::move(freezable_tq_runner_));
    } else if (name == "ObjectB") {
      mojo::PendingAssociatedReceiver<codelabs::mojom::ObjectB> pending_b(
          receiver.PassHandle());
      g_object_b.Bind(std::move(pending_b));
    }
  }

  mojo::Receiver<codelabs::mojom::Process> receiver_{this};
  // This is a freezable task runner that only `g_object_a` gets bound to.
  scoped_refptr<base::SingleThreadTaskRunner> freezable_tq_runner_;
};

static std::unique_ptr<ProcessImpl> g_process_impl;

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

int main(int argc, char** argv) {
  base::CommandLine::Init(argc, argv);
  LOG(INFO) << "Renderer: "
            << base::CommandLine::ForCurrentProcess()->GetCommandLineString();

  // Set up the scheduling infrastructure for this process. It consists of:
  //   1.) A SequenceManager that is bound to the current thread (main thread)
  //   2.) A default task queue
  //   3.) A `CustomTaskQueue` that is easily freezable and unfreezable. This
  //   part is specific to this example.
  ProcessBootstrapper bootstrapper;
  bootstrapper.InitMainThread(base::MessagePumpType::IO);
  bootstrapper.InitMojo(/*as_browser_process=*/false);

  scoped_refptr<CustomTaskQueue> freezable_tq =
      base::MakeRefCounted<CustomTaskQueue>(
          *bootstrapper.sequence_manager.get(),
          base::sequence_manager::TaskQueue::Spec(
              base::sequence_manager::QueueName::TEST_TQ));
  freezable_tq->FreezeTaskQueue();

  // Accept an invitation.
  mojo::IncomingInvitation invitation = mojo::IncomingInvitation::Accept(
      mojo::PlatformChannel::RecoverPassedEndpointFromCommandLine(
          *base::CommandLine::ForCurrentProcess()));
  mojo::ScopedMessagePipeHandle pipe = invitation.ExtractMessagePipe("pipe");

  base::RunLoop run_loop;

  // Post a task that will run in 3 seconds, that will unfreeze the custom task
  // queue to which the `codelabs::mojom::ObjectA` object is bound to.
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          [](scoped_refptr<CustomTaskQueue> freezable_tq) {
            freezable_tq->UnfreezeTaskQueue();
          },
          freezable_tq),
      base::Seconds(3));

  // Create a process-wide receiver that will broker connects to the backing
  // `codelabs::mojom::ObjectA` and `codelabs::mojom::ObjectB` implementations.
  mojo::PendingReceiver<codelabs::mojom::Process> pending_receiver(
      std::move(pipe));
  g_process_impl = std::make_unique<ProcessImpl>(std::move(pending_receiver),
                                                 freezable_tq->task_runner());
  run_loop.Run();

  return 0;
}
