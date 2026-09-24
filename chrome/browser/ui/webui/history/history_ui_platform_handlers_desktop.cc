// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/history_embeddings/history_embeddings_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_restore.h"
#include "chrome/browser/signin/account_preview_data_service_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_ui_util.h"
#include "chrome/browser/ui/webui/cr_components/history_clusters/history_clusters_util.h"
#include "chrome/browser/ui/webui/history/foreign_session_handler.h"
#include "chrome/browser/ui/webui/history/history_ui_platform_handlers.h"
#include "chrome/browser/ui/webui/managed_ui_handler.h"
#include "chrome/browser/ui/webui/theme_source.h"
#include "chrome/browser/ui/webui/user_education/user_education_handler.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/history_clusters/core/history_clusters_prefs.h"
#include "components/history_embeddings/core/history_embeddings_features.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "components/sessions/core/session_types.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/window_open_disposition.h"
#include "ui/webui/resources/cr_components/history/foreign_sessions.mojom.h"
#include "ui/webui/webui_util.h"

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
#include "chrome/browser/ui/signin/account_preview_utils.h"
#include "chrome/browser/ui/webui/history/history_cross_device_signin_promo_handler.h"
#include "components/signin/core/browser/account_preview_data_service.h"
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

namespace history {

void PopulatePlatformDataSource(content::WebUIDataSource* source,
                                Profile* profile) {
  const bool is_glic_enabled =
      glic::GlicEnabling::ShouldShowSettingsPage(profile);
  auto* glic_service = glic::GlicKeyedService::Get(profile);
  const bool is_glic_web_actuation_available =
      glic::GlicEnabling::IsEnabledAndConsentForProfile(profile) &&
      glic_service && glic_service->enabling().GetUserEnabledActuationOnWeb();

  source->AddBoolean("isGlicEnabled", is_glic_enabled);
  source->AddBoolean("isGlicWebActuationAvailable",
                     is_glic_web_actuation_available);

#if BUILDFLAG(IS_CHROMEOS)
  source->AddLocalizedString("turnOnSyncButton",
                             IDS_HISTORY_TURN_ON_SYNC_BUTTON);
#else
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  bool has_primary_account =
      identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin);
  signin::AccountPreviewDataService* account_preview_data_service =
      AccountPreviewDataServiceFactory::GetForProfile(profile);
  AccountInfo account_info = signin_ui_util::GetSingleAccountForPromos(
      identity_manager, account_preview_data_service);
  std::optional<signin::AccountPreviewDataService::AccountPreviewPreference>
      preferred_account =
          account_preview_data_service
              ? account_preview_data_service->GetPreferredAccountForPromo()
              : std::nullopt;
  bool has_matching_preferred_account =
      preferred_account.has_value() &&
      preferred_account->gaia_id == account_info.GetGaiaId();

  source->AddString(
      "historySyncPromoBodySignedIn",
      has_matching_preferred_account
          ? base::UTF8ToUTF16(
                signin::GetAccountPreviewHistorySignedInPromoSubtitle(
                    *preferred_account))
          : l10n_util::GetStringFUTF16(
                IDS_HISTORY_SYNC_PROMO_BODY_SIGNED_IN,
                base::UTF8ToUTF16(account_info.GetEmail())));

  std::optional<std::string> custom_signed_out_subtitle;
  if (has_matching_preferred_account) {
    custom_signed_out_subtitle = signin::GetAccountPreviewHistoryPromoSubtitle(
        account_info.GetEmail(), *preferred_account);
  }
  if (custom_signed_out_subtitle.has_value() &&
      !custom_signed_out_subtitle->empty()) {
    source->AddString("historySyncPromoBodyWebOnlySignedIn",
                      *custom_signed_out_subtitle);
  } else {
    source->AddLocalizedString("historySyncPromoBodyWebOnlySignedIn",
                               IDS_HISTORY_SYNC_PROMO_BODY_SIGNED_OUT);
  }
  source->AddString(
      "turnOnSignedInSyncHistoryPromoBodySignInSyncOff",
      l10n_util::GetStringFUTF16(
          IDS_RECENT_TABS_SYNC_HISTORY_PROMO_BODY_SIGNED_IN_SYNC_OFF,
          base::UTF8ToUTF16(account_info.GetEmail())));
  source->AddString("accountName", account_info.GetFullName().value_or(""));
  source->AddString("accountEmail", account_info.GetEmail());
  if (!has_primary_account && !account_info.IsEmpty()) {
    source->AddString(
        "turnOnSyncButton",
        l10n_util::GetStringFUTF16(
            IDS_PROFILES_DICE_WEB_ONLY_SIGNIN_BUTTON,
            base::UTF8ToUTF16(account_info.GetGivenName().value_or(
                account_info.GetEmail()))));
  } else {
    source->AddLocalizedString("turnOnSyncButton",
                               IDS_HISTORY_TURN_ON_SYNC_BUTTON);
  }
#endif  // BUILDFLAG(IS_CHROMEOS)

