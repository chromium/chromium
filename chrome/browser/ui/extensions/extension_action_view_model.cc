// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/extensions/extension_action_view_model.h"

#include <memory>
#include <string>
#include <utility>

#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/commands/command_service.h"
#include "chrome/browser/extensions/extension_action_runner.h"
#include "chrome/browser/extensions/extension_context_menu_model.h"
#include "chrome/browser/extensions/extension_view.h"
#include "chrome/browser/extensions/extension_view_host.h"
#include "chrome/browser/extensions/extension_view_host_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/extensions/extension_action_platform_delegate.h"
#include "chrome/browser/ui/extensions/extension_popup_types.h"
#include "chrome/browser/ui/extensions/icon_with_badge_image_source.h"
#include "chrome/browser/ui/tabs/tab_list_interface.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_model.h"
#include "chrome/grit/generated_resources.h"
#include "components/sessions/content/session_tab_helper.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_action.h"
#include "extensions/browser/extension_action_manager.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/permissions/site_permissions_helper.h"
#include "extensions/common/api/extension_action/action_info.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/manifest_constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/color/color_provider_manager.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/native_ui_types.h"
#include "ui/native_theme/native_theme.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/api/side_panel/side_panel_service.h"
#include "chrome/browser/ui/extensions/extension_side_panel_utils.h"
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

using content::WebContentsObserver;
using extensions::ActionInfo;
using extensions::CommandService;
using extensions::ExtensionActionRunner;
using extensions::PermissionsManager;

namespace {

void RecordInvocationSource(ToolbarActionViewModel::InvocationSource source) {
  base::UmaHistogramEnumeration("Extensions.Toolbar.InvocationSource", source);
}

// Computes hover card site access status based on:
// 1. Extension wants site access: user site settings takes precedence
// over the extension's site access.
// 2. Extension does not want access: if all extensions are blocked display
// such message because a) user could wrongly infer that an extension that
// does not want access has access if we only show the blocked message for
// extensions that want access; and b) it helps us work around tricky
// calculations where we get into collisions between withheld and denied
// permission. Otherwise, it should display "does not want access".
ExtensionActionViewModel::HoverCardState::SiteAccess
GetHoverCardSiteAccessState(
    extensions::PermissionsManager::UserSiteSetting site_setting,
    extensions::SitePermissionsHelper::SiteInteraction site_interaction) {
  switch (site_interaction) {
    case extensions::SitePermissionsHelper::SiteInteraction::kGranted:
      return site_setting == extensions::PermissionsManager::UserSiteSetting::
                                 kGrantAllExtensions
                 ? ExtensionActionViewModel::HoverCardState::SiteAccess::
                       kAllExtensionsAllowed
                 : ExtensionActionViewModel::HoverCardState::SiteAccess::
                       kExtensionHasAccess;

    case extensions::SitePermissionsHelper::SiteInteraction::kWithheld:
    case extensions::SitePermissionsHelper::SiteInteraction::kActiveTab:
      return site_setting == extensions::PermissionsManager::UserSiteSetting::
                                 kBlockAllExtensions
                 ? ExtensionActionViewModel::HoverCardState::SiteAccess::
                       kAllExtensionsBlocked
                 : ExtensionActionViewModel::HoverCardState::SiteAccess::
                       kExtensionRequestsAccess;

    case extensions::SitePermissionsHelper::SiteInteraction::kNone:
      // kNone site interaction includes extensions that don't want access when
      // user site setting is "block all extensions".
      return site_setting == extensions::PermissionsManager::UserSiteSetting::
                                 kBlockAllExtensions
                 ? ExtensionActionViewModel::HoverCardState::SiteAccess::
                       kAllExtensionsBlocked
                 : ExtensionActionViewModel::HoverCardState::SiteAccess::
                       kExtensionDoesNotWantAccess;
  }
}

// Computes hover card policy status based on admin policy. Note that an
// extension pinned by admin is also installed by admin. Thus, "pinned by admin"
// has preference.
ExtensionActionViewModel::HoverCardState::AdminPolicy GetHoverCardPolicyState(
    Profile& profile,
    const extensions::ExtensionId& extension_id) {
  auto* const model = ToolbarActionsModel::Get(&profile);
  if (model->IsActionForcePinned(extension_id)) {
    return ExtensionActionViewModel::HoverCardState::AdminPolicy::
        kPinnedByAdmin;
  }

  scoped_refptr<const extensions::Extension> extension =
      extensions::ExtensionRegistry::Get(&profile)
          ->enabled_extensions()
          .GetByID(extension_id);
  if (extensions::Manifest::IsPolicyLocation(extension->location())) {
    return ExtensionActionViewModel::HoverCardState::AdminPolicy::
        kInstalledByAdmin;
  }

  return ExtensionActionViewModel::HoverCardState::AdminPolicy::kNone;
}

}  // namespace

