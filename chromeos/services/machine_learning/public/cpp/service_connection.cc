// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/machine_learning/public/cpp/service_connection.h"

#include "base/bind.h"
#include "base/macros.h"
#include "base/no_destructor.h"
#include "base/sequence_checker.h"
#include "chromeos/dbus/machine_learning/machine_learning_client.h"
#include "chromeos/services/machine_learning/public/mojom/handwriting_recognizer.mojom.h"
#include "chromeos/services/machine_learning/public/mojom/machine_learning_service.mojom.h"
#include "chromeos/services/machine_learning/public/mojom/model.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "mojo/public/cpp/system/invitation.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {
namespace machine_learning {

namespace {

// Real Impl of ServiceConnection
class ServiceConnectionImpl : public ServiceConnection {
 public:
  ServiceConnectionImpl();
  ~ServiceConnectionImpl() override = default;

  mojom::MachineLearningService& GetMachineLearningService() override;

  void BindMachineLearningService(
      mojo::PendingReceiver<mojom::MachineLearningService> receiver) override;

  void Initialize() override;

  void LoadBuiltinModel(mojom::BuiltinModelSpecPtr spec,
                        mojo::PendingReceiver<mojom::Model> receiver,
                        mojom::MachineLearningService::LoadBuiltinModelCallback
                            result_callback) override;

  void LoadFlatBufferModel(
      mojom::FlatBufferModelSpecPtr spec,
      mojo::PendingReceiver<mojom::Model> receiver,
      mojom::MachineLearningService::LoadFlatBufferModelCallback
          result_callback) override;

  void LoadTextClassifier(
      mojo::PendingReceiver<mojom::TextClassifier> receiver,
      mojom::MachineLearningService::LoadTextClassifierCallback result_callback)
      override;

  void LoadHandwritingModel(
      mojom::HandwritingRecognizerSpecPtr spec,
      mojo::PendingReceiver<mojom::HandwritingRecognizer> receiver,
      mojom::MachineLearningService::LoadHandwritingModelCallback
          result_callback) override;

  void LoadHandwritingModelWithSpec(
      mojom::HandwritingRecognizerSpecPtr spec,
      mojo::PendingReceiver<mojom::HandwritingRecognizer> receiver,
      mojom::MachineLearningService::LoadHandwritingModelWithSpecCallback
          result_callback) override;

  void LoadGrammarChecker(
      mojo::PendingReceiver<mojom::GrammarChecker> receiver,
      mojom::MachineLearningService::LoadGrammarCheckerCallback result_callback)
      override;

  void LoadSpeechRecognizer(
      mojom::SodaConfigPtr soda_config,
      mojo::PendingRemote<mojom::SodaClient> soda_client,
      mojo::PendingReceiver<mojom::SodaRecognizer> soda_recognizer,
      mojom::MachineLearningService::LoadSpeechRecognizerCallback callback)
      override;

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

  mojo::Remote<mojom::MachineLearningService> machine_learning_service_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(ServiceConnectionImpl);
};

mojom::MachineLearningService&
ServiceConnectionImpl::GetMachineLearningService() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(task_runner_)
      << "Call Initialize before first use of ServiceConnection.";
  BindPrimordialMachineLearningServiceIfNeeded();
  return *machine_learning_service_.get();
}

void ServiceConnectionImpl::BindMachineLearningService(
    mojo::PendingReceiver<mojom::MachineLearningService> receiver) {
  DCHECK(task_runner_)
      << "Call Initialize before first use of ServiceConnection.";
  if (!task_runner_->RunsTasksInCurrentSequence()) {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&ServiceConnectionImpl::BindMachineLearningService,
                       base::Unretained(this), std::move(receiver)));
    return;
  }

  GetMachineLearningService().Clone(std::move(receiver));
}

void ServiceConnectionImpl::Initialize() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!task_runner_) << "Initialize must be called only once.";

  task_runner_ = base::SequencedTaskRunnerHandle::Get();
}

void ServiceConnectionImpl::LoadBuiltinModel(
    mojom::BuiltinModelSpecPtr spec,
    mojo::PendingReceiver<mojom::Model> receiver,
    mojom::MachineLearningService::LoadBuiltinModelCallback result_callback) {
  if (!task_runner_->RunsTasksInCurrentSequence()) {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&ServiceConnectionImpl::LoadBuiltinModel,
                       base::Unretained(this), std::move(spec),
                       std::move(receiver), std::move(result_callback)));
    return;
  }

  BindPrimordialMachineLearningServiceIfNeeded();
  machine_learning_service_->LoadBuiltinModel(
      std::move(spec), std::move(receiver), std::move(result_callback));
}

void ServiceConnectionImpl::LoadFlatBufferModel(
    mojom::FlatBufferModelSpecPtr spec,
    mojo::PendingReceiver<mojom::Model> receiver,
    mojom::MachineLearningService::LoadFlatBufferModelCallback
        result_callback) {
  if (!task_runner_->RunsTasksInCurrentSequence()) {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&ServiceConnectionImpl::LoadFlatBufferModel,
                       base::Unretained(this), std::move(spec),
                       std::move(receiver), std::move(result_callback)));
    return;
  }

  BindPrimordialMachineLearningServiceIfNeeded();
  machine_learning_service_->LoadFlatBufferModel(
      std::move(spec), std::move(receiver), std::move(result_callback));
}

