// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/extensions/extension_action_view_controller.h"

#include <memory>
#include <string>
#include <utility>

#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/api/side_panel/side_panel_service.h"
#include "chrome/browser/extensions/commands/command_service.h"
#include "chrome/browser/extensions/extension_action_runner.h"
#include "chrome/browser/extensions/extension_context_menu_model.h"
#include "chrome/browser/extensions/extension_view.h"
#include "chrome/browser/extensions/extension_view_host.h"
#include "chrome/browser/extensions/extension_view_host_factory.h"
#include "chrome/browser/extensions/permissions/site_permissions_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/extensions/extension_action_platform_delegate.h"
#include "chrome/browser/ui/extensions/extension_popup_types.h"
#include "chrome/browser/ui/extensions/extension_side_panel_utils.h"
#include "chrome/browser/ui/extensions/extensions_container.h"
#include "chrome/browser/ui/extensions/icon_with_badge_image_source.h"
#include "chrome/browser/ui/toolbar/toolbar_action_view_delegate.h"
#include "chrome/grit/generated_resources.h"
#include "components/sessions/content/session_tab_helper.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_action.h"
#include "extensions/browser/extension_action_manager.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/api/extension_action/action_info.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/manifest_constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/color/color_provider_manager.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/native_theme/native_theme.h"

using extensions::ActionInfo;
using extensions::CommandService;
using extensions::ExtensionActionRunner;
using extensions::PermissionsManager;