// static
std::unique_ptr<ExtensionActionViewModel> ExtensionActionViewModel::Create(
    const extensions::ExtensionId& extension_id,
    BrowserWindowInterface* browser,
    std::unique_ptr<ExtensionActionPlatformDelegate> platform_delegate) {
  DCHECK(browser);

  Profile* profile = browser->GetProfile();
  auto* registry = extensions::ExtensionRegistry::Get(profile);
  scoped_refptr<const extensions::Extension> extension =
      registry->enabled_extensions().GetByID(extension_id);
  DCHECK(extension);
  extensions::ExtensionAction* extension_action =
      extensions::ExtensionActionManager::Get(profile)->GetExtensionAction(
          *extension);
  DCHECK(extension_action);

  // WrapUnique() because the constructor is private.
  return base::WrapUnique(new ExtensionActionViewModel(
      std::move(extension), browser, extension_action, registry,
      std::move(platform_delegate)));
}

// static
bool ExtensionActionViewModel::AnyActionHasCurrentSiteAccess(
    const std::vector<std::unique_ptr<ToolbarActionViewModel>>& actions,
    content::WebContents* web_contents) {
  for (const auto& action : actions) {
    if (action->GetSiteInteraction(web_contents) ==
        extensions::SitePermissionsHelper::SiteInteraction::kGranted) {
      return true;
    }
  }
  return false;
}

ExtensionActionViewModel::ExtensionActionViewModel(
    scoped_refptr<const extensions::Extension> extension,
    BrowserWindowInterface* browser,
    extensions::ExtensionAction* extension_action,
    extensions::ExtensionRegistry* extension_registry,
    std::unique_ptr<ExtensionActionPlatformDelegate> platform_delegate)
    : extension_id_(extension->id()),
      extension_(std::move(extension)),
      browser_(browser),
      profile_(browser->GetProfile()),
      extension_action_(extension_action),
      platform_delegate_(std::move(platform_delegate)),
      icon_factory_(extension_.get(), extension_action, this),
      extension_registry_(extension_registry) {
  platform_delegate_->AttachToModel(this);
  WebContentsObserver::Observe(GetCurrentWebContents());
  tab_list_observation_.Observe(TabListInterface::From(browser_));
  toolbar_model_observation_.Observe(ToolbarActionsModel::Get(profile_));
  command_service_observation_.Observe(
      extensions::CommandService::Get(profile_));
}

ExtensionActionViewModel::~ExtensionActionViewModel() {
  DCHECK(!IsShowingPopup());
  WebContentsObserver::Observe(nullptr);
  platform_delegate_->DetachFromModel();
}

std::string ExtensionActionViewModel::GetId() const {
  return extension_id_;
}

base::CallbackListSubscription ExtensionActionViewModel::RegisterUpdateObserver(
    base::RepeatingClosure observer) {
  return observers_.Add(observer);
}

ui::ImageModel ExtensionActionViewModel::GetIcon(
    content::WebContents* web_contents,
    const gfx::Size& size) {
  if (!ExtensionIsValid()) {
    return ui::ImageModel();
  }

  return ui::ImageModel::FromImageSkia(
      gfx::ImageSkia(GetIconImageSource(web_contents, size), size));
}

std::u16string ExtensionActionViewModel::GetActionName() const {
  if (!ExtensionIsValid()) {
    return std::u16string();
  }

  return base::UTF8ToUTF16(extension_->name());
}

std::u16string ExtensionActionViewModel::GetActionTitle(
    content::WebContents* web_contents) const {
  if (!ExtensionIsValid()) {
    return std::u16string();
  }

  std::string title = extension_action_->GetTitle(
      sessions::SessionTabHelper::IdForTab(web_contents).id());
  return base::UTF8ToUTF16(title);
}

