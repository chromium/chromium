// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/external_mojo/external_service_support/external_connector_impl.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/synchronization/lock.h"
#include "base/task/sequenced_task_runner.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "chromecast/external_mojo/broker_service/broker_service.h"
#include "chromecast/external_mojo/external_service_support/external_service.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/platform/named_platform_channel.h"
#include "mojo/public/cpp/platform/platform_channel_endpoint.h"
#include "mojo/public/cpp/system/invitation.h"
#include "services/service_manager/public/cpp/connector.h"

namespace chromecast {
namespace external_service_support {

namespace {
constexpr base::TimeDelta kConnectRetryDelay = base::Milliseconds(500);
}  // namespace

// Since we are only allowed to make a single underlying connection to the
// broker, we share the underlying connection between all ExternalConnector
// instances. The ExternalConnectors use clones of the underlying connection.
//
// Since connection error callbacks are called in some arbitrary order, we need
// to be careful to handle disconnection correctly. Each underlying connection
// has a unique token (int64_t) associated with it, which is propagated to all
// clones. If any clone receives a disconnect callback, it tries to reconnect by
// calling ConnectClone(), passing in the previous token (associated with the
// broken connection). BrokerConnection then attempts to reconnect the
// underlying connection if the broken connection token matches the token for
// the current connection; if it doesn't match, the connection was already
// recreated, so nothing needs to be done.
class ExternalConnectorImpl::BrokerConnection
    : public base::RefCountedThreadSafe<BrokerConnection> {
 public:
  explicit BrokerConnection(std::string broker_path)
      : broker_path_(std::move(broker_path)),
        task_runner_(base::SequencedTaskRunner::GetCurrentDefault()) {
    Connect();
  }

  int64_t ConnectClone(
      int64_t dead_connection_token,
      mojo::PendingReceiver<external_mojo::mojom::ExternalConnector> receiver) {
    int64_t token;
    {
      base::AutoLock lock(lock_);
      if (dead_connection_token == connection_token_) {
        task_runner_->PostTask(
            FROM_HERE, base::BindOnce(&BrokerConnection::Connect, this));
        ++connection_token_;
      }
      token = connection_token_;
    }
    task_runner_->PostTask(FROM_HERE,
                           base::BindOnce(&BrokerConnection::AttachClone, this,
                                          std::move(receiver)));
    return token;
  }

 private:
  friend class base::RefCountedThreadSafe<BrokerConnection>;
  ~BrokerConnection() = default;

  void Connect() {
    DCHECK(task_runner_->RunsTasksInCurrentSequence());
    connector_.reset();
    pending_receiver_ = connector_.BindNewPipeAndPassReceiver();
    AttemptBrokerConnection();
  }

  void AttachClone(
      mojo::PendingReceiver<external_mojo::mojom::ExternalConnector> receiver) {
    DCHECK(task_runner_->RunsTasksInCurrentSequence());
    connector_->Clone(std::move(receiver));
  }

  void AttemptBrokerConnection() {
    mojo::NamedPlatformChannel::Options channel_options;
    channel_options.server_name = broker_path_;
#if BUILDFLAG(IS_ANDROID)
    // On Android, use the abstract namespace to avoid filesystem access.
    channel_options.use_abstract_namespace = true;
#endif
    mojo::PlatformChannelEndpoint endpoint =
        mojo::NamedPlatformChannel::ConnectToServer(channel_options);
    if (!endpoint.is_valid()) {
      task_runner_->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&BrokerConnection::AttemptBrokerConnection,
                         weak_factory_.GetWeakPtr()),
          kConnectRetryDelay);
      return;
    }

    auto invitation = mojo::IncomingInvitation::Accept(std::move(endpoint));
    auto remote_pipe = invitation.ExtractMessagePipe(0);
    if (!remote_pipe) {
      LOG(ERROR) << "Invalid message pipe";
      task_runner_->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&BrokerConnection::AttemptBrokerConnection,
                         weak_factory_.GetWeakPtr()),
          kConnectRetryDelay);
      return;
    }

    mojo::FuseMessagePipes(pending_receiver_.PassPipe(),
                           std::move(remote_pipe));
  }

  const std::string broker_path_;
  const scoped_refptr<base::SequencedTaskRunner> task_runner_;

  mojo::Remote<external_mojo::mojom::ExternalConnector> connector_;
  mojo::PendingReceiver<external_mojo::mojom::ExternalConnector>
      pending_receiver_;

  base::Lock lock_;
  int64_t connection_token_ GUARDED_BY(lock_) = 1;

  base::WeakPtrFactory<BrokerConnection> weak_factory_{this};
};

// static
void ExternalConnector::Connect(
    const std::string& broker_path,
    base::OnceCallback<void(std::unique_ptr<ExternalConnector>)> callback) {
  DCHECK(callback);
  std::move(callback).Run(Create(broker_path));
}

// static
std::unique_ptr<ExternalConnector> ExternalConnector::Create(
    const std::string& broker_path) {
  return std::make_unique<ExternalConnectorImpl>(broker_path);
}

// static
std::unique_ptr<ExternalConnector> ExternalConnector::Create(
    mojo::PendingRemote<external_mojo::mojom::ExternalConnector> remote) {
  return std::make_unique<ExternalConnectorImpl>(std::move(remote));
}

// static
std::unique_ptr<ExternalConnector> ExternalConnector::Create(
    service_manager::Connector* connector) {
  mojo::PendingRemote<external_mojo::mojom::ExternalConnector> pending_remote;
  connector->BindInterface(external_mojo::BrokerService::kServiceName,
                           pending_remote.InitWithNewPipeAndPassReceiver());
  return std::make_unique<ExternalConnectorImpl>(std::move(pending_remote));
}

