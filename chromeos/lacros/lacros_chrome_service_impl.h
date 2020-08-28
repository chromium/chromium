// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_LACROS_LACROS_CHROME_SERVICE_IMPL_H_
#define CHROMEOS_LACROS_LACROS_CHROME_SERVICE_IMPL_H_

#include <memory>

#include "base/component_export.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/sequenced_task_runner.h"
#include "chromeos/crosapi/mojom/attestation.mojom.h"
#include "chromeos/crosapi/mojom/crosapi.mojom.h"
#include "chromeos/crosapi/mojom/message_center.mojom.h"
#include "chromeos/crosapi/mojom/screen_manager.mojom.h"
#include "chromeos/crosapi/mojom/select_file.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace chromeos {

class LacrosChromeServiceDelegate;

// Forward declaration for class defined in .cc file that holds most of the
// business logic of this class.
class LacrosChromeServiceNeverBlockingState;

// This class is responsible for receiving and routing mojo messages from
// ash-chrome via the mojo::Receiver |sequenced_state_.receiver_|. This class is
// responsible for sending and routing messages to ash-chrome via the
// mojo::Remote |sequenced_state_.ash_chrome_service_|. Messages are sent and
// received on a dedicated, never-blocking sequence to avoid deadlocks.
//
// This object is constructed, destroyed, and mostly used on an "affine
// sequence". For most intents and purposes, this is the main/UI thread.
//
// This class is a singleton but is not thread safe. Each method is individually
// documented with threading requirements.
class COMPONENT_EXPORT(CHROMEOS_LACROS) LacrosChromeServiceImpl {
 public:
  // The getter is safe to call from all threads.
  //
  // This method returns nullptr very early or late in the application
  // lifecycle. We've chosen to have precise constructor/destructor timings
  // rather than rely on a lazy initializer and no destructor to allow for
  // more precise testing.
  //
  // If this is accessed on a thread other than the affine sequence, the caller
  // must invalidate or destroy the pointer before shutdown. Attempting to use
  // this pointer during shutdown can result in UaF.
  static LacrosChromeServiceImpl* Get();

  // This class is expected to be constructed and destroyed on the same
  // sequence.
  explicit LacrosChromeServiceImpl(
      std::unique_ptr<LacrosChromeServiceDelegate> delegate);
  ~LacrosChromeServiceImpl();

  // This can be called on any thread. This call allows LacrosChromeServiceImpl
  // to start receiving messages from ash-chrome.
  void BindReceiver(
      mojo::PendingReceiver<crosapi::mojom::LacrosChromeService> receiver);

  // This must be called on the affine sequence.
  mojo::Remote<crosapi::mojom::MessageCenter>& message_center_remote() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(affine_sequence_checker_);
    return message_center_remote_;
  }

  // This must be called on the affine sequence. It exposes a remote that can
  // be used to show a select-file dialog.
  mojo::Remote<crosapi::mojom::SelectFile>& select_file_remote() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(affine_sequence_checker_);
    return select_file_remote_;
  }

  // This must be called on the affine sequence. It exposes a remote that can
  // be used to perform attestation on challenges.
  mojo::Remote<crosapi::mojom::Attestation>& attestation_remote() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(affine_sequence_checker_);
    return attestation_remote_;
  }

  // This may be called on any thread.
  void BindScreenManagerReceiver(
      mojo::PendingReceiver<crosapi::mojom::ScreenManager> pending_receiver);

  const crosapi::mojom::LacrosInitParams* init_params() const {
    return init_params_.get();
  }

 private:
  // LacrosChromeServiceNeverBlockingState is an implementation detail of this
  // class.
  friend class LacrosChromeServiceNeverBlockingState;

  // Creates a new window on the affine sequence.
  void NewWindowAffineSequence();

  // Delegate instance to inject Chrome dependent code. Must only be used on the
  // affine sequence.
  std::unique_ptr<LacrosChromeServiceDelegate> delegate_;

  // Parameters passed from ash-chrome.
  crosapi::mojom::LacrosInitParamsPtr init_params_;

  // This member allows lacros-chrome to use the SelectFile interface. This
  // member is affine to the affine sequence. It is initialized in the
  // constructor and it is immediately available for use.
  mojo::Remote<crosapi::mojom::MessageCenter> message_center_remote_;
  mojo::Remote<crosapi::mojom::SelectFile> select_file_remote_;

  // This member allows lacros-chrome to use the Attestation interface. This
  // member is affine to the affine sequence. It is initialized in the
  // constructor and it is immediately available for use.
  mojo::Remote<crosapi::mojom::Attestation> attestation_remote_;

  // This member is instantiated on the affine sequence alongside the
  // constructor. All subsequent invocations of this member, including
  // destruction, happen on the |never_blocking_sequence_|.
  std::unique_ptr<LacrosChromeServiceNeverBlockingState,
                  base::OnTaskRunnerDeleter>
      sequenced_state_;

  // This member is instantiated on the affine sequence, but only ever
  // dereferenced on the |never_blocking_sequence_|.
  base::WeakPtr<LacrosChromeServiceNeverBlockingState> weak_sequenced_state_;

  // A sequence that is guaranteed to never block.
  scoped_refptr<base::SequencedTaskRunner> never_blocking_sequence_;

  // Checks that the method is called on the affine sequence.
  SEQUENCE_CHECKER(affine_sequence_checker_);

  base::WeakPtrFactory<LacrosChromeServiceImpl> weak_factory_{this};
};

}  // namespace chromeos

#endif  // CHROMEOS_LACROS_LACROS_CHROME_SERVICE_IMPL_H_
