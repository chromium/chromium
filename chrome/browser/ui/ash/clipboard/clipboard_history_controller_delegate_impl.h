// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_CLIPBOARD_CLIPBOARD_HISTORY_CONTROLLER_DELEGATE_IMPL_H_
#define CHROME_BROWSER_UI_ASH_CLIPBOARD_CLIPBOARD_HISTORY_CONTROLLER_DELEGATE_IMPL_H_

#include <memory>

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
  std::unique_ptr<ash::ClipboardHistoryUrlTitleFetcher> CreateUrlTitleFetcher()
      const override;
  std::unique_ptr<ash::ClipboardImageModelFactory> CreateImageModelFactory()
      const override;
  bool Paste() const override;
};

#endif  // CHROME_BROWSER_UI_ASH_CLIPBOARD_CLIPBOARD_HISTORY_CONTROLLER_DELEGATE_IMPL_H_
