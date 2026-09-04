// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/omnibox_everywhere/omnibox_everywhere_ui.h"

#include "base/feature_list.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/autocomplete/aim_eligibility_service_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/contextual_search/contextual_search_service_factory.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/enterprise/util/managed_browser_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/omnibox/omnibox_everywhere/omnibox_everywhere_prefs.h"
#include "chrome/browser/ui/omnibox/omnibox_everywhere_service.h"
#include "chrome/browser/ui/omnibox/omnibox_everywhere_service_factory.h"
#include "chrome/browser/ui/omnibox/omnibox_next_features.h"
#include "chrome/browser/ui/search/most_visited_metrics_logger.h"
#include "chrome/browser/ui/views/location_bar/omnibox_popup_file_selector.h"
#include "chrome/browser/ui/views/omnibox/omnibox_context_menu.h"
#include "chrome/browser/ui/views/user_education/browser_help_bubble.h"
#include "chrome/browser/ui/webui/cr_components/most_visited/most_visited_handler.h"
#include "chrome/browser/ui/webui/cr_components/most_visited/most_visited_pref_observer.h"
#include "chrome/browser/ui/webui/cr_components/searchbox/searchbox_handler.h"
#include "chrome/browser/ui/webui/favicon_source.h"
#include "chrome/browser/ui/webui/metrics_reporter/metrics_reporter_service.h"
#include "chrome/browser/ui/webui/new_tab_page/composebox/variations/composebox_fieldtrial.h"
#include "chrome/browser/ui/webui/omnibox_everywhere/composebox_everywhere_handler.h"
#include "chrome/browser/ui/webui/omnibox_everywhere/debug/omnibox_everywhere_debug_page_handler.h"
#include "chrome/browser/ui/webui/omnibox_everywhere/omnibox_everywhere_handler.h"
#include "chrome/browser/ui/webui/omnibox_everywhere/omnibox_everywhere_page_handler.h"
#include "chrome/browser/ui/webui/plural_string_handler.h"
#include "chrome/browser/ui/webui/sanitized_image/sanitized_image_source.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/omnibox_everywhere_resources.h"
#include "chrome/grit/omnibox_everywhere_resources_map.h"
#include "components/contextual_search/contextual_search_metrics_recorder.h"
#include "components/contextual_search/contextual_search_service.h"
#include "components/favicon_base/favicon_url_parser.h"
#include "components/lens/lens_features.h"
#include "components/omnibox/browser/aim_eligibility_service.h"
#include "components/omnibox/browser/omnibox_pref_names.h"
#include "components/omnibox/common/composebox_features.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/search/ntp_features.h"
#include "components/strings/grit/components_strings.h"
#include "components/user_education/webui/help_bubble_handler.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/base/mojom/menu_source_type.mojom.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/submenu_view.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/webui/tracked_element/tracked_element_handler_document_singleton.h"
#include "ui/webui/webui_util.h"

