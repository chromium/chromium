// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_LACROS_LACROS_CHROME_SERVICE_IMPL_NEVER_BLOCKING_STATE_H_
#define CHROMEOS_LACROS_LACROS_CHROME_SERVICE_IMPL_NEVER_BLOCKING_STATE_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/sequenced_task_runner.h"
#include "chromeos/lacros/lacros_chrome_service_impl.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace chromeos {

// This class that holds all state associated with LacrosChromeService that is
// affine to a single, never-blocking sequence. The sequence must be
// never-blocking to avoid deadlocks, see https://crbug.com/1103765.
//
// This class is considered an implementation detail of LacrosChromeServiceImpl.
// It exists as a standalone class/file because template member functions must
// be defined in header files.
class LacrosChromeServiceImplNeverBlockingState
    : public crosapi::mojom::BrowserService {
 public:
  LacrosChromeServiceImplNeverBlockingState(
      scoped_refptr<base::SequencedTaskRunner> owner_sequence,
      base::WeakPtr<LacrosChromeServiceImpl> owner);
  ~LacrosChromeServiceImplNeverBlockingState() override;

  // crosapi::mojom::BrowserService:
  void REMOVED_2(crosapi::mojom::BrowserInitParamsPtr) override;
  void RequestCrosapiReceiver(RequestCrosapiReceiverCallback callback) override;
  void NewWindow(bool incognito, NewWindowCallback callback) override;
  void NewTab(NewTabCallback callback) override;
  void RestoreTab(RestoreTabCallback callback) override;
  void GetFeedbackData(GetFeedbackDataCallback callback) override;
  void GetHistograms(GetHistogramsCallback callback) override;
  void GetActiveTabUrl(GetActiveTabUrlCallback callback) override;
  void UpdateDeviceAccountPolicy(const std::vector<uint8_t>& policy) override;

  // Crosapi is the interface that lacros-chrome uses to message
  // ash-chrome. This method binds the remote, which allows queuing of message
  // to ash-chrome. The messages will not go through until
  // RequestCrosapiReceiver() is invoked.
  void BindCrosapi();

  // BrowserService is the interface that ash-chrome uses to message
  // lacros-chrome. This handles and routes all incoming messages from
  // ash-chrome.
  void BindBrowserServiceReceiver(
      mojo::PendingReceiver<crosapi::mojom::BrowserService> receiver);

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

  base::WeakPtr<LacrosChromeServiceImplNeverBlockingState> GetWeakPtr();

 private:
  // Receives and routes messages from ash-chrome.
  mojo::Receiver<crosapi::mojom::BrowserService> receiver_{this};

  // This remote allows lacros-chrome to send messages to ash-chrome.
  mojo::Remote<crosapi::mojom::Crosapi> crosapi_;

  mojo::Remote<crosapi::mojom::BrowserServiceHost> browser_service_host_;

  // This class holds onto the receiver for Crosapi until ash-chrome
  // is ready to bind it.
  mojo::PendingReceiver<crosapi::mojom::Crosapi> pending_crosapi_receiver_;

  // This allows LacrosChromeServiceImplNeverBlockingState to route IPC messages
  // back to the affine thread on LacrosChromeServiceImpl. |owner_| is affine to
  // |owner_sequence_|.
  scoped_refptr<base::SequencedTaskRunner> owner_sequence_;
  base::WeakPtr<LacrosChromeServiceImpl> owner_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<LacrosChromeServiceImplNeverBlockingState> weak_factory_{
      this};
};

}  // namespace chromeos

#endif  // CHROMEOS_LACROS_LACROS_CHROME_SERVICE_IMPL_NEVER_BLOCKING_STATE_H_