  bool enable_history_embeddings =
      history_embeddings::IsHistoryEmbeddingsEnabledForProfile(profile);
  source->AddBoolean("enableHistoryEmbeddings", enable_history_embeddings);
  source->AddBoolean(
      "maybeShowEmbeddingsIph",
      history_embeddings::IsHistoryEmbeddingsSettingVisible(profile) &&
          !enable_history_embeddings);

  static constexpr webui::LocalizedString kHistoryEmbeddingsStrings[] = {
      {"historyEmbeddingsPromoLabel", IDS_HISTORY_EMBEDDINGS_PROMO_LABEL},
      {"historyEmbeddingsPromoClose", IDS_HISTORY_EMBEDDINGS_PROMO_CLOSE},
      {"historyEmbeddingsPromoHeading", IDS_HISTORY_EMBEDDINGS_PROMO_HEADING},
      {"historyEmbeddingsPromoBody", IDS_HISTORY_EMBEDDINGS_PROMO_BODY},
      {"historyEmbeddingsAnswersPromoHeading",
       IDS_HISTORY_EMBEDDINGS_ANSWERS_PROMO_HEADING},
      {"historyEmbeddingsAnswersPromoBody",
       IDS_HISTORY_EMBEDDINGS_ANSWERS_PROMO_BODY},
      {"historyEmbeddingsPromoSettingsLinkText",
       IDS_HISTORY_EMBEDDIGNS_PROMO_SETTINGS_LINK_TEXT},
  };
  source->AddLocalizedStrings(kHistoryEmbeddingsStrings);

  // History clusters
  HistoryClustersUtil::PopulateSource(source, profile, /*in_side_panel=*/false);
}

void InitializePlatformHandlers(content::WebUI* web_ui,
                                content::WebUIDataSource* source) {
  Profile* profile = Profile::FromWebUI(web_ui);
  ManagedUIHandler::Initialize(web_ui, source);
  content::URLDataSource::Add(profile, std::make_unique<ThemeSource>(profile));
}

void InitializePlatformRegistrars(PrefChangeRegistrar& registrar,
                                  Profile* profile,
                                  base::RepeatingClosure update_callback) {
  registrar.Init(profile->GetPrefs());
  registrar.Add(history_clusters::prefs::kVisible, std::move(update_callback));
}

void UpdatePlatformDataSource(content::WebUI* web_ui) {
  Profile* profile = Profile::FromWebUI(web_ui);

  base::DictValue update;
  const bool is_managed = profile->GetPrefs()->IsManagedPreference(
      history_clusters::prefs::kVisible);
  update.Set(
      kIsHistoryClustersVisibleKey,
      profile->GetPrefs()->GetBoolean(history_clusters::prefs::kVisible) ||
          !is_managed);

  content::WebUIDataSource::Update(profile, chrome::kChromeUIHistoryHost,
                                   std::move(update));
}

std::unique_ptr<history::mojom::ForeignSessionPageHandler>
CreateForeignSessionPageHandler(
    mojo::PendingRemote<history::mojom::ForeignSessionPage> page,
    mojo::PendingReceiver<history::mojom::ForeignSessionPageHandler> receiver,
    content::WebUI* web_ui) {
  return std::make_unique<browser_sync::ForeignSessionHandler>(
      std::move(receiver), std::move(page), Profile::FromWebUI(web_ui),
      web_ui->GetWebContents(),
      base::BindRepeating([](content::WebContents* source_web_contents,
                             const ::sessions::SessionTab& tab,
                             WindowOpenDisposition disposition) {
        SessionRestore::RestoreForeignSessionTab(source_web_contents, tab,
                                                 disposition);
      }),
      base::BindRepeating(
          [](Profile* profile,
             const std::vector<const ::sessions::SessionWindow*>& windows) {
            SessionRestore::RestoreForeignSessionWindows(
                profile, windows.begin(), windows.end(), base::DoNothing());
          }),
      /*side_panel_ui=*/nullptr);
}

std::unique_ptr<user_education::mojom::UserEducationMixedTrustHandler>
CreateUserEducationMixedTrustHandler(
    mojo::PendingReceiver<user_education::mojom::UserEducationMixedTrustHandler>
        receiver,
    content::WebUI* web_ui) {
  return std::make_unique<UserEducationMixedTrustHandler>(
      std::move(receiver), web_ui->GetWebContents());
}

std::unique_ptr<history_cross_device_signin_promo::mojom::
                    HistoryCrossDeviceSigninPromoHandler>
CreateCrossDeviceSigninPromoHandler(
    mojo::PendingReceiver<history_cross_device_signin_promo::mojom::
                              HistoryCrossDeviceSigninPromoHandler> receiver,
    content::WebUI* web_ui) {
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  return std::make_unique<HistoryCrossDeviceSigninPromoHandler>(
      std::move(receiver), web_ui->GetWebContents());
#else
  receiver.reset();
  return nullptr;
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)
}

}  // namespace history