namespace {

enum ScreenshotMenuCommand {
  kScreenshotEntireScreen = 1,
  kScreenshotWindow,
  kScreenshotRegion,
};

// Minimum preferred width for the screenshot Views menu, matching UX specs
// and the previous dropdown implementation (320px).
constexpr int kScreenshotMenuWidth = 320;
bool IsAimEligible(Profile* profile) {
  auto* aim_eligibility_service =
      AimEligibilityServiceFactory::GetForProfile(profile);
  return aim_eligibility_service && aim_eligibility_service->IsAimEligible();
}

bool IsFuseboxEligible(Profile* profile) {
  return IsAimEligible(profile) &&
         AimEligibilityServiceFactory::GetForProfile(profile)
             ->IsFuseboxEligible();
}

bool IsFuseboxEnabled(Profile* profile) {
  const bool show_ai_mode =
      !profile || !profile->GetPrefs() ||
      profile->GetPrefs()->GetBoolean(
          omnibox_everywhere::prefs::kOmniboxEverywhereShowAiMode);
  return IsFuseboxEligible(profile) && show_ai_mode;
}

class OmniboxEverywhereMostVisitedPrefObserver
    : public MostVisitedPrefObserver {
 public:
  OmniboxEverywhereMostVisitedPrefObserver(Profile* profile,
                                           MostVisitedHandler* handler)
      : MostVisitedPrefObserver(profile, handler) {
    if (g_browser_process && g_browser_process->local_state()) {
      local_state_pref_change_registrar_.Init(g_browser_process->local_state());
      local_state_pref_change_registrar_.Add(
          omnibox_everywhere::prefs::kOmniboxEverywhereShowShortcuts,
          base::BindRepeating(&OmniboxEverywhereMostVisitedPrefObserver::
                                  OnTilesVisibilityPrefChanged,
                              base::Unretained(this)));
    }
    OnTilesVisibilityPrefChanged();
  }

 protected:
  bool IsShortcutsVisible() const override {
    return omnibox_everywhere::prefs::IsOmniboxEverywhereShortcutsVisible(
        profile_,
        g_browser_process ? g_browser_process->local_state() : nullptr);
  }

  void OnTileTypesChanged() override {
    MostVisitedPrefObserver::OnTileTypesChanged();
    OnTilesVisibilityPrefChanged();
  }

 private:
  PrefChangeRegistrar local_state_pref_change_registrar_;
};

void AddMostVisitedSourceStrings(content::WebUIDataSource* source,
                                 Profile* profile) {
  source->AddBoolean("omniboxEverywhereMostVisitedEnabled",
                     omnibox::kOmniboxEverywhereMostVisitedParam.Get());
  source->AddBoolean(
      "omniboxEverywhereShowShortcuts",
      omnibox_everywhere::prefs::IsOmniboxEverywhereShortcutsVisible(
          profile, g_browser_process->local_state()));

  static constexpr webui::LocalizedString kMostVisitedStrings[] = {
      {"addLinkTitle", IDS_NTP_CUSTOM_LINKS_ADD_SHORTCUT_TITLE},
      {"editLinkTitle", IDS_NTP_CUSTOM_LINKS_EDIT_SHORTCUT},
      {"viewLinkTitle", IDS_NTP_CUSTOM_LINKS_SHORTCUT_DETAILS_TITLE},
      {"invalidUrl", IDS_NTP_CUSTOM_LINKS_INVALID_URL},
      {"linkAddedMsg", IDS_NTP_CONFIRM_MSG_SHORTCUT_ADDED},
      {"linkCancel", IDS_NTP_CUSTOM_LINKS_CANCEL},
      {"linkCantCreate", IDS_NTP_CUSTOM_LINKS_CANT_CREATE},
      {"linkCantEdit", IDS_NTP_CUSTOM_LINKS_CANT_EDIT},
      {"viewLink", IDS_NTP_CUSTOM_LINKS_DETAILS},
      {"linkDone", IDS_NTP_CUSTOM_LINKS_DONE},
      {"linkEditedMsg", IDS_NTP_CONFIRM_MSG_SHORTCUT_EDITED},
      {"linkRemove", IDS_NTP_CUSTOM_LINKS_REMOVE},
      {"linkRemoveA11y", IDS_NTP_MOST_VISITED_SITES_REMOVE},
      {"linkRemovedMsg", IDS_NTP_CONFIRM_MSG_SHORTCUT_REMOVED},
      {"shortcutMoreActions", IDS_NTP_CUSTOM_LINKS_MORE_ACTIONS},
      {"enterpriseShortcutSubtitle", IDS_NTP_ENTERPRISE_SHORTCUT_SUBTITLE},
      {"nameField", IDS_NTP_CUSTOM_LINKS_NAME},
      {"restoreDefaultLinks", IDS_NTP_CONFIRM_MSG_RESTORE_DEFAULTS},
      {"restoreDefaultEnterpriseShortcuts",
       IDS_NTP_CONFIRM_MSG_RESTORE_ENTERPRISE_DEFAULTS},
      {"restoreThumbnailsShort", IDS_NEW_TAB_RESTORE_THUMBNAILS_SHORT_LINK},
      {"shortcutAlreadyExists", IDS_NTP_CUSTOM_LINKS_ALREADY_EXISTS},
      {"urlField", IDS_NTP_CUSTOM_LINKS_URL},
      {"showMore", IDS_NTP_SHOW_MORE_BUTTON_LABEL},
      {"showLess", IDS_NTP_SHOW_LESS_BUTTON_LABEL},
      {"shortcutsInactivityRemovalMsg",
       IDS_NTP_MOST_VISITED_SHORTCUTS_INACTIVITY_REMOVAL},
      {"moduleInactivityRemovalMsg", IDS_NTP_MODULE_INACTIVITY_REMOVAL},
      {"modulesInactivityRemovalMsg", IDS_NTP_MODULES_INACTIVITY_REMOVAL},
      {"undo", IDS_NEW_TAB_UNDO_THUMBNAIL_REMOVE},
  };
  source->AddLocalizedStrings(kMostVisitedStrings);

  ui::Accelerator undo_accelerator(ui::VKEY_Z, ui::EF_PLATFORM_ACCELERATOR);
  source->AddString("undoDescription", l10n_util::GetStringFUTF16(
                                           IDS_UNDO_DESCRIPTION,
                                           undo_accelerator.GetShortcutText()));

  source->AddInteger("maxTilesInCollapsedState",
                     ntp_features::GetMaxTilesInCollapsedState());
  source->AddInteger("maxShortcutsInExpandedState",
                     ntp_features::GetMaxShortcutsInExpandedState());
  source->AddInteger("maxMostVisitedTilesInExpandedState",
                     ntp_features::GetMaxMostVisitedTilesInExpandedState());
  source->AddInteger("maxEnterpriseShortcuts",
                     ntp_features::GetMaxEnterpriseShortcuts());
  source->AddInteger("preconnectStartTimeThreshold", 0);
  source->AddInteger("prefetchStartTimeThreshold", 0);
  source->AddBoolean("prefetchTriggerEnabled", false);
  source->AddBoolean("prerenderOnPressEnabled", false);
  source->AddBoolean("mostVisitedHighDpiFaviconsEnabled", true);
}

}  // namespace

bool OmniboxEverywhereUIConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  Profile* profile = Profile::FromBrowserContext(browser_context);
  return omnibox::IsOmniboxEverywhereEnabled(profile);
}

