// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/feature_showcase/password_manager_handler.h"

#include <utility>

#include "base/logging.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/toolbar/pinned_toolbar/pinned_toolbar_actions_model.h"

PasswordManagerHandler::PasswordManagerHandler(
    mojo::PendingReceiver<feature_showcase::mojom::PasswordManagerPageHandler>
        receiver,
    Profile* profile)
    : receiver_(this, std::move(receiver)), profile_(profile) {}

PasswordManagerHandler::~PasswordManagerHandler() = default;

void PasswordManagerHandler::PinPasswordManager() {
  PinnedToolbarActionsModel* model = PinnedToolbarActionsModel::Get(profile_);
  if (model) {
    model->UpdatePinnedState(kActionShowPasswordsBubbleOrPage,
                             /*should_pin=*/true);
  }
}
