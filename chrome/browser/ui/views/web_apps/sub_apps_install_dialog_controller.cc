// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/web_apps/sub_apps_install_dialog_controller.h"

#include <optional>
#include <utility>

#include "base/auto_reset.h"
#include "base/check.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"

namespace web_app {

namespace {

std::optional<SubAppsInstallDialogController::DialogActionForTesting>
    g_dialog_override_for_testing;

}  // namespace

// static
[[nodiscard]] base::AutoReset<
    std::optional<SubAppsInstallDialogController::DialogActionForTesting>>
SubAppsInstallDialogController::SetAutomaticActionForTesting(  // IN-TEST
    DialogActionForTesting auto_accept) {
  return base::AutoReset<std::optional<DialogActionForTesting>>(
      &g_dialog_override_for_testing, auto_accept);
}

// static
bool SubAppsInstallDialogController::
    HandleAutomaticActionForTesting(  // IN-TEST
        base::OnceCallback<void(bool)>& callback) {
  if (!g_dialog_override_for_testing) {
    return false;
  }

  switch (g_dialog_override_for_testing.value()) {
    case DialogActionForTesting::kAccept:
      std::move(callback).Run(true);
      break;
    case DialogActionForTesting::kCancel:
      std::move(callback).Run(false);
      break;
  }
  return true;
}

SubAppsInstallDialogController::SubAppsInstallDialogController(
    base::OnceCallback<void(bool)> callback)
    : callback_(std::move(callback)) {}

SubAppsInstallDialogController::~SubAppsInstallDialogController() = default;

void SubAppsInstallDialogController::OnAccept() {
  CHECK(callback_);
  std::move(callback_).Run(true);
}

void SubAppsInstallDialogController::OnClose() {
  // Note `OnClose` is called on dialog cancel, dismiss and on dialog destroy,
  // so `callback_` can be null when this is called.
  if (!callback_) {
    return;
  }
  std::move(callback_).Run(false);
}

base::WeakPtr<SubAppsInstallDialogController>
SubAppsInstallDialogController::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace web_app