bool OmniboxEverywhereUIConfig::ShouldCrashOnJavascriptErrorInDevelopmentBuild()
    const {
  return true;
}

OmniboxEverywhereUI::OmniboxEverywhereUI(content::WebUI* web_ui)
    : TopChromeWebUIController(web_ui,
                               /*enable_chrome_send=*/true,
                               /*enable_chrome_histograms=*/true),
      profile_(Profile::FromWebUI(web_ui)) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      profile_, chrome::kChromeUIOmniboxEverywhereHost);

  webui::SetupWebUIDataSource(source, kOmniboxEverywhereResources,
                              IDR_OMNIBOX_EVERYWHERE_OMNIBOX_EVERYWHERE_HTML);
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::MediaSrc,
      "media-src blob: data: 'self';");
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ImgSrc,
      "img-src 'self' chrome://resources chrome://theme chrome://favicon/ "
      "chrome://favicon2/ chrome://image/ data: blob:;");

  std::string profile_avatar_url =
      "chrome://theme/IDR_PROFILE_AVATAR_PLACEHOLDER_LARGE";
  if (g_browser_process && g_browser_process->profile_manager()) {
    ProfileAttributesEntry* entry =
        g_browser_process->profile_manager()
            ->GetProfileAttributesStorage()
            .GetProfileAttributesWithPath(profile_->GetPath());
    if (entry) {
      gfx::Image icon =
          profiles::GetSizedAvatarIcon(entry->GetAvatarIcon(), 48, 48);
      profile_avatar_url = webui::GetBitmapDataUrl(icon.AsBitmap());

      std::u16string gaia_name = entry->GetGAIANameToDisplay();
      std::u16string local_name = entry->GetLocalProfileName();
      std::u16string display_name;
      if (!gaia_name.empty() && !local_name.empty() &&
          gaia_name != local_name) {
        display_name = gaia_name + u" (" + local_name + u")";
      } else {
        display_name = entry->GetName();
      }
      source->AddString("profileName", base::UTF16ToUTF8(display_name));
      source->AddString("profileEmail",
                        base::UTF16ToUTF8(entry->GetUserName()));
    } else {
      source->AddString("profileName", "");
      source->AddString("profileEmail", "");
    }
  }
  source->AddString("profileAvatarUrl", profile_avatar_url);
  source->AddBoolean("omniboxEverywhereProfilePickerEnabled",
                     omnibox::kOmniboxEverywhereProfilePickerParam.Get());
  bool is_enterprise_profile =
      enterprise_util::CanShowEnterpriseBadgingForAvatar(profile_);
  source->AddBoolean("isEnterpriseProfile", is_enterprise_profile);
  static constexpr webui::LocalizedString kStrings[] = {
      {"loomniboxFreAcceptHotkey", IDS_LOOMNIBOX_FRE_ACCEPT_HOTKEY},
      {"loomniboxFreCloseButtonAria", IDS_LOOMNIBOX_FRE_CLOSE_BUTTON_ARIA},
      {"loomniboxFreEditOwn", IDS_LOOMNIBOX_FRE_KEYBOARD_OPTION_EDIT_OWN},
      {"loomniboxFreKeyboardBadgeOption",
       IDS_LOOMNIBOX_FRE_KEYBOARD_BADGE_OPTION},
      {"loomniboxFreKeyboardBadgeSpace",
       IDS_LOOMNIBOX_FRE_KEYBOARD_BADGE_SPACE},
      {"loomniboxFreKeyboardPrimary", IDS_LOOMNIBOX_FRE_KEYBOARD_PRIMARY},
      {"loomniboxFreLensPrimary", IDS_LOOMNIBOX_FRE_LENS_PRIMARY},
      {"loomniboxFreLensSecondary", IDS_LOOMNIBOX_FRE_LENS_SECONDARY},
      {"loomniboxFreOr", IDS_LOOMNIBOX_FRE_OR},
      {"loomniboxFreTitle", IDS_LOOMNIBOX_FRE_TITLE},
      {"managedByYourOrganization", IDS_MANAGED},
      {"profileButtonLabel", IDS_OVERFLOW_MENU_ITEM_TEXT_PROFILE},
      {"screenshotEntireScreenLabel", IDS_OMNIBOX_EVERYWHERE_ENTIRE_SCREEN},
      {"screenshotRegionLabel", IDS_OMNIBOX_EVERYWHERE_REGION},
      {"screenshotWindowLabel", IDS_OMNIBOX_EVERYWHERE_WINDOW},
      {"searchBoxHintAskOrType", IDS_NTP_SEARCH_BOX_PLACEHOLDER_ASK_OR_TYPE},
      {"shareScreenshotLabel", IDS_OMNIBOX_EVERYWHERE_SHARE_SCREENSHOT},
  };
  source->AddLocalizedStrings(kStrings);

  bool initial_show_fre =
      base::FeatureList::IsEnabled(omnibox::kOmniboxEverywhereFre) &&
      !profile_->GetPrefs()->GetBoolean(
          omnibox_everywhere::prefs::kFreDismissed) &&
      (profile_->GetPrefs()->GetInteger(
           omnibox_everywhere::prefs::kFreImpressionCount) <
       omnibox_everywhere::prefs::kMaxFreImpressions);
  source->AddBoolean("initialShowFre", initial_show_fre);

  // Sanitized image and favicon source initialization
  content::URLDataSource::Add(profile_,
                              std::make_unique<SanitizedImageSource>(profile_));
  content::URLDataSource::Add(
      profile_, std::make_unique<FaviconSource>(
                    profile_, chrome::FaviconUrlFormat::kFavicon2));

  bool session_allows_drag_and_drop = false;
  if (auto* session_handle = GetOrCreateContextualSessionHandle()) {
    session_allows_drag_and_drop =
        session_handle->CheckSearchContentSharingSettings(profile_->GetPrefs());
  }

  // Configure WebUIDataSource dictionary
  source->AddLocalizedStrings(SearchboxHandler::GetWebUIDataSourceDict(
      profile_,
      {.enable_voice_search = true,
       .enable_lens_search = true,
       .session_allows_drag_and_drop = session_allows_drag_and_drop}));

  source->AddBoolean("isTopChromeSearchbox", false);
  source->AddBoolean(
      "omniboxPopupDebugEnabled",
      base::FeatureList::IsEnabled(omnibox::kWebUIOmniboxPopupDebug));

  source->AddBoolean("reportMetrics", true);
  source->AddString("charTypedToPaintMetricName",
                    "Omnibox.WebUI.CharTypedToRepaintLatency.ToPaint");
  source->AddString(
      "resultChangedToPaintMetricName",
      "Omnibox.Popup.WebUI.ResultChangedToRepaintLatency.ToPaint");

  // Add composebox data.
  auto composebox_config = omnibox::FeatureConfig::Get().config.composebox();
  const std::string attachment_mime_types =
      composebox_config.attachment_upload().mime_types_allowed();
  source->AddString("composeboxAttachmentFileTypes", attachment_mime_types);
  source->AddInteger("composeboxFileMaxSize",
                     composebox_config.attachment_upload().max_size_bytes());
  const std::string image_mime_types =
      composebox_config.image_upload().mime_types_allowed();
  source->AddString("composeboxImageFileTypes", image_mime_types);
  source->AddBoolean("lensSendRawFileMediaTypesEnabled",
                     lens::features::IsLensSendRawFileMediaTypesEnabled());
  source->AddBoolean(
      "caretAnimationEnabled",
      base::FeatureList::IsEnabled(omnibox::kOmniboxAnimatedCaret));
  source->AddBoolean("composeboxContextMenuEnableMultiTabSelection",
                     omnibox::kContextMenuEnableMultiTabSelection.Get());
  source->AddBoolean("composeboxShowContextMenu",
                     omnibox::kShowContextMenu.Get());
  source->AddBoolean(
      "composeboxShowContextMenuDescription",
      omnibox::kShowContextMenuDescription.Get() &&
          omnibox::kWebUIOmniboxAimPopupAddContextButtonVariantParam.Get() !=
              omnibox::AddContextButtonVariant::kInline);
  source->AddBoolean("composeboxShowContextMenuTabPreviews",
                     omnibox::kShowContextMenuTabPreviews.Get());
  source->AddBoolean("composeboxShowImageSuggest",
                     omnibox::kShowComposeboxImageSuggestions.Get());

  AddMostVisitedSourceStrings(source, profile_);

  const bool is_fusebox_enabled = IsFuseboxEnabled(profile_);
  source->AddBoolean("searchboxShowComposeEntrypoint", is_fusebox_enabled);
  source->AddBoolean("isFuseboxEnabled", is_fusebox_enabled);
  source->AddBoolean(
      "ntpRealboxDynamicAiModeButton",
      is_fusebox_enabled && base::FeatureList::IsEnabled(
                                ntp_realbox::kNtpRealboxDynamicAiModeButton));
  source->AddBoolean("composeboxShowTypedSuggest",
                     omnibox::kShowComposeboxTypedSuggest.Get());
  source->AddBoolean("composeboxShowZps", omnibox::kShowComposeboxZps.Get());
  source->AddBoolean("composeboxSmartComposeEnabled",
                     omnibox::kShowSmartCompose.Get());
  source->AddBoolean("webuiOmniboxSimplificationEnabled",
                     base::FeatureList::IsEnabled(
                         omnibox::internal::kWebUIOmniboxSimplification));
  source->AddBoolean(
      "contextManagementInComposeboxEnabled",
      base::FeatureList::IsEnabled(omnibox::kContextManagementInComposebox) &&
          base::FeatureList::IsEnabled(omnibox::kContextManagementInOmnibox));
  source->AddBoolean(
      "tabFaviconChipsToCoinsEnabled",
      base::FeatureList::IsEnabled(omnibox::kContextManagementInComposebox) &&
          base::FeatureList::IsEnabled(omnibox::kContextManagementInOmnibox) &&
          base::FeatureList::IsEnabled(omnibox::kTabFaviconChipsToCoins));
  source->AddBoolean(
      "composeboxSkillsEnabled",
      base::FeatureList::IsEnabled(omnibox::kComposeboxSkillsOmniboxEverywhere));

  source->AddString("searchboxLayoutMode", "TallBottomContext");
  source->AddString(
      "composeboxSource",
      contextual_search::ContextualSearchMetricsRecorder::
          ContextualSearchSourceToString(
              contextual_search::ContextualSearchSource::kOmniboxEverywhere));
  source->AddBoolean("caretColorAnimationDisabled",
                     base::FeatureList::IsEnabled(
                         omnibox::kWebUIOmniboxDisableCaretColorAnimation));
  source->AddBoolean("composeboxAnimationDisabled",
                     base::FeatureList::IsEnabled(
                         omnibox::kWebUIOmniboxAimPopupDisableAnimation));
  // Disable the energy effect for the searchbox in Omnibox Everywhere so the
  // AIM compose button renders the outer conic rainbow glow animation instead
  // of the energy effect. The composebox explicitly enables energy effect for
  // its own expanding glow animation.
  source->AddBoolean("energyEffectEnabled", false);
  source->AddBoolean("energyEffectAnimationEnabled", false);
  source->AddBoolean("composeboxEnergyEffectAnimationEnabled", true);
  source->AddBoolean("contextButtonShapeIsOblong",
                     omnibox::kContextButtonShapeIsOblong.Get());

  // Add a handler to provide pluralized strings.
  auto plural_string_handler = std::make_unique<PluralStringHandler>();
  plural_string_handler->AddLocalizedString("sharingTabs",
                                            IDS_COMPOSE_SHARING_TABS);
  web_ui->AddMessageHandler(std::move(plural_string_handler));

  ui::TrackedElementHandlerDocumentSingleton::Register(
      this, {kOmniboxEverywhereLensButtonElementId});

  ForceWebUIHelpBubbles::CreateForWebContents(web_ui->GetWebContents());
  if (auto* forced =
          ForceWebUIHelpBubbles::FromWebContents(web_ui->GetWebContents())) {
    forced->SetForceWebUIForAnchors({kOmniboxEverywhereLensButtonElementId});
  }
}