namespace {

void RecordInvocationSource(
    ToolbarActionViewController::InvocationSource source) {
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
ExtensionActionViewController::HoverCardState::SiteAccess
GetHoverCardSiteAccessState(
    extensions::PermissionsManager::UserSiteSetting site_setting,
    extensions::SitePermissionsHelper::SiteInteraction site_interaction) {
  switch (site_interaction) {
    case extensions::SitePermissionsHelper::SiteInteraction::kGranted:
      return site_setting == extensions::PermissionsManager::UserSiteSetting::
                                 kGrantAllExtensions
                 ? ExtensionActionViewController::HoverCardState::SiteAccess::
                       kAllExtensionsAllowed
                 : ExtensionActionViewController::HoverCardState::SiteAccess::
                       kExtensionHasAccess;

    case extensions::SitePermissionsHelper::SiteInteraction::kWithheld:
    case extensions::SitePermissionsHelper::SiteInteraction::kActiveTab:
      return site_setting == extensions::PermissionsManager::UserSiteSetting::
                                 kBlockAllExtensions
                 ? ExtensionActionViewController::HoverCardState::SiteAccess::
                       kAllExtensionsBlocked
                 : ExtensionActionViewController::HoverCardState::SiteAccess::
                       kExtensionRequestsAccess;

    case extensions::SitePermissionsHelper::SiteInteraction::kNone:
      // kNone site interaction includes extensions that don't want access when
      // user site setting is "block all extensions".
      return site_setting == extensions::PermissionsManager::UserSiteSetting::
                                 kBlockAllExtensions
                 ? ExtensionActionViewController::HoverCardState::SiteAccess::
                       kAllExtensionsBlocked
                 : ExtensionActionViewController::HoverCardState::SiteAccess::
                       kExtensionDoesNotWantAccess;
  }
}

// Computes hover card policy status based on admin policy. Note that an
// extension pinned by admin is also installed by admin. Thus, "pinned by admin"
// has preference.
ExtensionActionViewController::HoverCardState::AdminPolicy
GetHoverCardPolicyState(Browser* browser,
                        const extensions::ExtensionId& extension_id) {
  auto* const model = ToolbarActionsModel::Get(browser->profile());
  if (model->IsActionForcePinned(extension_id))
    return ExtensionActionViewController::HoverCardState::AdminPolicy::
        kPinnedByAdmin;

  scoped_refptr<const extensions::Extension> extension =
      extensions::ExtensionRegistry::Get(browser->profile())
          ->enabled_extensions()
          .GetByID(extension_id);
  if (extensions::Manifest::IsPolicyLocation(extension->location()))
    return ExtensionActionViewController::HoverCardState::AdminPolicy::
        kInstalledByAdmin;

  return ExtensionActionViewController::HoverCardState::AdminPolicy::kNone;
}

}  // namespace

// static
std::unique_ptr<ExtensionActionViewController>
ExtensionActionViewController::Create(
    const extensions::ExtensionId& extension_id,
    Browser* browser,
    ExtensionsContainer* extensions_container) {
  DCHECK(browser);
  DCHECK(extensions_container);

  auto* registry = extensions::ExtensionRegistry::Get(browser->profile());
  scoped_refptr<const extensions::Extension> extension =
      registry->enabled_extensions().GetByID(extension_id);
  DCHECK(extension);
  extensions::ExtensionAction* extension_action =
      extensions::ExtensionActionManager::Get(browser->profile())
          ->GetExtensionAction(*extension);
  DCHECK(extension_action);

  // WrapUnique() because the constructor is private.
  return base::WrapUnique(new ExtensionActionViewController(
      std::move(extension), browser, extension_action, registry,
      extensions_container));
}

// static
bool ExtensionActionViewController::AnyActionHasCurrentSiteAccess(
    const std::vector<std::unique_ptr<ToolbarActionViewController>>& actions,
    content::WebContents* web_contents) {
  for (const auto& action : actions) {
    if (action->GetSiteInteraction(web_contents) ==
        extensions::SitePermissionsHelper::SiteInteraction::kGranted) {
      return true;
    }
  }
  return false;
}

ExtensionActionViewController::ExtensionActionViewController(
    scoped_refptr<const extensions::Extension> extension,
    Browser* browser,
    extensions::ExtensionAction* extension_action,
    extensions::ExtensionRegistry* extension_registry,
    ExtensionsContainer* extensions_container)
    : extension_(std::move(extension)),
      browser_(browser),
      extension_action_(extension_action),
      extensions_container_(extensions_container),
      popup_host_(nullptr),
      view_delegate_(nullptr),
      platform_delegate_(ExtensionActionPlatformDelegate::Create(this)),
      icon_factory_(extension_.get(), extension_action, this),
      extension_registry_(extension_registry) {}

ExtensionActionViewController::~ExtensionActionViewController() {
  DCHECK(!IsShowingPopup());
}

std::string ExtensionActionViewController::GetId() const {
  return extension_->id();
}

void ExtensionActionViewController::SetDelegate(
    ToolbarActionViewDelegate* delegate) {
  DCHECK((delegate == nullptr) ^ (view_delegate_ == nullptr));
  if (delegate) {
    view_delegate_ = delegate;
  } else {
    HidePopup();
    platform_delegate_.reset();
    view_delegate_ = nullptr;
  }
}

ui::ImageModel ExtensionActionViewController::GetIcon(
    content::WebContents* web_contents,
    const gfx::Size& size) {
  if (!ExtensionIsValid())
    return ui::ImageModel();

  return ui::ImageModel::FromImageSkia(
      gfx::ImageSkia(GetIconImageSource(web_contents, size), size));
}

std::u16string ExtensionActionViewController::GetActionName() const {
  if (!ExtensionIsValid())
    return std::u16string();

  return base::UTF8ToUTF16(extension_->name());
}

std::u16string ExtensionActionViewController::GetActionTitle(
    content::WebContents* web_contents) const {
  if (!ExtensionIsValid()) {
    return std::u16string();
  }

  std::string title = extension_action_->GetTitle(
      sessions::SessionTabHelper::IdForTab(web_contents).id());
  return base::UTF8ToUTF16(title);
}

std::u16string ExtensionActionViewController::GetAccessibleName(
    content::WebContents* web_contents) const {
  if (!ExtensionIsValid())
    return std::u16string();

  // GetAccessibleName() can (surprisingly) be called during browser
  // teardown. Handle this gracefully.
  if (!web_contents)
    return base::UTF8ToUTF16(extension()->name());

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

std::u16string ExtensionActionViewController::GetTooltip(
    content::WebContents* web_contents) const {
  if (base::FeatureList::IsEnabled(
          extensions_features::kExtensionsMenuAccessControl)) {
    std::u16string action_title = GetActionTitle(web_contents);
    std::u16string tooltip =
        action_title.empty() ? GetActionName() : action_title;

    url::Origin origin =
        web_contents->GetPrimaryMainFrame()->GetLastCommittedOrigin();
    auto* permissions_manager =
        extensions::PermissionsManager::Get(browser_->profile());
    ToolbarActionViewController::HoverCardState::SiteAccess site_access =
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

bool ExtensionActionViewController::IsEnabled(
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

  extensions::SidePanelService* side_panel_service =
      extensions::SidePanelService::Get(browser_->profile());
  return side_panel_service &&
         side_panel_service->HasSidePanelActionForTab(*extension(), tab_id);
}

bool ExtensionActionViewController::IsShowingPopup() const {
  return popup_host_ != nullptr;
}

void ExtensionActionViewController::HidePopup() {
  if (IsShowingPopup()) {
    // Only call Close() on the popup if it's been shown; otherwise, the popup
    // will be cleaned up in ShowPopup().
    if (has_opened_popup_)
      popup_host_->Close();
    // We need to do these actions synchronously (instead of closing and then
    // performing the rest of the cleanup in OnExtensionHostDestroyed()) because
    // the extension host may close asynchronously, and we need to keep the view
    // delegate up to date.
    if (popup_host_)
      OnPopupClosed();
  }
}

gfx::NativeView ExtensionActionViewController::GetPopupNativeView() {
  return popup_host_ ? popup_host_->view()->GetNativeView() : nullptr;
}

ui::MenuModel* ExtensionActionViewController::GetContextMenu(
    extensions::ExtensionContextMenuModel::ContextMenuSource
        context_menu_source) {
  if (!ExtensionIsValid())
    return nullptr;

  bool is_pinned =
      ToolbarActionsModel::Get(browser_->profile())->IsActionPinned(GetId());

  // Reconstruct the menu every time because the menu's contents are dynamic.
  context_menu_model_ = std::make_unique<extensions::ExtensionContextMenuModel>(
      extension(), browser_, is_pinned, this,
      ToolbarActionsModel::CanShowActionsInToolbar(*browser_),
      context_menu_source);
  return context_menu_model_.get();
}

void ExtensionActionViewController::OnContextMenuShown(
    extensions::ExtensionContextMenuModel::ContextMenuSource source) {
  if (source == extensions::ExtensionContextMenuModel::ContextMenuSource::
                    kToolbarAction) {
    extensions_container_->OnContextMenuShownFromToolbar(GetId());
  }
}

void ExtensionActionViewController::OnContextMenuClosed(
    extensions::ExtensionContextMenuModel::ContextMenuSource source) {
  if (source == extensions::ExtensionContextMenuModel::ContextMenuSource::
                    kToolbarAction) {
    extensions_container_->OnContextMenuClosedFromToolbar();
  }
}

void ExtensionActionViewController::ExecuteUserAction(InvocationSource source) {
  if (!ExtensionIsValid())
    return;

  if (!IsEnabled(view_delegate_->GetCurrentWebContents())) {
    GetPreferredPopupViewController()
        ->view_delegate_->ShowContextMenuAsFallback();
    return;
  }

  content::WebContents* const web_contents =
      view_delegate_->GetCurrentWebContents();
  ExtensionActionRunner* action_runner =
      ExtensionActionRunner::GetForWebContents(web_contents);
  if (!action_runner)
    return;

  RecordInvocationSource(source);

  extensions_container_->CloseOverflowMenuIfOpen();

  // This method is only called to execute an action by the user, so we can
  // always grant tab permissions.
  constexpr bool kGrantTabPermissions = true;
  extensions::ExtensionAction::ShowAction action =
      action_runner->RunAction(extension(), kGrantTabPermissions);

  if (action == extensions::ExtensionAction::ShowAction::kShowPopup) {
    constexpr bool kByUser = true;
    GetPreferredPopupViewController()->TriggerPopup(
        PopupShowAction::kShow, kByUser, ShowPopupCallback());
  } else if (action ==
             extensions::ExtensionAction::ShowAction::kToggleSidePanel) {
    extensions::side_panel_util::ToggleExtensionSidePanel(browser_,
                                                          extension()->id());
  }
}

void ExtensionActionViewController::TriggerPopupForAPI(
    ShowPopupCallback callback) {
  RecordInvocationSource(InvocationSource::kApi);
  // This method is called programmatically by an API; it should never be
  // considered a user action.
  constexpr bool kByUser = false;
  TriggerPopup(PopupShowAction::kShow, kByUser, std::move(callback));
}

void ExtensionActionViewController::UpdateState() {
  if (!ExtensionIsValid())
    return;

  view_delegate_->UpdateState();
}

void ExtensionActionViewController::UpdateHoverCard(
    ToolbarActionView* action_view,
    ToolbarActionHoverCardUpdateType update_type) {
  if (!ExtensionIsValid())
    return;

  extensions_container_->UpdateToolbarActionHoverCard(action_view, update_type);
}

void ExtensionActionViewController::RegisterCommand() {
  if (!ExtensionIsValid())
    return;

  platform_delegate_->RegisterCommand();
}

void ExtensionActionViewController::UnregisterCommand() {
  platform_delegate_->UnregisterCommand();
}

void ExtensionActionViewController::InspectPopup() {
  // This method is only triggered through user action (clicking on the context
  // menu entry).
  GetPreferredPopupViewController()->TriggerPopup(
      PopupShowAction::kShowAndInspect, /*by_user*/ true, ShowPopupCallback());
}

void ExtensionActionViewController::TriggerPopupForAPI() {
  GetPreferredPopupViewController()->TriggerPopup(
      PopupShowAction::kShowAndInspect, /*by_user*/ false, ShowPopupCallback());
}

void ExtensionActionViewController::OnIconUpdated() {
  // We update the view first, so that if the observer relies on its UI it can
  // be ready.
  if (view_delegate_)
    view_delegate_->UpdateState();
}

void ExtensionActionViewController::OnExtensionHostDestroyed(
    extensions::ExtensionHost* host) {
  OnPopupClosed();
}

extensions::SitePermissionsHelper::SiteInteraction
ExtensionActionViewController::GetSiteInteraction(
    content::WebContents* web_contents) const {
  return extensions::SitePermissionsHelper(browser_->profile())
      .GetSiteInteraction(*extension(), web_contents);
}

bool ExtensionActionViewController::ExtensionIsValid() const {
  return extension_registry_->enabled_extensions().Contains(extension_->id());
}

bool ExtensionActionViewController::GetExtensionCommand(
    extensions::Command* command) const {
  DCHECK(command);
  if (!ExtensionIsValid())
    return false;

  CommandService* command_service = CommandService::Get(browser_->profile());
  return command_service->GetExtensionActionCommand(
      extension_->id(), extension_action_->action_type(),
      CommandService::ACTIVE, command, nullptr);
}

ToolbarActionViewController::HoverCardState
ExtensionActionViewController::GetHoverCardState(
    content::WebContents* web_contents) const {
  DCHECK(ExtensionIsValid());
  DCHECK(web_contents);

  url::Origin origin =
      web_contents->GetPrimaryMainFrame()->GetLastCommittedOrigin();
  extensions::PermissionsManager::UserSiteSetting site_setting =
      extensions::PermissionsManager::Get(browser_->profile())
          ->GetUserSiteSetting(origin);
  auto site_interaction = GetSiteInteraction(web_contents);

  HoverCardState state;
  state.site_access =
      GetHoverCardSiteAccessState(site_setting, site_interaction);
  state.policy = GetHoverCardPolicyState(browser_, GetId());

  return state;
}

bool ExtensionActionViewController::CanHandleAccelerators() const {
  if (!ExtensionIsValid())
    return false;

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
    return IsEnabled(view_delegate_->GetCurrentWebContents());
  }
  return true;
}

std::unique_ptr<IconWithBadgeImageSource>
ExtensionActionViewController::GetIconImageSourceForTesting(
    content::WebContents* web_contents,
    const gfx::Size& size) {
  return GetIconImageSource(web_contents, size);
}

ExtensionActionViewController*
ExtensionActionViewController::GetPreferredPopupViewController() {
  return static_cast<ExtensionActionViewController*>(
      extensions_container_->GetActionForId(GetId()));
}

void ExtensionActionViewController::TriggerPopup(PopupShowAction show_action,
                                                 bool by_user,
                                                 ShowPopupCallback callback) {
  DCHECK(ExtensionIsValid());
  DCHECK_EQ(this, GetPreferredPopupViewController());

  content::WebContents* const web_contents =
      view_delegate_->GetCurrentWebContents();
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

  // Always hide the current popup, even if it's not owned by this extension.
  // Only one popup should be visible at a time.
  extensions_container_->HideActivePopup();

  extensions_container_->CloseOverflowMenuIfOpen();

  popup_host_ = host.get();
  popup_host_observation_.Observe(popup_host_.get());
  extensions_container_->SetPopupOwner(this);

  extensions_container_->PopOutAction(
      GetId(), base::BindOnce(&ExtensionActionViewController::ShowPopup,
                              weak_factory_.GetWeakPtr(), std::move(host),
                              by_user, show_action, std::move(callback)));
}

void ExtensionActionViewController::ShowPopup(
    std::unique_ptr<extensions::ExtensionViewHost> popup_host,
    bool by_user,
    PopupShowAction show_action,
    ShowPopupCallback callback) {
  // It's possible that the popup should be closed before it finishes opening
  // (since it can open asynchronously). Check before proceeding.
  if (!popup_host_) {
    if (callback)
      std::move(callback).Run(nullptr);
    return;
  }
  // NOTE: Today, ShowPopup() always synchronously creates the platform-specific
  // popup class, which is what we care most about (since `has_opened_popup_`
  // is used to determine whether we need to manually close the
  // ExtensionViewHost). This doesn't necessarily mean that the popup has
  // completed rendering on the screen.
  has_opened_popup_ = true;
  platform_delegate_->ShowPopup(std::move(popup_host), show_action,
                                std::move(callback));
  view_delegate_->OnPopupShown(by_user);
}

void ExtensionActionViewController::OnPopupClosed() {
  DCHECK(popup_host_observation_.IsObservingSource(popup_host_.get()));
  popup_host_observation_.Reset();
  popup_host_ = nullptr;
  has_opened_popup_ = false;
  extensions_container_->SetPopupOwner(nullptr);
  if (extensions_container_->GetPoppedOutActionId() == GetId()) {
    extensions_container_->UndoPopOut();
  }
  view_delegate_->OnPopupClosed();
}

std::unique_ptr<IconWithBadgeImageSource>
ExtensionActionViewController::GetIconImageSource(
    content::WebContents* web_contents,
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

  extensions::SidePanelService* side_panel_service =
      extensions::SidePanelService::Get(browser_->profile());
  bool has_side_panel_action =
      side_panel_service &&
      side_panel_service->HasSidePanelActionForTab(*extension(), tab_id);
  bool grayscale =
      GetSiteInteraction(web_contents) ==
          extensions::SitePermissionsHelper::SiteInteraction::kNone &&
      !action_is_visible && !has_side_panel_action;
  image_source->set_grayscale(grayscale);

  if (base::FeatureList::IsEnabled(
          extensions_features::kExtensionsMenuAccessControl)) {
    return image_source;
  }

  bool was_blocked = extensions::SitePermissionsHelper(browser_->profile())
                         .HasBeenBlocked(*extension(), web_contents);
  image_source->set_paint_blocked_actions_decoration(was_blocked);

  return image_source;
}
