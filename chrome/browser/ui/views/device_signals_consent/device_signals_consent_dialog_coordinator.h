// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_DEVICE_SIGNALS_CONSENT_DEVICE_SIGNALS_CONSENT_DIALOG_COORDINATOR_H_
#define CHROME_BROWSER_UI_VIEWS_DEVICE_SIGNALS_CONSENT_DEVICE_SIGNALS_CONSENT_DIALOG_COORDINATOR_H_

#include "ui/views/widget/widget.h"

class Browser;

// Controller that displays the modal dialog for collecting user consent for
// sharing device signals.
class DeviceSignalsConsentDialogCoordinator {
 public:
  DeviceSignalsConsentDialogCoordinator() = default;
  ~DeviceSignalsConsentDialogCoordinator() = default;

  DeviceSignalsConsentDialogCoordinator(
      const DeviceSignalsConsentDialogCoordinator&) = delete;
  DeviceSignalsConsentDialogCoordinator& operator=(
      const DeviceSignalsConsentDialogCoordinator&) = delete;

  static views::Widget* ShowDialog(Browser* browser);
};

#endif  // CHROME_BROWSER_UI_VIEWS_DEVICE_SIGNALS_CONSENT_DEVICE_SIGNALS_CONSENT_DIALOG_COORDINATOR_H_