OmniboxEverywhereUI::~OmniboxEverywhereUI() = default;

void OmniboxEverywhereUI::BindInterface(
    mojo::PendingReceiver<composebox::mojom::PageHandlerFactory> receiver) {
  if (!omnibox::IsOmniboxEverywhereEnabled(profile_)) {
    return;
  }
  if (composebox_page_factory_receiver_.is_bound()) {
    composebox_page_factory_receiver_.reset();
  }
  composebox_page_factory_receiver_.Bind(std::move(receiver));
}

void OmniboxEverywhereUI::CreatePageHandler(
    mojo::PendingReceiver<composebox::mojom::PageHandler> pending_page_handler,
    mojo::PendingRemote<searchbox::mojom::Page> pending_searchbox_page,
    mojo::PendingReceiver<searchbox::mojom::PageHandler>
        pending_searchbox_handler) {
  // TODO(crbug.com/526629960): Create new EverywhereComposeboxHandler or allow
  // the ComposeboxHandler to parameterize the OmniboxClient.
  composebox_handler_ = std::make_unique<ComposeboxEverywhereHandler>(
      std::move(pending_page_handler), std::move(pending_searchbox_handler),
      std::move(pending_searchbox_page), profile_, web_ui()->GetWebContents(),
      base::BindRepeating(
          &OmniboxEverywhereUI::GetOrCreateContextualSessionHandle,
          base::Unretained(this)),
      base::BindRepeating(&OmniboxEverywhereUI::ClearContextualSessionHandle,
                          base::Unretained(this)),
      this);
  composebox_handler_->set_disconnect_handler(
      base::BindOnce(&OmniboxEverywhereUI::OnComposeboxHandlerDisconnected,
                     weak_factory_.GetWeakPtr()));

  for (const auto& pending : pending_upload_statuses_) {
    composebox_handler_->OnContextualInputStatusChanged(
        pending.token, pending.status, pending.error_type);
  }
  pending_upload_statuses_.clear();
}

