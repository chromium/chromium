// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/set_time/set_time_dialog.h"

#include <string>

#include "base/metrics/user_metrics.h"
#include "chrome/common/url_constants.h"
#include "chromeos/ash/components/login/login_state/login_state.h"
#include "ui/gfx/geometry/size.h"

namespace ash {

namespace {

// Dialog width and height in DIPs.
const int kDefaultWidth = 530;
const int kDefaultHeightWithTimezone = 396;
const int kDefaultHeightWithoutTimezone = 258;

}  // namespace

// static
void SetTimeDialog::ShowDialog(gfx::NativeWindow parent) {
  base::RecordAction(base::UserMetricsAction("Options_SetTimeDialog_Show"));
  auto* dialog = new SetTimeDialog();
  dialog->ShowSystemDialog(parent);
}

// static
bool SetTimeDialog::ShouldShowTimezone() {
  // After login the user should set the timezone via Settings, which applies
  // additional restrictions.
  return !LoginState::Get()->IsUserLoggedIn();
}

SetTimeDialog::SetTimeDialog()
    : SystemWebDialogDelegate(GURL(chrome::kChromeUISetTimeURL),
                              std::u16string() /* title */) {}

SetTimeDialog::~SetTimeDialog() = default;

void SetTimeDialog::GetDialogSize(gfx::Size* size) const {
  size->SetSize(kDefaultWidth, ShouldShowTimezone()
                                   ? kDefaultHeightWithTimezone
                                   : kDefaultHeightWithoutTimezone);
}

}  // namespace ash
