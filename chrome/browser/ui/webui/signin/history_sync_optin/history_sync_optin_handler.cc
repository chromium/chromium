// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/history_sync_optin/history_sync_optin_handler.h"

#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "chrome/browser/ui/browser.h"

HistorySyncOptinHandler::HistorySyncOptinHandler(
    mojo::PendingReceiver<history_sync_optin::mojom::PageHandler> receiver,
    mojo::PendingRemote<history_sync_optin::mojom::Page> page)
    : receiver_(this, std::move(receiver)), page_(std::move(page)) {}

HistorySyncOptinHandler::~HistorySyncOptinHandler() = default;