void OmniboxEverywhereUI::BindInterface(
    content::RenderFrameHost* host,
    mojo::PendingReceiver<searchbox::mojom::PageHandlerFactory>
        pending_page_handler) {
  if (!omnibox::IsOmniboxEverywhereEnabled(profile_)) {
    return;
  }
  if (searchbox_page_factory_receiver_.is_bound()) {
    searchbox_page_factory_receiver_.reset();
  }
  searchbox_page_factory_receiver_.Bind(std::move(pending_page_handler));
}

void OmniboxEverywhereUI::CreatePageHandler(
    mojo::PendingRemote<searchbox::mojom::Page> page,
    mojo::PendingReceiver<searchbox::mojom::PageHandler> pending_page_handler) {
  auto* service = OmniboxEverywhereServiceFactory::GetForProfile(profile_);
  CHECK(service);

  MetricsReporterService* metrics_reporter_service =
      MetricsReporterService::GetFromWebContents(web_ui()->GetWebContents());
  omnibox_handler_ = std::make_unique<OmniboxEverywhereHandler>(
      std::move(pending_page_handler), std::move(page),
      metrics_reporter_service->metrics_reporter(), web_ui(), service,
      base::BindRepeating(
          &OmniboxEverywhereUI::GetOrCreateContextualSessionHandle,
          base::Unretained(this)),
      this);
}

