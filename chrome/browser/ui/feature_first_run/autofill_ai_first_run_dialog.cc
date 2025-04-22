// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/feature_first_run/autofill_ai_first_run_dialog.h"

#include "build/branding_buildflags.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/feature_first_run/feature_first_run_helper.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/controls/rich_controls_container_view.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"

namespace feature_first_run {

namespace {

const gfx::VectorIcon& kGoogleGLogoIcon =
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
    vector_icons::kGoogleGLogoMonochromeIcon;
#else
    vector_icons::kProductIcon;
#endif

// TODO(crbug.com/409520456): Open learn more link.
void OnLearnMoreClicked() {}

std::unique_ptr<views::View> CreateDialogContentView() {
  auto container_view = CreateDialogContentViewContainer();

  container_view->AddChildView(CreateInfoBoxContainer(
      l10n_util::GetStringUTF16(IDS_AUTOFILL_AI_FFR_WHEN_ON_TITLE),
      l10n_util::GetStringUTF16(IDS_AUTOFILL_AI_FFR_WHEN_ON_DESCRIPTION),
      kTextAnalysisIcon, InfoBoxPosition::kStart));

  container_view->AddChildView(CreateInfoBoxContainerWithLearnMore(
      l10n_util::GetStringUTF16(IDS_SETTINGS_COLUMN_HEADING_CONSIDER),
      IDS_AUTOFILL_AI_FFR_CONSIDER_DESCRIPTION,
      l10n_util::GetStringUTF16(IDS_AUTOFILL_AI_FFR_CONSIDER_LEARN_MORE),
      base::BindRepeating(&OnLearnMoreClicked), kGoogleGLogoIcon,
      InfoBoxPosition::kEnd));

  return container_view;
}

}  // namespace

void ShowAutofillAiFirstRunDialog(content::WebContents* web_contents) {
  ShowFeatureFirstRunDialog(
      l10n_util::GetStringUTF16(IDS_AUTOFILL_AI_OPT_IN_IPH_TITLE),
      ui::ImageModel::FromResourceId(IDR_SAVE_PASSPORT),
      ui::ImageModel::FromResourceId(IDR_SAVE_PASSPORT_DARK),
      CreateDialogContentView(), web_contents);
}

}  // namespace feature_first_run