void ServiceConnectionImpl::LoadTextClassifier(
    mojo::PendingReceiver<mojom::TextClassifier> receiver,
    mojom::MachineLearningService::LoadTextClassifierCallback result_callback) {
  if (!task_runner_->RunsTasksInCurrentSequence()) {
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&ServiceConnectionImpl::LoadTextClassifier,
                                  base::Unretained(this), std::move(receiver),
                                  std::move(result_callback)));
    return;
  }

  BindPrimordialMachineLearningServiceIfNeeded();
  machine_learning_service_->LoadTextClassifier(std::move(receiver),
                                                std::move(result_callback));
}

void ServiceConnectionImpl::LoadHandwritingModel(
    mojom::HandwritingRecognizerSpecPtr spec,
    mojo::PendingReceiver<mojom::HandwritingRecognizer> receiver,
    mojom::MachineLearningService::LoadHandwritingModelCallback
        result_callback) {
  if (!task_runner_->RunsTasksInCurrentSequence()) {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&ServiceConnectionImpl::LoadHandwritingModel,
                       base::Unretained(this), std::move(spec),
                       std::move(receiver), std::move(result_callback)));
    return;
  }

  BindPrimordialMachineLearningServiceIfNeeded();
  machine_learning_service_->LoadHandwritingModel(
      std::move(spec), std::move(receiver), std::move(result_callback));
}

void ServiceConnectionImpl::LoadHandwritingModelWithSpec(
    mojom::HandwritingRecognizerSpecPtr spec,
    mojo::PendingReceiver<mojom::HandwritingRecognizer> receiver,
    mojom::MachineLearningService::LoadHandwritingModelWithSpecCallback
        result_callback) {
  if (!task_runner_->RunsTasksInCurrentSequence()) {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&ServiceConnectionImpl::LoadHandwritingModelWithSpec,
                       base::Unretained(this), std::move(spec),
                       std::move(receiver), std::move(result_callback)));
    return;
  }

  BindPrimordialMachineLearningServiceIfNeeded();
  machine_learning_service_->LoadHandwritingModelWithSpec(
      std::move(spec), std::move(receiver), std::move(result_callback));
}

void ServiceConnectionImpl::LoadGrammarChecker(
    mojo::PendingReceiver<mojom::GrammarChecker> receiver,
    mojom::MachineLearningService::LoadGrammarCheckerCallback result_callback) {
  if (!task_runner_->RunsTasksInCurrentSequence()) {
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&ServiceConnectionImpl::LoadGrammarChecker,
                                  base::Unretained(this), std::move(receiver),
                                  std::move(result_callback)));
    return;
  }

  BindPrimordialMachineLearningServiceIfNeeded();
  machine_learning_service_->LoadGrammarChecker(std::move(receiver),
                                                std::move(result_callback));
}

void ServiceConnectionImpl::LoadSpeechRecognizer(
    mojom::SodaConfigPtr soda_config,
    mojo::PendingRemote<mojom::SodaClient> soda_client,
    mojo::PendingReceiver<mojom::SodaRecognizer> soda_recognizer,
    mojom::MachineLearningService::LoadSpeechRecognizerCallback callback) {
  if (!task_runner_->RunsTasksInCurrentSequence()) {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&ServiceConnectionImpl::LoadSpeechRecognizer,
                       base::Unretained(this), std::move(soda_config),
                       std::move(soda_client), std::move(soda_recognizer),
                       std::move(callback)));
    return;
  }

  BindPrimordialMachineLearningServiceIfNeeded();
  machine_learning_service_->LoadSpeechRecognizer(
      std::move(soda_config), std::move(soda_client),
      std::move(soda_recognizer), std::move(callback));
}

void ServiceConnectionImpl::BindPrimordialMachineLearningServiceIfNeeded() {
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
  mojo::OutgoingInvitation::Send(std::move(invitation),
                                 base::kNullProcessHandle,
                                 platform_channel.TakeLocalEndpoint());

  // Bind our end of |pipe| to our mojo::Remote<MachineLearningService>. The
  // daemon should bind its end to a MachineLearningService implementation.
  machine_learning_service_.Bind(
      mojo::PendingRemote<machine_learning::mojom::MachineLearningService>(
          std::move(pipe), 0u /* version */));
  machine_learning_service_.set_disconnect_handler(base::BindOnce(
      &ServiceConnectionImpl::OnMojoDisconnect, base::Unretained(this)));

  // Send the file descriptor for the other end of |platform_channel| to the
  // ML service daemon over D-Bus.
  MachineLearningClient::Get()->BootstrapMojoConnection(
      platform_channel.TakeRemoteEndpoint().TakePlatformHandle().TakeFD(),
      base::BindOnce(&ServiceConnectionImpl::OnBootstrapMojoConnectionResponse,
                     base::Unretained(this)));
}

ServiceConnectionImpl::ServiceConnectionImpl() {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

void ServiceConnectionImpl::OnMojoDisconnect() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Connection errors are not expected so log a warning.
  LOG(WARNING) << "ML Service Mojo connection closed";
  machine_learning_service_.reset();
}

void ServiceConnectionImpl::OnBootstrapMojoConnectionResponse(
    const bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!success) {
    LOG(WARNING) << "BootstrapMojoConnection D-Bus call failed";
    machine_learning_service_.reset();
  }
}

static ServiceConnection* g_fake_service_connection_for_testing = nullptr;

}  // namespace

ServiceConnection* ServiceConnection::GetInstance() {
  if (g_fake_service_connection_for_testing) {
    return g_fake_service_connection_for_testing;
  }
  static base::NoDestructor<ServiceConnectionImpl> service_connection;
  return service_connection.get();
}

void ServiceConnection::UseFakeServiceConnectionForTesting(
    ServiceConnection* const fake_service_connection) {
  g_fake_service_connection_for_testing = fake_service_connection;
}

}  // namespace machine_learning
}  // namespace chromeos
