// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_LACROS_LACROS_SERVICE_NEVER_BLOCKING_STATE_H_
#define CHROMEOS_LACROS_LACROS_SERVICE_NEVER_BLOCKING_STATE_H_

#include <utility>

#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "chromeos/crosapi/mojom/crosapi.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace chromeos {

// This class that holds all state associated with LacrosChromeService that is
// affine to a single, never-blocking sequence. The sequence must be
// never-blocking to avoid deadlocks, see https://crbug.com/1103765.
//
// This class is considered an implementation detail of LacrosService.
// It exists as a standalone class/file because template member functions must
// be defined in header files.
class LacrosServiceNeverBlockingState {
 public:
  LacrosServiceNeverBlockingState();
  ~LacrosServiceNeverBlockingState();

  // Crosapi is the interface that lacros-chrome uses to message
  // ash-chrome. This method binds the remote, which allows queuing of message
  // to ash-chrome. The messages will not go through until FusePipeCrosapi() is
  // invoked.
  void BindCrosapi();

  void FusePipeCrosapi(
      mojo::PendingRemote<crosapi::mojom::Crosapi> pending_remote);

  void OnBrowserStartup(crosapi::mojom::BrowserInfoPtr browser_info);

  // Calls the indicated Bind* function on the crosapi interface with the given
  // receiver.
  template <typename ReceiverType,
            void (crosapi::mojom::Crosapi::*bind_func)(ReceiverType)>
  void BindCrosapiFeatureReceiver(ReceiverType receiver) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    (crosapi_.get()->*bind_func)(std::move(receiver));
  }

  base::WeakPtr<LacrosServiceNeverBlockingState> GetWeakPtr();

 private:
  // This remote allows lacros-chrome to send messages to ash-chrome.
  mojo::Remote<crosapi::mojom::Crosapi> crosapi_;

  // This class holds onto the receiver for Crosapi until ash-chrome
  // is ready to bind it.
  mojo::PendingReceiver<crosapi::mojom::Crosapi> pending_crosapi_receiver_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<LacrosServiceNeverBlockingState> weak_factory_{this};
};

}  // namespace chromeos

#endif  // CHROMEOS_LACROS_LACROS_SERVICE_NEVER_BLOCKING_STATE_H_