void OmniboxEverywhereUI::BindInterface(
    mojo::PendingReceiver<omnibox_everywhere::mojom::PageHandlerFactory>
        receiver) {
  if (!omnibox::IsOmniboxEverywhereEnabled(profile_)) {
    return;
  }
  if (page_factory_receiver_.is_bound()) {
    page_factory_receiver_.reset();
  }
  page_factory_receiver_.Bind(std::move(receiver));
}

void OmniboxEverywhereUI::CreatePageHandler(
    mojo::PendingRemote<omnibox_everywhere::mojom::Page> pending_page,
    mojo::PendingReceiver<omnibox_everywhere::mojom::PageHandler>
        pending_page_handler) {
  page_handler_ = std::make_unique<OmniboxEverywherePageHandler>(
      std::move(pending_page_handler), std::move(pending_page), this);
}

void OmniboxEverywhereUI::OnScreensharePickerOpened() {
  if (auto* service =
          OmniboxEverywhereServiceFactory::GetForProfile(profile_)) {
    service->OnScreensharePickerOpened();
  }
}

void OmniboxEverywhereUI::OnScreensharePickerClosed() {
  if (auto* service =
          OmniboxEverywhereServiceFactory::GetForProfile(profile_)) {
    service->OnScreensharePickerClosed();
  }
}

void OmniboxEverywhereUI::ShowRegionSelectOverlay(
    const SkBitmap& screenshot,
    const RegionCaptureSource& source,
    RegionSelectedCallback callback) {
  if (auto* service =
          OmniboxEverywhereServiceFactory::GetForProfile(profile_)) {
    service->ShowRegionSelectOverlay(screenshot, source, std::move(callback));
    return;
  }
  std::move(callback).Run(SkBitmap());
}

void OmniboxEverywhereUI::BindInterface(
    mojo::PendingReceiver<omnibox_everywhere_debug::mojom::PageHandlerFactory>
        receiver) {
  if (debug_page_factory_receiver_.is_bound()) {
    debug_page_factory_receiver_.reset();
  }
  debug_page_factory_receiver_.Bind(std::move(receiver));
}

void OmniboxEverywhereUI::CreatePageHandler(
    mojo::PendingRemote<omnibox_everywhere_debug::mojom::Page> page,
    mojo::PendingReceiver<omnibox_everywhere_debug::mojom::PageHandler>
        handler) {
  debug_page_handler_ = std::make_unique<
      omnibox_everywhere_debug::OmniboxEverywhereDebugPageHandler>(
      web_ui(), profile_, std::move(page), std::move(handler));
}

void OmniboxEverywhereUI::BindInterface(
    mojo::PendingReceiver<most_visited::mojom::MostVisitedPageHandlerFactory>
        receiver) {
  if (!base::FeatureList::IsEnabled(omnibox::kOmniboxEverywhere)) {
    return;
  }
  if (most_visited_page_factory_receiver_.is_bound()) {
    most_visited_page_factory_receiver_.reset();
  }
  most_visited_page_factory_receiver_.Bind(std::move(receiver));
}

void OmniboxEverywhereUI::CreatePageHandler(
    mojo::PendingRemote<most_visited::mojom::MostVisitedPage> pending_page,
    mojo::PendingReceiver<most_visited::mojom::MostVisitedPageHandler>
        pending_page_handler) {
  most_visited_handler_ = std::make_unique<MostVisitedHandler>(
      std::move(pending_page_handler), std::move(pending_page), profile_,
      web_ui()->GetWebContents(),
      std::make_unique<MostVisitedMetricsLogger>("Omnibox"));
  most_visited_pref_observer_ =
      std::make_unique<OmniboxEverywhereMostVisitedPrefObserver>(
          profile_, most_visited_handler_.get());
}

void OmniboxEverywhereUI::BindInterface(
    mojo::PendingReceiver<help_bubble::mojom::HelpBubbleHandlerFactory>
        receiver) {
  if (help_bubble_handler_factory_receiver_.is_bound()) {
    help_bubble_handler_factory_receiver_.reset();
  }
  help_bubble_handler_factory_receiver_.Bind(std::move(receiver));
}

void OmniboxEverywhereUI::CreateHelpBubbleHandler(
    mojo::PendingRemote<help_bubble::mojom::HelpBubbleClient> client,
    mojo::PendingReceiver<help_bubble::mojom::HelpBubbleHandler> handler) {
  help_bubble_handler_ = std::make_unique<user_education::HelpBubbleHandler>(
      std::move(handler), std::move(client),
      ui::TrackedElementHandlerDocumentSingleton::GetOrCreate(
          web_ui()->GetRenderFrameHost()));
}

ContextualSearchboxHandler*
OmniboxEverywhereUI::GetContextualSearchboxHandler() {
  if (is_composebox_mode_ && composebox_handler_) {
    return composebox_handler_.get();
  }
  return omnibox_handler_.get();
}

contextual_search::ContextualSearchSessionHandle*
OmniboxEverywhereUI::GetOrCreateContextualSessionHandle() {
  if (!shared_session_handle_) {
    auto* contextual_search_service =
        ContextualSearchServiceFactory::GetForProfile(profile_);
    if (contextual_search_service) {
      shared_session_handle_ = contextual_search_service->CreateSession(
          omnibox::CreateQueryControllerConfigParams(),
          contextual_search::ContextualSearchSource::kOmniboxEverywhere,
          lens::LensOverlayInvocationSource::kOmniboxEverywhereComposebox);
      shared_session_handle_->CheckSearchContentSharingSettings(
          profile_->GetPrefs());
    }
  }
  return shared_session_handle_.get();
}

