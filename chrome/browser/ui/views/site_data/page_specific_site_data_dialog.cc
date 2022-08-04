// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/site_data/page_specific_site_data_dialog.h"

#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/dialog_model.h"

// static
views::Widget* ShowPageSpecificSiteDataDialog(
    content::WebContents* web_contents) {
  auto builder = ui::DialogModel::Builder();
  builder.SetTitle(
      l10n_util::GetStringUTF16(IDS_COLLECTED_COOKIES_DIALOG_TITLE));
  // TODO(crbug.com/1344787): Build the rest of the dialog. Add action handling.
  // Remove the dialog from WebContentsUserData when destroyed.
  return constrained_window::ShowWebModal(builder.Build(), web_contents);
}