std::u16string ExtensionActionViewModel::GetAccessibleName(
    content::WebContents* web_contents) const {
  if (!ExtensionIsValid()) {
    return std::u16string();
  }

  // GetAccessibleName() can (surprisingly) be called during browser
  // teardown. Handle this gracefully.
  if (!web_contents) {
    return base::UTF8ToUTF16(extension_->name());
  }

  std::u16string action_title = GetActionTitle(web_contents);
  std::u16string accessible_name =
      action_title.empty() ? GetActionName() : action_title;

  // Include a "host access" portion of the tooltip if the extension has active
  // or pending interaction with the site.
  auto site_interaction = GetSiteInteraction(web_contents);
  int site_interaction_description_id = -1;
  switch (site_interaction) {
    case extensions::SitePermissionsHelper::SiteInteraction::kNone:
      // No string for neither having nor wanting access.
      break;
    case extensions::SitePermissionsHelper::SiteInteraction::kWithheld:
    case extensions::SitePermissionsHelper::SiteInteraction::kActiveTab:
      site_interaction_description_id = IDS_EXTENSIONS_WANTS_ACCESS_TO_SITE;
      break;
    case extensions::SitePermissionsHelper::SiteInteraction::kGranted:
      site_interaction_description_id = IDS_EXTENSIONS_HAS_ACCESS_TO_SITE;
      break;
  }

  if (site_interaction_description_id != -1) {
    accessible_name = base::StrCat(
        {accessible_name, u"\n",
         l10n_util::GetStringUTF16(site_interaction_description_id)});
  }

  return accessible_name;
}

std::u16string ExtensionActionViewModel::GetTooltip(
    content::WebContents* web_contents) const {
  if (base::FeatureList::IsEnabled(
          extensions_features::kExtensionsMenuAccessControl)) {
    std::u16string action_title = GetActionTitle(web_contents);
    std::u16string tooltip =
        action_title.empty() ? GetActionName() : action_title;

    url::Origin origin =
        web_contents->GetPrimaryMainFrame()->GetLastCommittedOrigin();
    auto* permissions_manager = extensions::PermissionsManager::Get(profile_);
    ToolbarActionViewModel::HoverCardState::SiteAccess site_access =
        GetHoverCardSiteAccessState(
            permissions_manager->GetUserSiteSetting(origin),
            GetSiteInteraction(web_contents));

    int tooltip_site_access_id;
    switch (site_access) {
      case HoverCardState::SiteAccess::kAllExtensionsAllowed:
      case HoverCardState::SiteAccess::kExtensionHasAccess:
        tooltip_site_access_id =
            IDS_EXTENSIONS_MENU_MAIN_PAGE_EXTENSION_BUTTON_HAS_ACCESS_TOOLTIP;
        break;
      case HoverCardState::SiteAccess::kAllExtensionsBlocked:
        tooltip_site_access_id =
            IDS_EXTENSIONS_MENU_MAIN_PAGE_EXTENSION_BUTTON_BLOCKED_ACCESS_TOOLTIP;
        break;
      case HoverCardState::SiteAccess::kExtensionRequestsAccess:
        tooltip_site_access_id =
            IDS_EXTENSIONS_MENU_MAIN_PAGE_EXTENSION_BUTTON_REQUESTS_TOOLTIP;
        break;
      case HoverCardState::SiteAccess::kExtensionDoesNotWantAccess:
        tooltip_site_access_id = -1;
    }

    return tooltip_site_access_id == -1
               ? tooltip
               : base::JoinString({tooltip, l10n_util::GetStringUTF16(
                                                tooltip_site_access_id)},
                                  u"\n");
  }

  return GetAccessibleName(web_contents);
}

