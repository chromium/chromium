// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/waffle/waffle_handler.h"

WaffleHandler::WaffleHandler(
    mojo::PendingReceiver<waffle::mojom::PageHandler> receiver)
    : receiver_(this, std::move(receiver)) {}

WaffleHandler::~WaffleHandler() = default;

// Triggered by closeClicked() call in TS.
void WaffleHandler::CloseClicked() {
  NOTIMPLEMENTED();
}
