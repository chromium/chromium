// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/search_engines/dse_reset_dialog.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/views/frame/app_menu_button.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/grit/branded_strings.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/default_search_manager.h"
#include "components/search_engines/search_engines_pref_names.h"
#include "components/search_engines/search_engines_switches.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/common/referrer.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/dialog_model.h"
#include "ui/base/window_open_disposition.h"
#include "ui/views/bubble/bubble_dialog_model_host.h"
#include "ui/views/controls/link.h"
#include "url/gurl.h"

namespace search_engines {

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
namespace {

const char kDefaultSearchEngineResetNotificationShown[] =
    "Search.DefaultSearchEngineResetNotificationShown";

void OpenLearnMoreLink(Browser* browser, const ui::Event& event) {
  const GURL kLearnMoreUrl(
      "https://support.google.com/chrome/answer/"
      "3296214#zippy=%2Cchrome-reset-my-browser-settings");
  browser->OpenURL(
      content::OpenURLParams(kLearnMoreUrl, content::Referrer(),
                             WindowOpenDisposition::NEW_FOREGROUND_TAB,
                             ui::PAGE_TRANSITION_LINK, false),
      {});
}

void ShowSearchEngineResetNotification(Browser* browser) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  if (!browser_view) {
    return;
  }
  base::UmaHistogramBoolean(kDefaultSearchEngineResetNotificationShown, true);

  views::View* anchor_view =
      browser_view->toolbar_button_provider()->GetAppMenuButton();

  auto bubble_delegate_unique = std::make_unique<ui::DialogModelDelegate>();

  ui::DialogModel::Builder dialog_builder(std::move(bubble_delegate_unique));

  dialog_builder
      .SetTitle(l10n_util::GetStringUTF16(
          IDS_DEFAULT_SEARCH_ENGINE_RESET_NOTIFICATION_TITLE))
      .AddParagraph(ui::DialogModelLabel(
                        l10n_util::GetStringUTF16(
                            IDS_DEFAULT_SEARCH_ENGINE_RESET_NOTIFICATION_BODY))
                        .set_is_secondary())
      .AddExtraButton(
          base::BindRepeating(&OpenLearnMoreLink, browser),
          ui::DialogModel::Button::Params().SetLabel(l10n_util::GetStringUTF16(
              IDS_DEFAULT_SEARCH_ENGINE_RESET_NOTIFICATION_LEARN_MORE_BUTTON)))
      .AddOkButton(
          base::DoNothing(),
          ui::DialogModel::Button::Params()
              .SetLabel(l10n_util::GetStringUTF16(
                  IDS_DEFAULT_SEARCH_ENGINE_RESET_NOTIFICATION_GOT_IT_BUTTON))
              .SetStyle(ui::ButtonStyle::kProminent))
      .DisableCloseOnDeactivate()
      .SetIsAlertDialog();

  auto bubble = std::make_unique<views::BubbleDialogModelHost>(
      dialog_builder.Build(), anchor_view, views::BubbleBorder::TOP_RIGHT);

  views::BubbleDialogDelegate::CreateBubble(std::move(bubble))->Show();

  // Don't show this notification again.
  browser->profile()->GetPrefs()->SetBoolean(
      prefs::kShowDefaultSearchEngineResetNotification, false);
}

}  // namespace
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)

void MaybeShowSearchEngineResetNotification(
    Browser* browser,
    AutocompleteMatch::Type match_type) {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  // Ensure it is a non-navigation search query.
  if (!AutocompleteMatch::IsSearchType(match_type)) {
    return;
  }

  if (!base::FeatureList::IsEnabled(
          switches::kResetTamperedDefaultSearchEngine)) {
    return;
  }

  Profile* profile = browser->profile();
  TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile);
  if (!template_url_service) {
    return;
  }

  if (!profile->GetPrefs()->GetBoolean(
          prefs::kShowDefaultSearchEngineResetNotification)) {
    return;
  }

  // Don't show notification if default search engine is disabled by policy.
  if (!template_url_service->GetDefaultSearchProvider() &&
      template_url_service->default_search_provider_source() ==
          DefaultSearchManager::FROM_POLICY) {
    profile->GetPrefs()->SetBoolean(
        prefs::kShowDefaultSearchEngineResetNotification, false);
    return;
  }

  ShowSearchEngineResetNotification(browser);
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
}

}  // namespace search_engines