bool ExtensionActionViewModel::IsEnabled(
    content::WebContents* web_contents) const {
  if (!ExtensionIsValid()) {
    return false;
  }

  int tab_id = sessions::SessionTabHelper::IdForTab(web_contents).id();
  if (extension_action_->GetIsVisible(tab_id)) {
    return true;
  }

  extensions::SitePermissionsHelper::SiteInteraction site_interaction =
      GetSiteInteraction(web_contents);
  if (site_interaction ==
          extensions::SitePermissionsHelper::SiteInteraction::kWithheld ||
      site_interaction ==
          extensions::SitePermissionsHelper::SiteInteraction::kActiveTab) {
    return true;
  }

#if BUILDFLAG(ENABLE_EXTENSIONS)
  extensions::SidePanelService* side_panel_service =
      extensions::SidePanelService::Get(profile_);
  if (side_panel_service &&
      side_panel_service->HasSidePanelActionForTab(*extension_, tab_id)) {
    return true;
  }
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)}
  return false;
}

bool ExtensionActionViewModel::IsShowingPopup() const {
  return platform_delegate_->IsShowingPopup();
}

void ExtensionActionViewModel::HidePopup() {
  return platform_delegate_->HidePopup();
}

gfx::NativeView ExtensionActionViewModel::GetPopupNativeView() {
  return platform_delegate_->GetPopupNativeView();
}

ui::MenuModel* ExtensionActionViewModel::GetContextMenu(
    extensions::ExtensionContextMenuModel::ContextMenuSource
        context_menu_source) {
  if (!ExtensionIsValid()) {
    return nullptr;
  }

  bool is_pinned = ToolbarActionsModel::Get(profile_)->IsActionPinned(GetId());

  // Reconstruct the menu every time because the menu's contents are dynamic.
  context_menu_model_ = std::make_unique<extensions::ExtensionContextMenuModel>(
      extension_.get(), browser_, is_pinned, this,
      ToolbarActionsModel::CanShowActionsInToolbar(*browser_),
      context_menu_source);
  return context_menu_model_.get();
}

void ExtensionActionViewModel::ExecuteUserAction(InvocationSource source) {
  if (!ExtensionIsValid()) {
    return;
  }

  content::WebContents* const web_contents = GetCurrentWebContents();
  if (!IsEnabled(web_contents)) {
    platform_delegate_->ShowContextMenuAsFallback();
    return;
  }

  ExtensionActionRunner* action_runner =
      ExtensionActionRunner::GetForWebContents(web_contents);
  if (!action_runner) {
    return;
  }

  RecordInvocationSource(source);

  platform_delegate_->CloseOverflowMenuIfOpen();

  // This method is only called to execute an action by the user, so we can
  // always grant tab permissions.
  constexpr bool kGrantTabPermissions = true;
  extensions::ExtensionAction::ShowAction action =
      action_runner->RunAction(extension_.get(), kGrantTabPermissions);

  if (action == extensions::ExtensionAction::ShowAction::kShowPopup) {
    constexpr bool kByUser = true;
    TriggerPopup(PopupShowAction::kShow, kByUser, ShowPopupCallback());
  } else if (action ==
             extensions::ExtensionAction::ShowAction::kToggleSidePanel) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
    extensions::side_panel_util::ToggleExtensionSidePanel(browser_,
                                                          extension_->id());
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
  }
}

void ExtensionActionViewModel::TriggerPopupForAPI(ShowPopupCallback callback) {
  RecordInvocationSource(InvocationSource::kApi);
  // This method is called programmatically by an API; it should never be
  // considered a user action.
  constexpr bool kByUser = false;
  TriggerPopup(PopupShowAction::kShow, kByUser, std::move(callback));
}

void ExtensionActionViewModel::RegisterCommand() {
  if (!ExtensionIsValid()) {
    return;
  }

  platform_delegate_->RegisterCommand();
}

void ExtensionActionViewModel::UnregisterCommand() {
  platform_delegate_->UnregisterCommand();
}

void ExtensionActionViewModel::DidFinishNavigation(
    content::NavigationHandle* handle) {
  NotifyObservers();
}

void ExtensionActionViewModel::OnActiveTabChanged(tabs::TabInterface* tab) {
  WebContentsObserver::Observe(GetCurrentWebContents());
  NotifyObservers();
}

void ExtensionActionViewModel::OnToolbarActionAdded(
    const ToolbarActionsModel::ActionId& action_id) {}

void ExtensionActionViewModel::OnToolbarActionRemoved(
    const ToolbarActionsModel::ActionId& action_id) {}

void ExtensionActionViewModel::OnToolbarActionUpdated(
    const ToolbarActionsModel::ActionId& action_id) {
  if (action_id != extension_->id()) {
    return;
  }
  NotifyObservers();
}

