// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/machine_learning/public/cpp/service_connection.h"

#include <utility>

#include "base/component_export.h"
#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "chromeos/dbus/machine_learning/machine_learning_client.h"
#include "chromeos/services/machine_learning/public/mojom/machine_learning_service.mojom.h"
#include "mojo/core/embedder/embedder.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "mojo/public/cpp/system/invitation.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace ash {
namespace machine_learning {

namespace {

// Real Impl of ServiceConnection
class COMPONENT_EXPORT(CHROMEOS_MLSERVICE) ServiceConnectionAsh
    : public chromeos::machine_learning::ServiceConnection {
 public:
  ServiceConnectionAsh();
  ServiceConnectionAsh(const ServiceConnectionAsh&) = delete;
  ServiceConnectionAsh& operator=(const ServiceConnectionAsh&) = delete;

  ~ServiceConnectionAsh() override;

  chromeos::machine_learning::mojom::MachineLearningService&
  GetMachineLearningService() override;

  void BindMachineLearningService(
      mojo::PendingReceiver<
          chromeos::machine_learning::mojom::MachineLearningService> receiver)
      override;

  void Initialize() override;

 private:
  // Binds the primordial, top-level interface |machine_learning_service_| to an
  // implementation in the ML Service daemon, if it is not already bound. The
  // binding is accomplished via D-Bus bootstrap.
  void BindPrimordialMachineLearningServiceIfNeeded();

  // Mojo disconnect handler. Resets |machine_learning_service_|, which
  // will be reconnected upon next use.
  void OnMojoDisconnect();

  // Response callback for MlClient::BootstrapMojoConnection.
  void OnBootstrapMojoConnectionResponse(bool success);

  mojo::Remote<chromeos::machine_learning::mojom::MachineLearningService>
      machine_learning_service_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  SEQUENCE_CHECKER(sequence_checker_);
};

ServiceConnectionAsh::ServiceConnectionAsh() {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

ServiceConnectionAsh::~ServiceConnectionAsh() = default;

chromeos::machine_learning::mojom::MachineLearningService&
ServiceConnectionAsh::GetMachineLearningService() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(task_runner_)
      << "Call Initialize before first use of ServiceConnection.";
  BindPrimordialMachineLearningServiceIfNeeded();
  return *machine_learning_service_.get();
}

void ServiceConnectionAsh::BindMachineLearningService(
    mojo::PendingReceiver<
        chromeos::machine_learning::mojom::MachineLearningService> receiver) {
  DCHECK(task_runner_)
      << "Call Initialize before first use of ServiceConnection.";
  if (!task_runner_->RunsTasksInCurrentSequence()) {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&ServiceConnectionAsh::BindMachineLearningService,
                       base::Unretained(this), std::move(receiver)));
    return;
  }

  GetMachineLearningService().Clone(std::move(receiver));
}

void ServiceConnectionAsh::Initialize() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!task_runner_) << "Initialize must be called only once.";

  task_runner_ = base::SequencedTaskRunner::GetCurrentDefault();
}

void ServiceConnectionAsh::BindPrimordialMachineLearningServiceIfNeeded() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (machine_learning_service_) {
    return;
  }

  mojo::PlatformChannel platform_channel;

  // Prepare a Mojo invitation to send through |platform_channel|.
  mojo::OutgoingInvitation invitation;
  // Include an initial Mojo pipe in the invitation.
  mojo::ScopedMessagePipeHandle pipe =
      invitation.AttachMessagePipe(ml::kBootstrapMojoConnectionChannelToken);
  if (mojo::core::IsMojoIpczEnabled()) {
    // IPCz requires an application to explicitly opt in to broker sharing
    // and inheritance when establishing a direct connection between two
    // non-broker nodes.
    invitation.set_extra_flags(MOJO_SEND_INVITATION_FLAG_SHARE_BROKER);
  }
  mojo::OutgoingInvitation::Send(std::move(invitation),
                                 base::kNullProcessHandle,
                                 platform_channel.TakeLocalEndpoint());

  // Bind our end of |pipe| to our mojo::Remote<MachineLearningService>. The
  // daemon should bind its end to a MachineLearningService implementation.
  machine_learning_service_.Bind(
      mojo::PendingRemote<
          chromeos::machine_learning::mojom::MachineLearningService>(
          std::move(pipe), 0u /* version */));
  machine_learning_service_.set_disconnect_handler(base::BindOnce(
      &ServiceConnectionAsh::OnMojoDisconnect, base::Unretained(this)));

  // Send the file descriptor for the other end of |platform_channel| to the
  // ML service daemon over D-Bus.
  chromeos::MachineLearningClient::Get()->BootstrapMojoConnection(
      platform_channel.TakeRemoteEndpoint().TakePlatformHandle().TakeFD(),
      base::BindOnce(&ServiceConnectionAsh::OnBootstrapMojoConnectionResponse,
                     base::Unretained(this)));
}

void ServiceConnectionAsh::OnMojoDisconnect() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Connection errors are not expected so log a warning.
  LOG(WARNING) << "ML Service Mojo connection closed";
  machine_learning_service_.reset();
}

void ServiceConnectionAsh::OnBootstrapMojoConnectionResponse(
    const bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!success) {
    LOG(WARNING) << "BootstrapMojoConnection D-Bus call failed";
    machine_learning_service_.reset();
  }
}

}  // namespace

}  // namespace machine_learning
}  // namespace ash

namespace chromeos {
namespace machine_learning {

ServiceConnection* ServiceConnection::CreateRealInstance() {
  static base::NoDestructor<ash::machine_learning::ServiceConnectionAsh>
      service_connection;
  return service_connection.get();
}

}  // namespace machine_learning
}  // namespace chromeos
