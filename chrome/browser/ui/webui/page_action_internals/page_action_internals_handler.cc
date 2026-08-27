// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/page_action_internals/page_action_internals_handler.h"

PageActionInternalsHandler::PageActionInternalsHandler(
    mojo::PendingReceiver<page_action_internals::mojom::PageHandler> receiver)
    : receiver_(this, std::move(receiver)) {}

PageActionInternalsHandler::~PageActionInternalsHandler() = default;
