// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/service_manager_connection.h"

#include <map>
#include <queue>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "base/task/current_thread.h"
#include "base/task/sequenced_task_runner.h"
#include "base/thread_annotations.h"
#include "base/threading/thread_checker.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/service_receiver.h"
#include "services/service_manager/public/mojom/constants.mojom.h"
#include "services/service_manager/public/mojom/interface_provider.mojom.h"

namespace chromecast {
namespace {

std::unique_ptr<ServiceManagerConnection>& GetConnectionForProcess() {
  static base::NoDestructor<std::unique_ptr<ServiceManagerConnection>>
      connection;
  return *connection;
}

}  // namespace

// A ref-counted object which owns the IO thread state of a
// ServiceManagerConnection. This includes Service and ServiceFactory
// bindings.
class ServiceManagerConnection::IOThreadContext
    : public base::RefCountedThreadSafe<IOThreadContext>,
      public service_manager::Service {
 public:
  IOThreadContext(
      mojo::PendingReceiver<service_manager::mojom::Service> service_receiver,
      scoped_refptr<base::SequencedTaskRunner> io_task_runner,
      mojo::PendingReceiver<service_manager::mojom::Connector>
          connector_receiver)
      : pending_service_receiver_(std::move(service_receiver)),
        io_task_runner_(io_task_runner),
        pending_connector_receiver_(std::move(connector_receiver)) {
    // This will be reattached by any of the IO thread functions on first call.
    io_thread_checker_.DetachFromThread();
  }

  IOThreadContext(const IOThreadContext&) = delete;
  IOThreadContext& operator=(const IOThreadContext&) = delete;

  // Safe to call from any thread.
  void Start() {
    DCHECK(!started_);

    started_ = true;
    io_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&IOThreadContext::StartOnIOThread, this));
  }

  // Safe to call from whichever thread called Start() (or may have called
  // Start()). Must be called before IO thread shutdown.
  void ShutDown() {
    if (!started_)
      return;

    bool posted = io_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&IOThreadContext::ShutDownOnIOThread, this));
    DCHECK(posted);
  }

 private:
  friend class base::RefCountedThreadSafe<IOThreadContext>;

  class MessageLoopObserver : public base::CurrentThread::DestructionObserver {
   public:
    explicit MessageLoopObserver(base::WeakPtr<IOThreadContext> context)
        : context_(context) {
      base::CurrentThread::Get()->AddDestructionObserver(this);
    }

    MessageLoopObserver(const MessageLoopObserver&) = delete;
    MessageLoopObserver& operator=(const MessageLoopObserver&) = delete;

    ~MessageLoopObserver() override {
      base::CurrentThread::Get()->RemoveDestructionObserver(this);
    }

    void ShutDown() {
      if (!is_active_)
        return;

      // The call into |context_| below may reenter ShutDown(), hence we set
      // |is_active_| to false here.
      is_active_ = false;
      if (context_)
        context_->ShutDownOnIOThread();

      delete this;
    }

   private:
    void WillDestroyCurrentMessageLoop() override {
      DCHECK(is_active_);
      ShutDown();
    }

    bool is_active_ = true;
    base::WeakPtr<IOThreadContext> context_;
  };

  ~IOThreadContext() override {}

  void StartOnIOThread() {
    // Should bind |io_thread_checker_| to the context's thread.
    DCHECK(io_thread_checker_.CalledOnValidThread());
    service_receiver_ = std::make_unique<service_manager::ServiceReceiver>(
        this, std::move(pending_service_receiver_));
    service_receiver_->GetConnector()->BindConnectorReceiver(
        std::move(pending_connector_receiver_));

    // MessageLoopObserver owns itself.
    message_loop_observer_ =
        new MessageLoopObserver(weak_factory_.GetWeakPtr());
  }

  void ShutDownOnIOThread() {
    DCHECK(io_thread_checker_.CalledOnValidThread());

    weak_factory_.InvalidateWeakPtrs();

    // Note that this method may be invoked by MessageLoopObserver observing
    // MessageLoop destruction. In that case, this call to ShutDown is
    // effectively a no-op. In any case it's safe.
    if (message_loop_observer_) {
      message_loop_observer_->ShutDown();
      message_loop_observer_ = nullptr;
    }

    // Resetting the ServiceContext below may otherwise release the last
    // reference to this IOThreadContext. We keep it alive until the stack
    // unwinds.
    scoped_refptr<IOThreadContext> keepalive(this);

    service_receiver_.reset();
  }

  /////////////////////////////////////////////////////////////////////////////
  // service_manager::Service implementation

  void OnBindInterface(const service_manager::BindSourceInfo& source_info,
                       const std::string& interface_name,
                       mojo::ScopedMessagePipeHandle interface_pipe) override {}

  base::ThreadChecker io_thread_checker_;
  bool started_ = false;

  // Temporary state established on construction and consumed on the IO thread
  // once the connection is started.
  mojo::PendingReceiver<service_manager::mojom::Service>
      pending_service_receiver_;
  scoped_refptr<base::SequencedTaskRunner> io_task_runner_;
  mojo::PendingReceiver<service_manager::mojom::Connector>
      pending_connector_receiver_;

  // TaskRunner on which to run our owner's callbacks, i.e. the ones passed to
  // Start().
  std::unique_ptr<service_manager::ServiceReceiver> service_receiver_;

  // Not owned.
  MessageLoopObserver* message_loop_observer_ = nullptr;

  base::WeakPtrFactory<IOThreadContext> weak_factory_{this};
};

// static
void ServiceManagerConnection::SetForProcess(
    std::unique_ptr<ServiceManagerConnection> connection) {
  DCHECK(!GetConnectionForProcess());
  GetConnectionForProcess() = std::move(connection);
}

// static
ServiceManagerConnection* ServiceManagerConnection::GetForProcess() {
  return GetConnectionForProcess().get();
}

// static
void ServiceManagerConnection::DestroyForProcess() {
  // This joins the service manager controller thread.
  GetConnectionForProcess().reset();
}

// static
std::unique_ptr<ServiceManagerConnection> ServiceManagerConnection::Create(
    mojo::PendingReceiver<service_manager::mojom::Service> receiver,
    scoped_refptr<base::SequencedTaskRunner> io_task_runner) {
  return std::make_unique<ServiceManagerConnection>(std::move(receiver),
                                                    io_task_runner);
}

ServiceManagerConnection::ServiceManagerConnection(
    mojo::PendingReceiver<service_manager::mojom::Service> receiver,
    scoped_refptr<base::SequencedTaskRunner> io_task_runner) {
  mojo::PendingReceiver<service_manager::mojom::Connector> connector_receiver;
  connector_ = service_manager::Connector::Create(&connector_receiver);
  context_ = new IOThreadContext(std::move(receiver), io_task_runner,
                                 std::move(connector_receiver));
}

ServiceManagerConnection::~ServiceManagerConnection() {
  context_->ShutDown();
}

void ServiceManagerConnection::Start() {
  context_->Start();
}

service_manager::Connector* ServiceManagerConnection::GetConnector() {
  return connector_.get();
}

}  // namespace chromecast
