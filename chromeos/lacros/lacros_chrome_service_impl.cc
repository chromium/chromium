// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/lacros/lacros_chrome_service_impl.h"

#include <atomic>
#include <utility>

#include "base/logging.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chromeos/lacros/lacros_chrome_service_delegate.h"

namespace chromeos {
namespace {

// We use a std::atomic here rather than a base::NoDestructor because we want to
// allow instances of LacrosChromeServiceImpl to be destroyed to facilitate
// testing.
std::atomic<LacrosChromeServiceImpl*> g_instance = {nullptr};

}  // namespace

// This class that holds all state that is affine to a single, never-blocking
// sequence. The sequence must be never-blocking to avoid deadlocks, see
// https://crbug.com/1103765.
class LacrosChromeServiceNeverBlockingState
    : public crosapi::mojom::LacrosChromeService {
 public:
  LacrosChromeServiceNeverBlockingState(
      scoped_refptr<base::SequencedTaskRunner> owner_sequence,
      base::WeakPtr<LacrosChromeServiceImpl> owner,
      crosapi::mojom::LacrosInitParamsPtr* init_params)
      : owner_sequence_(owner_sequence),
        owner_(owner),
        init_params_(init_params) {
    DCHECK(init_params);
    DETACH_FROM_SEQUENCE(sequence_checker_);
  }
  ~LacrosChromeServiceNeverBlockingState() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  }

  // crosapi::mojom::LacrosChromeService:
  void Init(crosapi::mojom::LacrosInitParamsPtr params) override {
    *init_params_ = std::move(params);
    initialized_.Signal();
  }

  void RequestAshChromeServiceReceiver(
      RequestAshChromeServiceReceiverCallback callback) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    // TODO(hidehiko): Remove non-error logging from here.
    LOG(WARNING) << "AshChromeServiceReceiver requested.";
    std::move(callback).Run(std::move(pending_ash_chrome_service_receiver_));
  }

  void NewWindow(NewWindowCallback callback) override {
    owner_sequence_->PostTaskAndReply(
        FROM_HERE,
        base::BindOnce(&LacrosChromeServiceImpl::NewWindowAffineSequence,
                       owner_),
        std::move(callback));
  }

  // Unlike most of other methods of this class, this is called on the
  // affined thread. Specifically, it is intended to be called before starting
  // the message pumping of the affined thread to pass the initialization
  // parameter from ash-chrome needed for the procedure running before the
  // message pumping.
  void WaitForInit() { initialized_.Wait(); }

  // AshChromeService is the interface that lacros-chrome uses to message
  // ash-chrome. This method binds the remote, which allows queuing of message
  // to ash-chrome. The messages will not go through until
  // RequestAshChromeServiceReceiver() is invoked.
  void BindAshChromeServiceRemote() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    pending_ash_chrome_service_receiver_ =
        ash_chrome_service_.BindNewPipeAndPassReceiver();
  }

  // LacrosChromeService is the interface that ash-chrome uses to message
  // lacros-chrome. This handles and routes all incoming messages from
  // ash-chrome.
  void BindLacrosChromeServiceReceiver(
      mojo::PendingReceiver<crosapi::mojom::LacrosChromeService> receiver) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    receiver_.Bind(std::move(receiver));
  }

  // These methods pass the receiver end of a mojo message pipe to ash-chrome.
  // This effectively allows ash-chrome to receive messages sent on these
  // message pipes.
  void BindMessageCenterReceiver(
      mojo::PendingReceiver<crosapi::mojom::MessageCenter> pending_receiver) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    ash_chrome_service_->BindMessageCenter(std::move(pending_receiver));
  }

  void BindSelectFileReceiver(
      mojo::PendingReceiver<crosapi::mojom::SelectFile> pending_receiver) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    ash_chrome_service_->BindSelectFile(std::move(pending_receiver));
  }

  void BindScreenManagerReceiver(
      mojo::PendingReceiver<crosapi::mojom::ScreenManager> pending_receiver) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    ash_chrome_service_->BindScreenManager(std::move(pending_receiver));
  }

  void BindAttestationReceiver(
      mojo::PendingReceiver<crosapi::mojom::Attestation> pending_receiver) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    ash_chrome_service_->BindAttestation(std::move(pending_receiver));
  }

  base::WeakPtr<LacrosChromeServiceNeverBlockingState> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  // Receives and routes messages from ash-chrome.
  mojo::Receiver<crosapi::mojom::LacrosChromeService> receiver_{this};

  // This remote allows lacros-chrome to send messages to ash-chrome.
  mojo::Remote<crosapi::mojom::AshChromeService> ash_chrome_service_;

  // This class holds onto the receiver for AshChromeService until ash-chrome
  // is ready to bind it.
  mojo::PendingReceiver<crosapi::mojom::AshChromeService>
      pending_ash_chrome_service_receiver_;

  // This allows LacrosChromeServiceNeverBlockingState to route IPC messages
  // back to the affine thread on LacrosChromeServiceImpl. |owner_| is affine to
  // |owner_sequence_|.
  scoped_refptr<base::SequencedTaskRunner> owner_sequence_;
  base::WeakPtr<LacrosChromeServiceImpl> owner_;

  // Owned by LacrosChromeServiceImpl.
  crosapi::mojom::LacrosInitParamsPtr* const init_params_;

  // Lock to wait for Init() invocation.
  // Because the parameters are needed before starting the affined thread's
  // message pumping, it is necessary to use sync primitive here, instead.
  base::WaitableEvent initialized_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<LacrosChromeServiceNeverBlockingState> weak_factory_{
      this};
};