void OmniboxEverywhereUI::set_is_composebox_mode(bool mode) {
  is_composebox_mode_ = mode;
}

void OmniboxEverywhereUI::ClearContextualSessionHandle() {
  shared_session_handle_.reset();
  pending_upload_statuses_.clear();
  set_is_composebox_mode(false);

  // OmniboxEverywhereUI concurrently hosts both `omnibox_handler_` and
  // `composebox_handler_` across a persistent WebContents.
  // Because `selected_tabs` and `input_state_model_` are owned directly by each
  // ContextualSearchboxHandler rather than the shared session handle, we must
  // explicitly reset both handlers to prevent stale tab mappings or input
  // models from leaking across subsequent queries and mode switches.
  if (omnibox_handler_) {
    omnibox_handler_->selected_tabs.clear();
    omnibox_handler_->ResetInputStateModel();
  }
  if (composebox_handler_) {
    composebox_handler_->selected_tabs.clear();
    composebox_handler_->ResetInputStateModel();
  }
}

// Shows a native Views menu rather than a WebUI <cr-action-menu> so that the
// menu:
// 1. Is not clipped by the Omnibox Everywhere WebUI popup widget bounds.
// 2. Can intelligently flip and position across monitor/screen boundaries.
// 3. Matches native TopChrome menu styling, accessibility, and keyboard
//    traversal.
void OmniboxEverywhereUI::ShowScreenshotMenu(
    const gfx::Rect& anchor_rect,
    base::WeakPtr<ContextualSearchboxScreenshareController> controller) {
  if (screenshot_menu_runner_ && screenshot_menu_runner_->IsRunning()) {
    if (controller) {
      controller->OnScreenshotMenuClosed();
    }
    return;
  }
  content::WebContents* web_contents = web_ui()->GetWebContents();
  if (!web_contents) {
    if (controller) {
      controller->OnScreenshotMenuClosed();
    }
    return;
  }
  views::Widget* widget = views::Widget::GetWidgetForNativeWindow(
      web_contents->GetTopLevelNativeWindow());
  if (!widget || !widget->GetContentsView()) {
    if (controller) {
      controller->OnScreenshotMenuClosed();
    }
    return;
  }

  active_screenshot_controller_ = std::move(controller);

  screenshot_menu_model_ = std::make_unique<ui::SimpleMenuModel>(this);
  screenshot_menu_model_->AddTitle(
      l10n_util::GetStringUTF16(IDS_OMNIBOX_EVERYWHERE_SHARE_SCREENSHOT));
  screenshot_menu_model_->AddItemWithIcon(
      kScreenshotEntireScreen,
      l10n_util::GetStringUTF16(IDS_OMNIBOX_EVERYWHERE_ENTIRE_SCREEN),
      ui::ImageModel::FromVectorIcon(vector_icons::kScreenShareIcon,
                                     ui::kColorMenuIcon, 16));
  screenshot_menu_model_->AddItemWithIcon(
      kScreenshotWindow,
      l10n_util::GetStringUTF16(IDS_OMNIBOX_EVERYWHERE_WINDOW),
      ui::ImageModel::FromVectorIcon(kDesktopWindowsIcon, ui::kColorMenuIcon,
                                     16));
  screenshot_menu_model_->AddItemWithIcon(
      kScreenshotRegion,
      l10n_util::GetStringUTF16(IDS_OMNIBOX_EVERYWHERE_REGION),
      ui::ImageModel::FromVectorIcon(vector_icons::kCropFreeIcon,
                                     ui::kColorMenuIcon, 16));

  menu_model_adapter_ = std::make_unique<views::MenuModelAdapter>(
      screenshot_menu_model_.get(),
      base::BindRepeating(&OmniboxEverywhereUI::OnScreenshotMenuClosed,
                          weak_factory_.GetWeakPtr()));
  std::unique_ptr<views::MenuItemView> menu = menu_model_adapter_->CreateMenu();
  if (menu && menu->HasSubmenu()) {
    menu->GetSubmenu()->set_minimum_preferred_width(kScreenshotMenuWidth);
  }

  screenshot_menu_runner_ = std::make_unique<views::MenuRunner>(
      std::move(menu),
      views::MenuRunner::HAS_MNEMONICS | views::MenuRunner::CONTEXT_MENU);

  gfx::Rect screen_rect =
      anchor_rect + web_contents->GetContainerBounds().OffsetFromOrigin();

  screenshot_menu_runner_->RunMenuAt(widget, nullptr, screen_rect,
                                     views::MenuAnchorPosition::kTopLeft,
                                     ui::mojom::MenuSourceType::kNone);
}

void OmniboxEverywhereUI::OnScreenshotMenuClosed() {
  if (active_screenshot_controller_) {
    active_screenshot_controller_->OnScreenshotMenuClosed();
  }
}

