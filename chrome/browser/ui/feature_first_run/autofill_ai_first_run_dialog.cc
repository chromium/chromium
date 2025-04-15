// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/feature_first_run/autofill_ai_first_run_dialog.h"

#include "chrome/browser/ui/feature_first_run/feature_first_run_helper.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace feature_first_run {

void ShowAutofillAiFirstRunDialog(content::WebContents* web_contents) {
  ShowFeatureFirstRunDialog(
      l10n_util::GetStringUTF16(IDS_AUTOFILL_AI_OPT_IN_IPH_TITLE),
      web_contents);
}

}  // namespace feature_first_run
