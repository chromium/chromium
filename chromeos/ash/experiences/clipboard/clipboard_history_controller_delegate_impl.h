// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_EXPERIENCES_CLIPBOARD_CLIPBOARD_HISTORY_CONTROLLER_DELEGATE_IMPL_H_
#define CHROMEOS_ASH_EXPERIENCES_CLIPBOARD_CLIPBOARD_HISTORY_CONTROLLER_DELEGATE_IMPL_H_

#include "ash/clipboard/clipboard_history_controller_delegate.h"

// The browser-implemented delegate of the `ClipboardHistoryControllerImpl`.
class ClipboardHistoryControllerDelegateImpl
    : public ash::ClipboardHistoryControllerDelegate {
 public:
  ClipboardHistoryControllerDelegateImpl();
  ClipboardHistoryControllerDelegateImpl(
      const ClipboardHistoryControllerDelegateImpl&) = delete;
  ClipboardHistoryControllerDelegateImpl& operator=(
      const ClipboardHistoryControllerDelegateImpl&) = delete;
  ~ClipboardHistoryControllerDelegateImpl() override;

 private:
  // ash::ClipboardHistoryControllerDelegate:
  bool Paste() const override;
};

#endif  // CHROMEOS_ASH_EXPERIENCES_CLIPBOARD_CLIPBOARD_HISTORY_CONTROLLER_DELEGATE_IMPL_H_
