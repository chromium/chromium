// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/email_verifier/email_verified_toast_menu_model.h"

#include <utility>

#include "base/check.h"
#include "base/metrics/user_metrics.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/models/image_model.h"

namespace autofill {

EmailVerifiedToastMenuModel::EmailVerifiedToastMenuModel(
    base::RepeatingClosure open_settings_callback)
    : ui::SimpleMenuModel(this),
      open_settings_callback_(std::move(open_settings_callback)) {
  CHECK(open_settings_callback_);
  AddItemWithStringIdAndIcon(
      CommandId::kManage, IDS_MANAGE,
      ui::ImageModel::FromVectorIcon(vector_icons::kSettingsFilledIcon,
                                     ui::kColorMenuIcon, 16));
}

EmailVerifiedToastMenuModel::~EmailVerifiedToastMenuModel() = default;

void EmailVerifiedToastMenuModel::ExecuteCommand(int command_id,
                                                 int event_flags) {
  if (command_id == CommandId::kManage) {
    base::RecordAction(
        base::UserMetricsAction("Toast.EmailVerified.ManageClicked"));
    // Copy the callback to the stack because running it may synchronously
    // switch tabs and close the toast, destroying `this`.
    base::RepeatingClosure callback = open_settings_callback_;
    callback.Run();
  }
}

}  // namespace autofill