// static
LacrosChromeServiceImpl* LacrosChromeServiceImpl::Get() {
  return g_instance;
}

LacrosChromeServiceImpl::LacrosChromeServiceImpl(
    std::unique_ptr<LacrosChromeServiceDelegate> delegate)
    : delegate_(std::move(delegate)),
      sequenced_state_(nullptr, base::OnTaskRunnerDeleter(nullptr)) {
  // The sequence on which this object was constructed, and thus affine to.
  scoped_refptr<base::SequencedTaskRunner> affine_sequence =
      base::SequencedTaskRunnerHandle::Get();

  never_blocking_sequence_ = base::ThreadPool::CreateSequencedTaskRunner(
      {base::TaskPriority::USER_BLOCKING,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN});

  sequenced_state_ = std::unique_ptr<LacrosChromeServiceNeverBlockingState,
                                     base::OnTaskRunnerDeleter>(
      new LacrosChromeServiceNeverBlockingState(
          affine_sequence, weak_factory_.GetWeakPtr(), &init_params_),
      base::OnTaskRunnerDeleter(never_blocking_sequence_));
  weak_sequenced_state_ = sequenced_state_->GetWeakPtr();

  never_blocking_sequence_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &LacrosChromeServiceNeverBlockingState::BindAshChromeServiceRemote,
          weak_sequenced_state_));

  // Bind the remote for SelectFile on the current thread, and then pass the
  // receiver to the never_blocking_sequence_.
  never_blocking_sequence_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &LacrosChromeServiceNeverBlockingState::BindMessageCenterReceiver,
          weak_sequenced_state_,
          message_center_remote_.BindNewPipeAndPassReceiver()));

  // Bind the remote for SelectFile on the current thread, and then pass the
  // receiver to the never_blocking_sequence_.
  mojo::PendingReceiver<crosapi::mojom::SelectFile>
      select_file_pending_receiver =
          select_file_remote_.BindNewPipeAndPassReceiver();
  never_blocking_sequence_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &LacrosChromeServiceNeverBlockingState::BindSelectFileReceiver,
          weak_sequenced_state_, std::move(select_file_pending_receiver)));

  mojo::PendingReceiver<crosapi::mojom::Attestation>
      attestation_pending_receiver =
          attestation_remote_.BindNewPipeAndPassReceiver();
  never_blocking_sequence_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &LacrosChromeServiceNeverBlockingState::BindAttestationReceiver,
          weak_sequenced_state_, std::move(attestation_pending_receiver)));

  DCHECK(!g_instance);
  g_instance = this;
}

LacrosChromeServiceImpl::~LacrosChromeServiceImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(affine_sequence_checker_);
  DCHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

void LacrosChromeServiceImpl::BindReceiver(
    mojo::PendingReceiver<crosapi::mojom::LacrosChromeService> receiver) {
  never_blocking_sequence_->PostTask(
      FROM_HERE, base::BindOnce(&LacrosChromeServiceNeverBlockingState::
                                    BindLacrosChromeServiceReceiver,
                                weak_sequenced_state_, std::move(receiver)));
  sequenced_state_->WaitForInit();
}

void LacrosChromeServiceImpl::BindScreenManagerReceiver(
    mojo::PendingReceiver<crosapi::mojom::ScreenManager> pending_receiver) {
  never_blocking_sequence_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &LacrosChromeServiceNeverBlockingState::BindScreenManagerReceiver,
          weak_sequenced_state_, std::move(pending_receiver)));
}

void LacrosChromeServiceImpl::NewWindowAffineSequence() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(affine_sequence_checker_);
  delegate_->NewWindow();
}

}  // namespace chromeos
