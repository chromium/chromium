// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/office_fallback/office_fallback_page_handler.h"

namespace ash::office_fallback {

OfficeFallbackPageHandler::OfficeFallbackPageHandler(
    mojo::PendingReceiver<mojom::PageHandler> pending_page_handler,
    CloseCallback callback)
    : receiver_{this, std::move(pending_page_handler)},
      callback_{std::move(callback)} {}

OfficeFallbackPageHandler::~OfficeFallbackPageHandler() = default;

void OfficeFallbackPageHandler::Close(mojom::DialogChoice choice) {
  if (callback_) {
    std::move(callback_).Run(choice);
  }
}

}  // namespace ash::office_fallback