void ExtensionActionViewModel::OnToolbarModelInitialized() {}

void ExtensionActionViewModel::OnToolbarPinnedActionsChanged() {}

void ExtensionActionViewModel::OnExtensionCommandAdded(
    const std::string& extension_id,
    const std::string& command_name) {
  if (extension_id != extension_->id()) {
    return;  // Not this action's extension.
  }

  if (!extensions::Command::IsActionRelatedCommand(command_name)) {
    return;
  }

  RegisterCommand();
}

void ExtensionActionViewModel::OnExtensionCommandRemoved(
    const std::string& extension_id,
    const std::string& command_name) {
  if (extension_id != extension_->id()) {
    return;
  }

  if (!extensions::Command::IsActionRelatedCommand(command_name)) {
    return;
  }

  extensions::Command extension_command;
  if (GetExtensionCommand(&extension_command)) {
    return;  // Command has not been removed.
  }

  UnregisterCommand();
}

void ExtensionActionViewModel::OnCommandServiceDestroying() {
  DCHECK(command_service_observation_.IsObserving());
  command_service_observation_.Reset();
}

void ExtensionActionViewModel::InspectPopup() {
  // This method is only triggered through user action (clicking on the context
  // menu entry).
  TriggerPopup(PopupShowAction::kShowAndInspect, /*by_user*/ true,
               ShowPopupCallback());
}

content::WebContents* ExtensionActionViewModel::GetCurrentWebContents() const {
  tabs::TabInterface* tab = TabListInterface::From(browser_)->GetActiveTab();
  if (!tab) {
    return nullptr;
  }
  return tab->GetContents();
}

void ExtensionActionViewModel::NotifyObservers() {
  if (!TabListInterface::From(browser_)->GetActiveTab()) {
    return;
  }
  observers_.Notify();
}

void ExtensionActionViewModel::OnIconUpdated() {
  NotifyObservers();
}

extensions::SitePermissionsHelper::SiteInteraction
ExtensionActionViewModel::GetSiteInteraction(
    content::WebContents* web_contents) const {
  return extensions::SitePermissionsHelper(profile_).GetSiteInteraction(
      *extension_, web_contents);
}

bool ExtensionActionViewModel::ExtensionIsValid() const {
  return extension_registry_->enabled_extensions().Contains(extension_->id());
}

bool ExtensionActionViewModel::GetExtensionCommand(
    extensions::Command* command) const {
  DCHECK(command);
  if (!ExtensionIsValid()) {
    return false;
  }

  CommandService* command_service = CommandService::Get(profile_);
  return command_service->GetExtensionActionCommand(
      extension_->id(), extension_action_->action_type(),
      CommandService::ACTIVE, command, nullptr);
}

ToolbarActionViewModel::HoverCardState
ExtensionActionViewModel::GetHoverCardState(
    content::WebContents* web_contents) const {
  DCHECK(web_contents);

  if (!ExtensionIsValid()) {
    HoverCardState state;
    state.site_access = HoverCardState::SiteAccess::kExtensionDoesNotWantAccess;
    state.policy = HoverCardState::AdminPolicy::kNone;
    return state;
  }

  url::Origin origin =
      web_contents->GetPrimaryMainFrame()->GetLastCommittedOrigin();
  extensions::PermissionsManager::UserSiteSetting site_setting =
      extensions::PermissionsManager::Get(profile_)->GetUserSiteSetting(origin);
  auto site_interaction = GetSiteInteraction(web_contents);

  HoverCardState state;
  state.site_access =
      GetHoverCardSiteAccessState(site_setting, site_interaction);
  state.policy = GetHoverCardPolicyState(*profile_, GetId());

  return state;
}

bool ExtensionActionViewModel::CanHandleAccelerators() const {
  if (!ExtensionIsValid()) {
    return false;
  }

#if DCHECK_IS_ON()
  {
    extensions::Command command;
    DCHECK(GetExtensionCommand(&command));
  }
#endif

  // Page action accelerators are enabled if and only if the page action is
  // enabled ("visible" in legacy terms) on the given tab. Other actions can
  // always accept accelerators.
  // TODO(devlin): Have all actions behave similarly; this should likely mean
  // always checking IsEnabled(). It's weird to use a keyboard shortcut on a
  // disabled action (in most cases, this will result in opening the context
  // menu).
  if (extension_action_->action_type() == extensions::ActionInfo::Type::kPage) {
    return IsEnabled(GetCurrentWebContents());
  }
  return true;
}