ExternalConnectorImpl::ExternalConnectorImpl(const std::string& broker_path)
    : broker_connection_(base::MakeRefCounted<BrokerConnection>(broker_path)) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
  Connect();
}

ExternalConnectorImpl::ExternalConnectorImpl(
    scoped_refptr<BrokerConnection> broker_connection)
    : broker_connection_(broker_connection) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
  Connect();
}

ExternalConnectorImpl::ExternalConnectorImpl(
    mojo::PendingRemote<external_mojo::mojom::ExternalConnector> pending_remote)
    : pending_remote_(std::move(pending_remote)) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

ExternalConnectorImpl::~ExternalConnectorImpl() = default;

base::CallbackListSubscription
ExternalConnectorImpl::AddConnectionErrorCallback(
    base::RepeatingClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return error_closures_.Add(std::move(callback));
}

void ExternalConnectorImpl::RegisterService(const std::string& service_name,
                                            ExternalService* service) {
  RegisterService(service_name, service->GetReceiver());
}

void ExternalConnectorImpl::RegisterService(
    const std::string& service_name,
    mojo::PendingRemote<external_mojo::mojom::ExternalService> service_remote) {
  BindConnectorIfNecessary();
  auto service_instance_info =
      chromecast::external_mojo::mojom::ServiceInstanceInfo::New(
          service_name, std::move(service_remote));
  std::vector<chromecast::external_mojo::mojom::ServiceInstanceInfoPtr> v;
  v.emplace_back(std::move(service_instance_info));
  connector_->RegisterServiceInstances(std::move(v));
}

void ExternalConnectorImpl::RegisterServices(
    const std::vector<std::string>& service_names,
    const std::vector<ExternalService*>& services) {
  CHECK(service_names.size() == services.size());
  std::vector<chromecast::external_mojo::mojom::ServiceInstanceInfoPtr>
      service_instances_info;
  service_instances_info.reserve(services.size());
  for (size_t i = 0; i < services.size(); ++i) {
    service_instances_info.emplace_back(
        chromecast::external_mojo::mojom::ServiceInstanceInfo::New(
            service_names[i], services[i]->GetReceiver()));
  }

  RegisterServices(std::move(service_instances_info));
}

void ExternalConnectorImpl::RegisterServices(
    std::vector<chromecast::external_mojo::mojom::ServiceInstanceInfoPtr>
        service_instances_info) {
  BindConnectorIfNecessary();
  connector_->RegisterServiceInstances(std::move(service_instances_info));
}

void ExternalConnectorImpl::QueryServiceList(
    base::OnceCallback<void(
        std::vector<chromecast::external_mojo::mojom::ExternalServiceInfoPtr>)>
        callback) {
  BindConnectorIfNecessary();
  connector_->QueryServiceList(std::move(callback));
}

void ExternalConnectorImpl::BindInterface(
    const std::string& service_name,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe,
    bool async) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!async) {
    BindInterfaceImmediately(service_name, interface_name,
                             std::move(interface_pipe));
    return;
  }
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&ExternalConnectorImpl::BindInterfaceImmediately,
                     weak_factory_.GetWeakPtr(), service_name, interface_name,
                     std::move(interface_pipe)));
}

void ExternalConnectorImpl::BindInterfaceImmediately(
    const std::string& service_name,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  BindConnectorIfNecessary();
  connector_->BindInterface(service_name, interface_name,
                            std::move(interface_pipe));
}

std::unique_ptr<ExternalConnector> ExternalConnectorImpl::Clone() {
  if (broker_connection_) {
    return std::make_unique<ExternalConnectorImpl>(broker_connection_);
  }
  // Bind to the current sequence since this is a public method.
  BindConnectorIfNecessary();
  return std::make_unique<ExternalConnectorImpl>(RequestConnector());
}

mojo::PendingRemote<external_mojo::mojom::ExternalConnector>
ExternalConnectorImpl::RequestConnector() {
  // Bind to the current sequence since this is a public method.
  BindConnectorIfNecessary();
  mojo::PendingRemote<external_mojo::mojom::ExternalConnector> remote;
  connector_->Clone(remote.InitWithNewPipeAndPassReceiver());
  return remote;
}

void ExternalConnectorImpl::SendChromiumConnectorRequest(
    mojo::ScopedMessagePipeHandle request) {
  BindConnectorIfNecessary();
  connector_->BindChromiumConnector(std::move(request));
}

void ExternalConnectorImpl::Connect() {
  DCHECK(broker_connection_);
  connection_token_ = broker_connection_->ConnectClone(
      connection_token_, pending_remote_.InitWithNewPipeAndPassReceiver());
}

void ExternalConnectorImpl::OnMojoDisconnect() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  connector_.reset();
  pending_remote_.reset();
  if (broker_connection_) {
    Connect();
    BindConnectorIfNecessary();
  }
  error_closures_.Notify();
}

void ExternalConnectorImpl::BindConnectorIfNecessary() {
  // Bind the message pipe and SequenceChecker to the current thread the first
  // time it is used to connect.
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (connector_.is_bound()) {
    return;
  }

  DCHECK(pending_remote_.is_valid());

  connector_.Bind(std::move(pending_remote_));
  connector_.set_disconnect_handler(base::BindOnce(
      &ExternalConnectorImpl::OnMojoDisconnect, base::Unretained(this)));
}

}  // namespace external_service_support
}  // namespace chromecast
