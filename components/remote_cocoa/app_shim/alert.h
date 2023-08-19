// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REMOTE_COCOA_APP_SHIM_ALERT_H_
#define COMPONENTS_REMOTE_COCOA_APP_SHIM_ALERT_H_

#import <Cocoa/Cocoa.h>

#include "components/remote_cocoa/app_shim/remote_cocoa_app_shim_export.h"
#include "components/remote_cocoa/common/alert.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/gfx/text_elider.h"

@class AlertBridgeHelper;

namespace remote_cocoa {

// Class that displays an NSAlert with associated UI as described by the mojo
// AlertBridge interface.
class REMOTE_COCOA_APP_SHIM_EXPORT AlertBridge
    : public remote_cocoa::mojom::AlertBridge {
 public:
  // Creates a new alert which controls its own lifetime. It will destroy itself
  // once its NSAlert goes away.
  explicit AlertBridge(
      mojo::PendingReceiver<mojom::AlertBridge> bridge_receiver);

  AlertBridge(const AlertBridge&) = delete;
  AlertBridge& operator=(const AlertBridge&) = delete;

  // Send the specified disposition via the Show callback, then destroy |this|.
  void SendResultAndDestroy(mojom::AlertDisposition disposition);

  // Called by Cocoa to indicate when the NSAlert is visible (and can be
  // programmatically updated by Accept, Cancel, and Close).
  void SetAlertHasShown();

  // Called by Cocoa to indicate that the alert can be closed and callbacks can
  // be discarded.
  void Dismiss() override;

 private:
  // Private destructor is called only through SendResultAndDestroy.
  ~AlertBridge() override;

  // Handle being disconnected (e.g, because the alert was programmatically
  // dismissed).
  void OnMojoDisconnect();

  // remote_cocoa::mojom::Alert:
  void Show(mojom::AlertBridgeInitParamsPtr params,
            ShowCallback callback) override;

  // The NSAlert's owner and delegate.
  AlertBridgeHelper* __strong helper_;

  // Set once the alert window is showing (needed because showing is done in a
  // posted task).
  bool alert_shown_ = false;

  // Set once the alert has been dismissed.
  bool alert_dismissed_ = false;

  // The callback to make when the dialog has finished running.
  ShowCallback callback_;

  mojo::Receiver<remote_cocoa::mojom::AlertBridge> mojo_receiver_{this};
  base::WeakPtrFactory<AlertBridge> weak_factory_;
};

}  // namespace remote_cocoa

#endif  // COMPONENTS_REMOTE_COCOA_APP_SHIM_ALERT_H_
