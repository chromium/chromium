// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/facilitated_payments/core/browser/pix_account_linking_manager.h"

#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "base/notreached.h"
#include "components/facilitated_payments/core/browser/facilitated_payments_client.h"

namespace payments::facilitated {

PixAccountLinkingManager::PixAccountLinkingManager(
    FacilitatedPaymentsClient* client)
    : client_(CHECK_DEREF(client)) {}

PixAccountLinkingManager::~PixAccountLinkingManager() = default;

void PixAccountLinkingManager::MaybeShowPixAccountLinkingPrompt() {
  if (!client_->IsPixAccountLinkingSupported()) {
    return;
  }

  ShowPixAccountLinkingPrompt();
}

void PixAccountLinkingManager::ShowPixAccountLinkingPrompt() {
  client_->SetUiEventListener(
      base::BindRepeating(&PixAccountLinkingManager::OnUiScreenEvent,
                          weak_ptr_factory_.GetWeakPtr()));
  client_->ShowPixAccountLinkingPrompt(
      base::BindOnce(&PixAccountLinkingManager::OnAccepted,
                     weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&PixAccountLinkingManager::OnDeclined,
                     weak_ptr_factory_.GetWeakPtr()));
}

void PixAccountLinkingManager::OnAccepted() {
  // TODO(crbug.com/419108993): Add metrics.
  client_->OnPixAccountLinkingPromptAccepted();
}

void PixAccountLinkingManager::OnDeclined() {
  // TODO(crbug.com/419108993): Add metrics.
  // TODO(crbug.com/419682918): Update pref.
}

void PixAccountLinkingManager::OnUiScreenEvent(UiEvent ui_event_type) {
  switch (ui_event_type) {
    case UiEvent::kNewScreenShown: {
      // TODO(crbug.com/419108993): Add specific logging for Pix Account Linking
      // prompt shown.
      break;
    }
    case UiEvent::kScreenClosedNotByUser: {
      // TODO(crbug.com/419108993): Add specific logging for Pix Account Linking
      // prompt closed not by user.
      break;
    }
    case UiEvent::kScreenClosedByUser: {
      // TODO(crbug.com/419108993): Add specific logging for Pix Account Linking
      // prompt closed by user.
      break;
    }
    default:
      NOTREACHED() << "Unhandled UiEvent "
                   << base::to_underlying(ui_event_type);
  }
}

}  // namespace payments::facilitated
