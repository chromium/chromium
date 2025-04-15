// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/feature_first_run/feature_first_run_helper.h"

#include "components/constrained_window/constrained_window_views.h"
#include "components/tabs/public/tab_interface.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/dialog_model.h"
#include "ui/strings/grit/ui_strings.h"

namespace {

// Builds a DialogModel for a generic FFR dialog.
std::unique_ptr<ui::DialogModel> CreateGenericFeatureFirstRunDialogModel(
    std::u16string title) {
  return ui::DialogModel::Builder()
      .SetTitle(title)
      .AddOkButton(base::DoNothing(),
                   ui::DialogModel::Button::Params().SetLabel(
                       l10n_util::GetStringUTF16(IDS_APP_TURN_ON)))
      .AddCancelButton(base::DoNothing())
      .Build();
}

}  // namespace

namespace feature_first_run {

views::Widget* ShowFeatureFirstRunDialog(std::u16string title,
                                         content::WebContents* web_contents) {
  tabs::TabInterface* tab =
      tabs::TabInterface::MaybeGetFromContents(web_contents);
  if (!tab || !tab->CanShowModalUI()) {
    return nullptr;
  }

  return constrained_window::ShowWebModal(
      CreateGenericFeatureFirstRunDialogModel(title), web_contents);
}

}  // namespace feature_first_run
