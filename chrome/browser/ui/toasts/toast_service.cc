// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toasts/toast_service.h"

#include <memory>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "build/branding_buildflags.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/actor/resources/grit/actor_browser_resources.h"
#include "chrome/browser/glic/browser_ui/glic_vector_icon_manager.h"
#include "chrome/browser/indigo/indigo_page_action_controller.h"
#include "chrome/browser/indigo/resources/grit/indigo_strings.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/send_tab_to_self/send_tab_to_self_client_service.h"
#include "chrome/browser/send_tab_to_self/send_tab_to_self_client_service_factory.h"
#include "chrome/browser/skills/skills_ui_window_controller.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/commerce/commerce_ui_tab_helper.h"
#include "chrome/browser/ui/send_tab_to_self/send_tab_to_self_toolbar_icon_controller.h"
#include "chrome/browser/ui/side_panel/side_panel_entry_id.h"
#include "chrome/browser/ui/side_panel/side_panel_enums.h"
#include "chrome/browser/ui/side_panel/side_panel_ui.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/collaboration_messaging_observer.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/collaboration_messaging_observer_factory.h"
#include "chrome/browser/ui/toasts/api/toast_id.h"
#include "chrome/browser/ui/toasts/api/toast_registry.h"
#include "chrome/browser/ui/toasts/api/toast_specification.h"
#include "chrome/browser/ui/toasts/toast_controller.h"
#include "chrome/browser/ui/toasts/toast_features.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/data_sharing/public/features.h"
#include "components/omnibox/browser/vector_icons.h"
#include "components/plus_addresses/core/browser/grit/plus_addresses_strings.h"
#include "components/plus_addresses/core/common/features.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/strings/grit/components_strings.h"
#include "components/tabs/public/tab_interface.h"
#include "components/translate/core/browser/translate_manager.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/ui_base_features.h"
#include "ui/menus/simple_menu_model.h"
#include "ui/strings/grit/ui_strings.h"

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
#include "components/plus_addresses/core/browser/resources/vector_icons.h"
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

namespace {
const gfx::VectorIcon& GetTaskInProgressIcon() {
  return glic::GlicVectorIconManager::GetVectorIcon(IDR_ACTOR_AUTO_BROWSE_ICON);
}
}  // namespace

ToastService::ToastService(BrowserWindowInterface* browser_window_interface) {
  toast_registry_ = std::make_unique<ToastRegistry>();
  toast_controller_ = std::make_unique<ToastController>(
      browser_window_interface, toast_registry_.get());
  toast_controller_->Init();
  RegisterToasts(browser_window_interface);
}

ToastService::~ToastService() = default;