std::unique_ptr<IconWithBadgeImageSource>
ExtensionActionViewModel::GetIconImageSourceForTesting(
    content::WebContents* web_contents,
    const gfx::Size& size) {
  return GetIconImageSource(web_contents, size);
}

void ExtensionActionViewModel::TriggerPopup(PopupShowAction show_action,
                                            bool by_user,
                                            ShowPopupCallback callback) {
  if (!ExtensionIsValid()) {
    return;
  }

  content::WebContents* const web_contents = GetCurrentWebContents();
  const int tab_id = sessions::SessionTabHelper::IdForTab(web_contents).id();
  DCHECK(extension_action_->GetIsVisible(tab_id));
  DCHECK(extension_action_->HasPopup(tab_id));

  const GURL popup_url = extension_action_->GetPopupUrl(tab_id);

  std::unique_ptr<extensions::ExtensionViewHost> host =
      extensions::ExtensionViewHostFactory::CreatePopupHost(popup_url,
                                                            browser_);
  // Creating a host should never fail in this case, since the extension is
  // valid and has a valid popup URL.
  CHECK(host);

  platform_delegate_->TriggerPopup(std::move(host), show_action, by_user,
                                   std::move(callback));
}

std::unique_ptr<IconWithBadgeImageSource>
ExtensionActionViewModel::GetIconImageSource(content::WebContents* web_contents,
                                             const gfx::Size& size) {
  // `web_contents` may be null during tab closure or in tests.  Fall back on a
  // generic color provider.
  auto get_color_provider_callback = base::BindRepeating(
      [](base::WeakPtr<content::WebContents> weak_web_contents) {
        return weak_web_contents
                   ? &weak_web_contents->GetColorProvider()
                   : ui::ColorProviderManager::Get().GetColorProviderFor(
                         ui::NativeTheme::GetInstanceForNativeUi()
                             ->GetColorProviderKey(nullptr));
      },
      web_contents ? web_contents->GetWeakPtr()
                   : base::WeakPtr<content::WebContents>());
  auto image_source = std::make_unique<IconWithBadgeImageSource>(
      size, std::move(get_color_provider_callback));

  int tab_id = sessions::SessionTabHelper::IdForTab(web_contents).id();
  image_source->SetIcon(icon_factory_.GetIcon(tab_id));

  std::unique_ptr<IconWithBadgeImageSource::Badge> badge;
  std::string badge_text = extension_action_->GetDisplayBadgeText(tab_id);
  if (!badge_text.empty()) {
    badge = std::make_unique<IconWithBadgeImageSource::Badge>(
        badge_text, extension_action_->GetBadgeTextColor(tab_id),
        extension_action_->GetBadgeBackgroundColor(tab_id));
  }
  image_source->SetBadge(std::move(badge));

  // We only grayscale the icon if it cannot interact with the page and the icon
  // is disabled.
  bool action_is_visible = extension_action_->GetIsVisible(tab_id);

#if BUILDFLAG(ENABLE_EXTENSIONS)
  extensions::SidePanelService* side_panel_service =
      extensions::SidePanelService::Get(profile_);
  bool has_side_panel_action =
      side_panel_service &&
      side_panel_service->HasSidePanelActionForTab(*extension_, tab_id);
#else   // BUILDFLAG(ENABLE_EXTENSIONS)
  bool has_side_panel_action = false;
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
  bool is_grayscale =
      GetSiteInteraction(web_contents) ==
          extensions::SitePermissionsHelper::SiteInteraction::kNone &&
      !action_is_visible && !has_side_panel_action;
  image_source->set_grayscale(is_grayscale);

  if (base::FeatureList::IsEnabled(
          extensions_features::kExtensionsMenuAccessControl)) {
    return image_source;
  }

  bool was_blocked = extensions::SitePermissionsHelper(profile_).HasBeenBlocked(
      *extension_, web_contents);
  image_source->set_paint_blocked_actions_decoration(was_blocked);

  return image_source;
}