void OmniboxEverywhereUI::ExecuteCommand(int command_id, int event_flags) {
  if (!active_screenshot_controller_) {
    return;
  }
  auto controller = std::move(active_screenshot_controller_);
  controller->OnScreenshotMenuClosed();
  switch (command_id) {
    case kScreenshotEntireScreen:
      controller->StartScreenshare(
          /*prefer_entire_screen=*/true, base::DoNothing());
      break;
    case kScreenshotWindow:
      controller->StartScreenshare(
          /*prefer_entire_screen=*/false, base::DoNothing());
      break;
    case kScreenshotRegion:
      controller->CaptureRegionScreenshot(base::DoNothing());
      break;
  }
}

bool OmniboxEverywhereUI::IsCommandIdChecked(int command_id) const {
  return false;
}

bool OmniboxEverywhereUI::IsCommandIdEnabled(int command_id) const {
  return true;
}

bool OmniboxEverywhereUI::IsCommandIdVisible(int command_id) const {
  return true;
}

void OmniboxEverywhereUI::OpenComposebox(
    omnibox_everywhere::mojom::ComposeboxInitialStatePtr initial_state) {
  set_is_composebox_mode(true);
  if (page_handler_) {
    page_handler_->OpenComposebox(std::move(initial_state));
  }
}

void OmniboxEverywhereUI::OnComposeboxHandlerDisconnected() {
  composebox_handler_.reset();
  ClearContextualSessionHandle();
}

void OmniboxEverywhereUI::AddFileContext(
    const base::UnguessableToken& token,
    searchbox::mojom::SelectedFileInfoPtr file_info) {
  if (is_composebox_mode_ && composebox_handler_) {
    composebox_handler_->AddFileContextFromBrowser(token, std::move(file_info));
  } else {
    auto initial_state =
        omnibox_everywhere::mojom::ComposeboxInitialState::New();
    initial_state->file_token = token;
    initial_state->file_info = std::move(file_info);
    OpenComposebox(std::move(initial_state));
  }
}

void OmniboxEverywhereUI::OnContextualInputStatusChanged(
    const base::UnguessableToken& token,
    contextual_search::ContextUploadStatus status,
    std::optional<contextual_search::ContextUploadErrorType> error_type) {
  if (is_composebox_mode_ && composebox_handler_) {
    composebox_handler_->OnContextualInputStatusChanged(token, status,
                                                        error_type);
  } else {
    pending_upload_statuses_.push_back({token, status, error_type});
  }
}

void OmniboxEverywhereUI::OnContextMenuClosed() {
  if (page_handler_) {
    page_handler_->OnContextMenuClosed();
  }
}

void OmniboxEverywhereUI::OnFileChooserOpened() {
  if (auto* service =
          OmniboxEverywhereServiceFactory::GetForProfile(profile_)) {
    service->OnFileChooserOpened();
  }
}

void OmniboxEverywhereUI::OnFileChooserClosed() {
  if (auto* service =
          OmniboxEverywhereServiceFactory::GetForProfile(profile_)) {
    service->OnFileChooserClosed();
  }
}

void OmniboxEverywhereUI::ShowContextActionMenu(const gfx::Rect& anchor_rect) {
  if (context_menu_) {
    context_menu_->Cancel();
  }
  content::WebContents* web_contents = web_ui()->GetWebContents();
  if (!web_contents) {
    OnContextMenuClosed();
    return;
  }
  views::Widget* widget = views::Widget::GetWidgetForNativeWindow(
      web_contents->GetTopLevelNativeWindow());
  if (!widget || !widget->GetContentsView()) {
    OnContextMenuClosed();
    return;
  }

  if (!file_selector_) {
    file_selector_ = std::make_unique<OmniboxPopupFileSelector>(
        web_contents->GetTopLevelNativeWindow());
    file_selector_->set_open_ai_mode_callback(base::BindRepeating(
        &OmniboxEverywhereUI::OpenComposebox, weak_factory_.GetWeakPtr(),
        /*initial_state=*/nullptr));
    file_selector_->set_file_chooser_opened_callback(base::BindRepeating(
        &OmniboxEverywhereUI::OnFileChooserOpened, weak_factory_.GetWeakPtr()));
    file_selector_->set_file_chooser_closed_callback(base::BindRepeating(
        &OmniboxEverywhereUI::OnFileChooserClosed, weak_factory_.GetWeakPtr()));
  }

  // `anchor_rect` is the bounding box of the '+' entrypoint button in WebUI
  // viewport coordinates (CSS DIPs relative to the top-left of the
  // WebContents). We offset it by `GetContainerBounds().OffsetFromOrigin()`
  // (the screen position of the WebContents) to convert
  // `anchor_rect.bottom_left()` into desktop screen DIP coordinates expected by
  // Views MenuRunner.
  gfx::Point screen_point =
      anchor_rect.bottom_left() +
      web_contents->GetContainerBounds().OffsetFromOrigin();

  context_menu_ = std::make_unique<OmniboxContextMenu>(
      widget, file_selector_.get(), web_contents,
      base::BindRepeating(&OmniboxEverywhereUI::OnContextMenuClosed,
                          weak_factory_.GetWeakPtr()));
  context_menu_->RunMenuAt(screen_point, ui::mojom::MenuSourceType::kNone);
}

WEB_UI_CONTROLLER_TYPE_IMPL(OmniboxEverywhereUI)