void ToastService::RegisterToasts(
    BrowserWindowInterface* browser_window_interface) {
  CHECK(toast_registry_->IsEmpty());

  toast_registry_->RegisterToast(
      ToastId::kLinkCopied,
      ToastSpecification::Builder(features::IsRoundedIconsEnabled()
                                      ? kLinkIcon
                                      : kLinkChromeRefreshOldIcon,
                                  IDS_LINK_COPIED_TOAST_BODY)
          .Build());

  toast_registry_->RegisterToast(
      ToastId::kImageCopied,
      ToastSpecification::Builder(features::IsRoundedIconsEnabled()
                                      ? kContentCopyIcon
                                      : kCopyMenuOldIcon,
                                  IDS_IMAGE_COPIED_TOAST_BODY)
          .Build());
  toast_registry_->RegisterToast(
      ToastId::kVideoFrameCopied,
      ToastSpecification::Builder(features::IsRoundedIconsEnabled()
                                      ? kContentCopyIcon
                                      : kCopyMenuOldIcon,
                                  IDS_VIDEO_FRAME_COPIED_TOAST_BODY)
          .Build());

  toast_registry_->RegisterToast(
      ToastId::kLinkToHighlightCopied,
      ToastSpecification::Builder(features::IsRoundedIconsEnabled()
                                      ? kLinkIcon
                                      : kLinkChromeRefreshOldIcon,
                                  IDS_LINK_COPIED_TO_HIGHLIGHT_TOAST_BODY)
          .Build());

  toast_registry_->RegisterToast(
      ToastId::kAddedToReadingList,
      ToastSpecification::Builder(features::IsRoundedIconsEnabled()
                                      ? kListAltIcon
                                      : kReadingListOldIcon,
                                  IDS_READING_LIST_TOAST_BODY)
          .AddActionButton(IDS_READING_LIST_TOAST_BUTTON,
                           base::BindRepeating(
                               [](BrowserWindowInterface* window) {
                                 window->GetFeatures().side_panel_ui()->Show(
                                     SidePanelEntryId::kReadingList,
                                     SidePanelOpenTrigger::kReadingListToast);
                               },
                               base::Unretained(browser_window_interface)))
          .AddCloseButton()
          .Build());

  toast_registry_->RegisterToast(
      ToastId::kClearBrowsingData,
      ToastSpecification::Builder(features::IsRoundedIconsEnabled()
                                      ? kDeleteIcon
                                      : kTrashCanRefreshOldIcon,
                                  IDS_CLEAR_BROWSING_DATA_TOAST_BODY)
          .Build());

  // TODO(crbug.com/357930023): This registration only partially implements the
  // non-milestone update toast for testing purposes and will need to be
  // updated.
  toast_registry_->RegisterToast(
      ToastId::kNonMilestoneUpdate,
      ToastSpecification::Builder(features::IsRoundedIconsEnabled()
                                      ? kChromeProductIcon
                                      : kBrowserLogoOldIcon,
                                  IDS_NON_MILESTONE_UPDATE_TOAST_BODY)
          .AddGlobalScoped()
          .Build());

  if (base::FeatureList::IsEnabled(
          plus_addresses::features::kPlusAddressesEnabled)) {
    toast_registry_->RegisterToast(
        ToastId::kPlusAddressOverride,
        ToastSpecification::Builder(
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
            plus_addresses::kPlusAddressLogoSmallIcon,
#else
            features::IsRoundedIconsEnabled() ? vector_icons::kMailFilledIcon
                                              : vector_icons::kEmailOldIcon,
#endif
            IDS_PLUS_ADDRESS_FULL_FORM_FILL_TOAST_MESSAGE)
            .AddMenu()
            .Build());
  }

  // ESB as a synced setting.
  if (base::FeatureList::IsEnabled(safe_browsing::kEsbAsASyncedSetting)) {
    toast_registry_->RegisterToast(
        ToastId::kSyncEsbOn,
        ToastSpecification::Builder(
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
            vector_icons::kGshieldIcon,
#else
            features::IsRoundedIconsEnabled() ? kSecurityIcon
                                              : kSecurityOldIcon,
#endif
            IDS_SETTINGS_SAFEBROWSING_ENHANCED_ON_TOAST_MESSAGE)
            .AddActionButton(
                IDS_SETTINGS_SETTINGS,
                base::BindRepeating(
                    [](BrowserWindowInterface* window) {
                      window->OpenGURL(
                          chrome::GetSettingsUrl(
                              chrome::kSafeBrowsingEnhancedProtectionSubPage),
                          WindowOpenDisposition::NEW_FOREGROUND_TAB);
                    },
                    base::Unretained(browser_window_interface)))
            .AddCloseButton()
            .Build());
    toast_registry_->RegisterToast(
        ToastId::kSyncEsbOnWithoutActionButton,
        ToastSpecification::Builder(
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
            vector_icons::kGshieldIcon,
#else
            features::IsRoundedIconsEnabled() ? kSecurityIcon
                                              : kSecurityOldIcon,
#endif
            IDS_SETTINGS_SAFEBROWSING_ENHANCED_ON_TOAST_MESSAGE)
            .Build());
    toast_registry_->RegisterToast(
        ToastId::kSyncEsbOff,
        ToastSpecification::Builder(
            features::IsRoundedIconsEnabled() ? kInfoIcon : kInfoOldIcon,
            IDS_SETTINGS_SAFEBROWSING_ENHANCED_OFF_TOAST_MESSAGE)
            .AddActionButton(
                IDS_SETTINGS_SAFEBROWSING_TURN_ON_ENHANCED_TOAST_BUTTON,
                base::BindRepeating(
                    [](BrowserWindowInterface* window) {
                      Profile* profile = window->GetProfile();
                      if (profile) {
                        profile->GetPrefs()->SetBoolean(
                            prefs::kSafeBrowsingEnhanced, true);
                      }
                    },
                    base::Unretained(browser_window_interface)))
            .AddCloseButton()
            .Build());
  }

  if (data_sharing::features::IsDataSharingFunctionalityEnabled()) {
    // Current tab has been removed from the group.
    toast_registry_->RegisterToast(
        ToastId::kTabGroupSyncTabRemoved,
        ToastSpecification::Builder(features::IsRoundedIconsEnabled()
                                        ? kAccountCircleIcon
                                    : features::IsRoundedIconsEnabled()
                                        ? vector_icons::kAccountCircleIcon
                                        : kAccountCircleChromeRefreshOldIcon,
                                    IDS_DATA_SHARING_TOAST_TAB_REMOVED)
            .AddCloseButton()
            .AddActionButton(
                IDS_DATA_SHARING_TOAST_TAB_REMOVED_ACTION,
                base::BindRepeating(
                    [](BrowserWindowInterface* window) {
                      Profile* profile = window->GetProfile();
                      auto* collaboration_messaging_observer =
                          tab_groups::CollaborationMessagingObserverFactory::
                              GetForProfile(profile);
                      if (collaboration_messaging_observer) {
                        collaboration_messaging_observer
                            ->ReopenTabForCurrentInstantMessage();
                      }
                    },
                    base::Unretained(browser_window_interface)))
            .AddGlobalScoped()
            .Build());

    // Another user has joined an open group (global)
    toast_registry_->RegisterToast(
        ToastId::kTabGroupSyncUserJoined,
        ToastSpecification::Builder(features::IsRoundedIconsEnabled()
                                        ? kAccountCircleIcon
                                    : features::IsRoundedIconsEnabled()
                                        ? vector_icons::kAccountCircleIcon
                                        : kAccountCircleChromeRefreshOldIcon,
                                    IDS_DATA_SHARING_TOAST_NEW_MEMBER)
            .AddCloseButton()
            .AddActionButton(
                IDS_DATA_SHARING_TOAST_NEW_MEMBER_ACTION,
                base::BindRepeating(
                    [](BrowserWindowInterface* window) {
                      Profile* profile = window->GetProfile();
                      auto* collaboration_messaging_observer =
                          tab_groups::CollaborationMessagingObserverFactory::
                              GetForProfile(profile);
                      if (collaboration_messaging_observer) {
                        collaboration_messaging_observer
                            ->ManageSharingForCurrentInstantMessage(window);
                      }
                    },
                    base::Unretained(browser_window_interface)))
            .AddGlobalScoped()
            .Build());

    // Profile has been removed from open group (global)
    toast_registry_->RegisterToast(
        ToastId::kTabGroupSyncRemovedFromGroup,
        ToastSpecification::Builder(features::IsRoundedIconsEnabled()
                                        ? kGroupCustomIcon
                                        : kTabGroupSharingOldIcon,
                                    IDS_DATA_SHARING_TOAST_BLOCK_LEAVE)
            .AddGlobalScoped()
            .Build());

    // The version has been updated and shared tab groups is enabled again.
    toast_registry_->RegisterToast(
        ToastId::kTabGroupSharingVersionUpToDate,
        ToastSpecification::Builder(
            features::IsRoundedIconsEnabled() ? kGroupCustomIcon
                                              : kTabGroupSharingOldIcon,
            IDS_COLLABORATION_SHARED_TAB_GROUPS_AVAILABLE_AGAIN_IPH_MESSAGE)
            .AddGlobalScoped()
            .Build());
  }

  toast_registry_->RegisterToast(
      ToastId::kClosePinnedTab,
      ToastSpecification::Builder(
          features::IsRoundedIconsEnabled() ? kKeepIcon : kKeepOldIcon,
          IDS_CLOSE_PINNED_TAB_TOAST_BODY)
          .SetToastAsActionable()
          .Build());

  if (base::FeatureList::IsEnabled(features::kGlicActorUi) &&
      features::kGlicActorUiToast.Get()) {
    toast_registry_->RegisterToast(
        ToastId::kGeminiWorkingOnTask,
        ToastSpecification::Builder(GetTaskInProgressIcon(),
                                    IDS_TASK_IN_PROGRESS_TOAST_BODY)
            .AddGlobalScoped()
            .AddCloseButton()
            .Build());
  }

  toast_registry_->RegisterToast(
      ToastId::kDiceUserMigrated,
      ToastSpecification::Builder(features::IsRoundedIconsEnabled()
                                      ? vector_icons::kCelebrationIcon
                                      : vector_icons::kCelebrationOldIcon,
                                  IDS_DICE_MIGRATION_CONFIRMATION_TOAST_MESSAGE)
          .AddCloseButton()
          .AddActionButton(IDS_DICE_MIGRATION_CONFIRMATION_TOAST_BUTTON,
                           base::BindRepeating(
                               [](BrowserWindowInterface* window) {
                                 chrome::ShowSettingsSubPageForProfile(
                                     window->GetProfile(),
                                     chrome::kSyncSetupSubPage);
                               },
                               base::Unretained(browser_window_interface)))
          .AddGlobalScoped()
          .Build());

  toast_registry_->RegisterToast(
      ToastId::kEmailVerified,
      ToastSpecification::Builder(features::IsRoundedIconsEnabled()
                                      ? vector_icons::kMarkEmailReadIcon
                                      : vector_icons::kMarkEmailReadOldIcon,
                                  IDS_EMAIL_VERIFIED)
          .AddGlobalScoped()
          .AddMenu()
          .Build());

  toast_registry_->RegisterToast(
      ToastId::kGlicShareImageFailed,
      ToastSpecification::Builder(features::IsRoundedIconsEnabled()
                                      ? vector_icons::kInfoIcon
                                      : vector_icons::kInfoRefreshOldIcon,
                                  IDS_GLIC_SHARE_IMAGE_FAILED_TOAST_BODY)
          .AddCloseButton()
          .Build());

  toast_registry_->RegisterToast(
      ToastId::kCopiedToClipboard,
      ToastSpecification::Builder(
          features::IsRoundedIconsEnabled() ? kInfoIcon : kInfoOldIcon,
          IDS_COPIED_TO_CLIPBOARD_TOAST_BODY)
          .Build());

  toast_registry_->RegisterToast(
      ToastId::kEnhancedBundledSecuritySettings,
      ToastSpecification::Builder(
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
          vector_icons::kGshieldIcon,
#else
          features::IsRoundedIconsEnabled() ? kSecurityIcon : kSecurityOldIcon,
#endif
          IDS_SETTINGS_SECURITY_BUNDLE_TOAST_FOR_USER_OPTED_INTO_ENHANCED_BUNDLE)
          .AddActionButton(
              IDS_SETTINGS_SETTINGS,
              base::BindRepeating(
                  [](BrowserWindowInterface* window) {
                    window->OpenGURL(
                        chrome::GetSettingsUrl(chrome::kSecuritySubPage),
                        WindowOpenDisposition::NEW_FOREGROUND_TAB);
                  },
                  base::Unretained(browser_window_interface)))
          .AddCloseButton()
          .AddGlobalScoped()
          .Build());

  toast_registry_->RegisterToast(
      ToastId::kSkillSaved,
      ToastSpecification::Builder(
          features::IsRoundedIconsEnabled() ? kCheckSmallIcon : kCheckOldIcon,
          IDS_SKILL_SAVED_TOAST_BODY)
          .AddCloseButton()
          .AddActionButton(IDS_SKILL_SAVED_TOAST_BUTTON,
                           base::BindRepeating(
                               [](BrowserWindowInterface* window) {
                                 skills::SkillsUiWindowController::From(window)
                                     ->InvokeLastSavedSkill();
                               },
                               base::Unretained(browser_window_interface)))
          .Build());

  toast_registry_->RegisterToast(
      ToastId::kSkillSavedWithoutInvokeButton,
      ToastSpecification::Builder(
          features::IsRoundedIconsEnabled() ? kCheckSmallIcon : kCheckOldIcon,
          IDS_SKILL_SAVED_TOAST_BODY)
          .Build());

  toast_registry_->RegisterToast(
      ToastId::kSkillDeleted,
      ToastSpecification::Builder(
          features::IsRoundedIconsEnabled() ? kDeleteIcon : kDeleteOldIcon,
          IDS_SKILL_DELETED_TOAST_BODY)
          .AddCloseButton()
          .AddActionButton(IDS_SKILL_UNDO_TOAST_BUTTON,
                           base::BindRepeating(
                               [](BrowserWindowInterface* window) {
                                 skills::SkillsUiWindowController::From(window)
                                     ->UndoLastSkillRemoval();
                               },
                               base::Unretained(browser_window_interface)))
          .Build());

  toast_registry_->RegisterToast(
      ToastId::kRecordReplay,
      ToastSpecification::Builder(
          features::IsRoundedIconsEnabled() ? kInfoIcon : kInfoOldIcon)
          .Build());

  toast_registry_->RegisterToast(
      ToastId::kAutoSignIn,
      ToastSpecification::Builder(features::IsRoundedIconsEnabled()
                                      ? vector_icons::kPasswordManagerIcon
                                      : vector_icons::kPasswordManagerOldIcon,
                                  IDS_MANAGE_PASSWORDS_AUTO_SIGNIN_TOAST_BODY)
          .AddMenu()
          .Build());

  if (base::FeatureList::IsEnabled(
          autofill::features::kAutofillAiWalletPrivatePasses)) {
    toast_registry_->RegisterToast(
        ToastId::kAutofillAiFetchFromWalletErrorMessage,
        ToastSpecification::Builder(
            features::IsRoundedIconsEnabled()
                ? vector_icons::kPersonTextIcon
                : vector_icons::kPersonTextOldIcon,
            IDS_AUTOFILL_AI_WALLET_FETCH_FAILURE_NOTIFICATION)
            .AddGlobalScoped()
            .Build());
    toast_registry_->RegisterToast(
        ToastId::kAutofillAiSaveToWalletErrorMessage,
        ToastSpecification::Builder(
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
            vector_icons::kGoogleWalletMonochromeIcon,
#else
            features::IsRoundedIconsEnabled()
                ? vector_icons::kPersonTextIcon
                : vector_icons::kPersonTextOldIcon,
#endif
            IDS_AUTOFILL_AI_WALLET_UPDATE_OR_MIGRATE_FAILURE_NOTIFICATION)
            .AddGlobalScoped()
            .Build());
  }

  if (base::FeatureList::IsEnabled(toast_features::kTranslateToast)) {
    toast_registry_->RegisterToast(
        ToastId::kTranslate,
        ToastSpecification::Builder(vector_icons::kGTranslateIcon,
                                    IDS_TRANSLATE_TOAST_BODY)
            .AddActionButton(
                IDS_TRANSLATE_TOAST_UNDO_BUTTON,
                base::BindRepeating(
                    [](BrowserWindowInterface* window) {
                      content::WebContents* web_contents =
                          window->GetActiveTabInterface()->GetContents();
                      if (!web_contents) {
                        return;
                      }
                      ChromeTranslateClient* chrome_translate_client =
                          ChromeTranslateClient::FromWebContents(web_contents);
                      if (chrome_translate_client) {
                        chrome_translate_client->UndoTranslate();
                      }
                    },
                    base::Unretained(browser_window_interface)))
            .AddCloseButton()
            .Build());
  }

  toast_registry_->RegisterToast(
      ToastId::kSendTabToSelfTabOpened,
      // TODO(crbug.com/488072250): Update the strings.
      ToastSpecification::Builder(
          features::IsRoundedIconsEnabled()   ? kDevicesIcon
          : features::IsRoundedIconsEnabled() ? vector_icons::kDevicesIcon
                                              : vector_icons::kDevicesOldIcon,
          IDS_SEND_TAB_PUSH_NOTIFICATION_TITLE_USER_GIVEN_DEVICE_NAME)
          .AddGlobalScoped()
          .Build());

  toast_registry_->RegisterToast(
      ToastId::kSendTabToSelfTabsOpenedInBackground,
      // TODO(crbug.com/488072250): Update the strings.
      ToastSpecification::Builder(
          features::IsRoundedIconsEnabled()   ? kDevicesIcon
          : features::IsRoundedIconsEnabled() ? vector_icons::kDevicesIcon
                                              : vector_icons::kDevicesOldIcon,
          IDS_SEND_TAB_PUSH_NOTIFICATION_TITLE_USER_GIVEN_DEVICE_NAME)
          .AddCloseButton()
          .AddActionButton(
              IDS_SEND_TAB_RECEIVE_TOAST_BACKGROUND_ACTION_BUTTON,
              base::BindRepeating(
                  [](BrowserWindowInterface* window) {
                    send_tab_to_self::ReceivingUiHandler* handler =
                        send_tab_to_self::SendTabToSelfClientServiceFactory::
                            GetForProfile(window->GetProfile())
                                ->GetReceivingUiHandler();
                    send_tab_to_self::SendTabToSelfToolbarIconController::
                        FromReceivingUiHandlerInstance(handler)
                            ->SwitchToLatestTabsOpenedInBackground(window);
                  },
                  base::Unretained(browser_window_interface)))
          .AddGlobalScoped()
          .Build());

  // Note: The registered icon here is a fallback default required by the
  // interface. The icon is always overridden dynamically in UI code based on
  // the target device's form factor.
  toast_registry_->RegisterToast(
      ToastId::kSendTabToSelfSuccess,
      ToastSpecification::Builder(
          features::IsRoundedIconsEnabled()   ? kDevicesIcon
          : features::IsRoundedIconsEnabled() ? vector_icons::kDevicesIcon
                                              : vector_icons::kDevicesOldIcon,
          IDS_SEND_TAB_TO_SELF_POST_SEND_SUCCESS_TOAST)
          .AddCloseButton()
          .Build());

  toast_registry_->RegisterToast(
      ToastId::kSendTabToSelfSuccessThrottled,
      ToastSpecification::Builder(
          features::IsRoundedIconsEnabled()   ? kDevicesIcon
          : features::IsRoundedIconsEnabled() ? vector_icons::kDevicesIcon
                                              : vector_icons::kDevicesOldIcon,
          IDS_SEND_TAB_TO_SELF_POST_SEND_THROTTLED_TOAST)
          .AddCloseButton()
          .Build());

  toast_registry_->RegisterToast(
      ToastId::kSendTabToSelfFailure,
      ToastSpecification::Builder(features::IsRoundedIconsEnabled()
                                      ? vector_icons::kErrorIcon
                                      : vector_icons::kErrorOldIcon,
                                  IDS_SEND_TAB_TO_SELF_POST_SEND_FAILURE_TOAST)
          .AddCloseButton()
          .Build());

  toast_registry_->RegisterToast(
      ToastId::kSendTabToSelfNoInternetConnection,
      ToastSpecification::Builder(
          features::IsRoundedIconsEnabled() ? vector_icons::kErrorIcon
                                            : vector_icons::kErrorOldIcon,
          IDS_SEND_TAB_TO_SELF_POST_SEND_NO_INTERNET_TOAST)
          .AddCloseButton()
          .Build());

  // Report a scam confirmation toast.
  toast_registry_->RegisterToast(
      ToastId::kReportUnsafeSiteConfirmation,
      ToastSpecification::Builder(
          features::IsRoundedIconsEnabled() ? kCheckSmallIcon : kCheckOldIcon,
          IDS_REPORT_UNSAFE_SITE_CONFIRMATION_TOAST)
          .Build());

#if !BUILDFLAG(IS_CHROMEOS)
  toast_registry_->RegisterToast(
      ToastId::kDefaultBrowserUpdateSuccess,
      ToastSpecification::Builder(
          features::IsRoundedIconsEnabled() ? kCheckSmallIcon : kCheckOldIcon,
          IDS_DEFAULT_BROWSER_SUCCESS_TOAST_BODY)
          .Build());
#endif  // !BUILDFLAG(IS_CHROMEOS)

  if (base::FeatureList::IsEnabled(features::kIndigo)) {
    toast_registry_->RegisterToast(
        ToastId::kIndigoInvokeError,
        ToastSpecification::Builder(features::IsRoundedIconsEnabled()
                                        ? vector_icons::kErrorIcon
                                        : vector_icons::kErrorOutlineOldIcon,
                                    IDS_INDIGO_INVOKE_ERROR_TOAST_BODY)
            .AddActionButton(
                IDS_INDIGO_INVOKE_ERROR_TOAST_TRY_AGAIN_BUTTON,
                base::BindRepeating(
                    [](BrowserWindowInterface* window) {
                      if (tabs::TabInterface* tab =
                              window->GetActiveTabInterface()) {
                        if (auto* controller =
                                indigo::IndigoPageActionController::From(tab)) {
                          controller->InvokeAction(
                              indigo::EntryPoint::kErrorToast);
                        }
                      }
                    },
                    base::Unretained(browser_window_interface)))
            .AddCloseButton()
            .Build());
  }
}  // RegisterToasts() end.
