// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/feature_first_run/autofill_ai_first_run_dialog.h"

#include "build/branding_buildflags.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/feature_first_run/feature_first_run_helper.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/controls/rich_controls_container_view.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/autofill/content/browser/content_autofill_client.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/browser/permissions/autofill_ai/autofill_ai_permission_utils.h"
#include "components/autofill_ai/core/browser/autofill_ai_metrics.h"
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

void OnLearnMoreClicked(content::WebContents* web_contents) {
  autofill_ai::LogOptInFunnelEvent(
      autofill_ai::AutofillAiOptInFunnelEvents::kFFRLearnMoreButtonClicked);
  Browser* browser = chrome::FindBrowserWithTab(web_contents);
  chrome::ShowSettingsSubPage(browser, chrome::kAutofillAiSubPage);
}

void OnDialogAccepted(content::WebContents* web_contents) {
  autofill_ai::LogOptInFunnelEvent(
      autofill_ai::AutofillAiOptInFunnelEvents::kFFRDialogAccepted);
  autofill::AutofillClient* client =
      autofill::ContentAutofillClient::FromWebContents(web_contents);

  autofill::SetAutofillAiOptInStatus(*client, true);
}

void OnDialogCancelled() {
  // Do nothing.
}

std::unique_ptr<views::View> CreateDialogContentView(
    content::WebContents* web_contents) {
  auto container_view = CreateDialogContentViewContainer();

  container_view->AddChildView(CreateInfoBoxContainer(
      l10n_util::GetStringUTF16(IDS_AUTOFILL_AI_FFR_WHEN_ON_TITLE),
      l10n_util::GetStringUTF16(IDS_AUTOFILL_AI_FFR_WHEN_ON_DESCRIPTION),
      kTextAnalysisIcon, InfoBoxPosition::kStart));

  container_view->AddChildView(CreateInfoBoxContainerWithLearnMore(
      l10n_util::GetStringUTF16(IDS_SETTINGS_COLUMN_HEADING_CONSIDER),
      IDS_AUTOFILL_AI_FFR_CONSIDER_DESCRIPTION,
      l10n_util::GetStringUTF16(IDS_AUTOFILL_AI_FFR_CONSIDER_LEARN_MORE),
      base::BindRepeating(&OnLearnMoreClicked, web_contents), kGoogleGLogoIcon,
      InfoBoxPosition::kEnd));

  return container_view;
}

}  // namespace

void ShowAutofillAiFirstRunDialog(content::WebContents* web_contents) {
  autofill_ai::LogOptInFunnelEvent(
      autofill_ai::AutofillAiOptInFunnelEvents::kFFRDialogShown);
  ShowFeatureFirstRunDialog(
      l10n_util::GetStringUTF16(IDS_AUTOFILL_AI_OPT_IN_IPH_TITLE),
      ui::ImageModel::FromResourceId(IDR_AUTOFILL_AI_FFR_BANNER),
      ui::ImageModel::FromResourceId(IDR_AUTOFILL_AI_FFR_BANNER_DARK),
      CreateDialogContentView(web_contents),
      base::BindOnce(&OnDialogAccepted, web_contents),
      base::BindOnce(&OnDialogCancelled), web_contents);
}

}  // namespace feature_first_run
