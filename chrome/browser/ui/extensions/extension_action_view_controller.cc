// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/extensions/extension_action_view_controller.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/check_op.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/api/commands/command_service.h"
#include "chrome/browser/extensions/api/extension_action/extension_action_api.h"
#include "chrome/browser/extensions/extension_action_runner.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/extension_view.h"
#include "chrome/browser/extensions/extension_view_host.h"
#include "chrome/browser/extensions/extension_view_host_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/extensions/extension_action_platform_delegate.h"
#include "chrome/browser/ui/extensions/extensions_container.h"
#include "chrome/browser/ui/extensions/icon_with_badge_image_source.h"
#include "chrome/browser/ui/toolbar/toolbar_action_view_delegate.h"
#include "chrome/grit/generated_resources.h"
#include "components/sessions/content/session_tab_helper.h"
#include "extensions/browser/extension_action.h"
#include "extensions/browser/extension_action_manager.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/api/extension_action/action_info.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/permissions/api_permission.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"

using extensions::ActionInfo;
using extensions::CommandService;
using extensions::ExtensionActionRunner;

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
      icon_factory_(browser->profile(),
                    extension_.get(),
                    extension_action,
                    this),
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

gfx::Image ExtensionActionViewController::GetIcon(
    content::WebContents* web_contents,
    const gfx::Size& size) {
  if (!ExtensionIsValid())
    return gfx::Image();

  return gfx::Image(
      gfx::ImageSkia(GetIconImageSource(web_contents, size), size));
}

std::u16string ExtensionActionViewController::GetActionName() const {
  if (!ExtensionIsValid())
    return std::u16string();

  return base::UTF8ToUTF16(extension_->name());
}

std::u16string ExtensionActionViewController::GetAccessibleName(
    content::WebContents* web_contents) const {
  if (!ExtensionIsValid())
    return std::u16string();

  // GetAccessibleName() can (surprisingly) be called during browser
  // teardown. Handle this gracefully.
  if (!web_contents)
    return base::UTF8ToUTF16(extension()->name());

  std::string title = extension_action()->GetTitle(
      sessions::SessionTabHelper::IdForTab(web_contents).id());

  std::u16string title_utf16 =
      base::UTF8ToUTF16(title.empty() ? extension()->name() : title);

  // Include a "host access" portion of the tooltip if the extension has or
  // wants access to the site.
  PageInteractionStatus interaction_status =
      GetPageInteractionStatus(web_contents);
  int interaction_status_description_id = -1;
  switch (interaction_status) {
    case PageInteractionStatus::kNone:
      // No string for neither having nor wanting access.
      break;
    case PageInteractionStatus::kPending:
      interaction_status_description_id = IDS_EXTENSIONS_WANTS_ACCESS_TO_SITE;
      break;
    case PageInteractionStatus::kActive:
      interaction_status_description_id = IDS_EXTENSIONS_HAS_ACCESS_TO_SITE;
      break;
  }

  if (interaction_status_description_id != -1) {
    title_utf16 = base::StrCat(
        {title_utf16, u"\n",
         l10n_util::GetStringUTF16(interaction_status_description_id)});
  }

  return title_utf16;
}

std::u16string ExtensionActionViewController::GetTooltip(
    content::WebContents* web_contents) const {
  return GetAccessibleName(web_contents);
}

bool ExtensionActionViewController::IsEnabled(
    content::WebContents* web_contents) const {
  if (!ExtensionIsValid())
    return false;

  return extension_action_->GetIsVisible(
             sessions::SessionTabHelper::IdForTab(web_contents).id()) ||
         GetPageInteractionStatus(web_contents) ==
             PageInteractionStatus::kPending;
}

bool ExtensionActionViewController::IsShowingPopup() const {
  return popup_host_ != nullptr;
}

void ExtensionActionViewController::HidePopup() {
  if (IsShowingPopup()) {
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

ui::MenuModel* ExtensionActionViewController::GetContextMenu() {
  if (!ExtensionIsValid())
    return nullptr;

  ToolbarActionViewController* const action =
      extensions_container_->GetActionForId(GetId());
  extensions::ExtensionContextMenuModel::ButtonVisibility visibility =
      extensions_container_->GetActionVisibility(action);

  // Reconstruct the menu every time because the menu's contents are dynamic.
  context_menu_model_ = std::make_unique<extensions::ExtensionContextMenuModel>(
      extension(), browser_, visibility, this,
      view_delegate_->CanShowIconInToolbar());
  return context_menu_model_.get();
}

void ExtensionActionViewController::OnContextMenuShown() {
  extensions_container_->OnContextMenuShown(this);
}

void ExtensionActionViewController::OnContextMenuClosed() {
  extensions_container_->OnContextMenuClosed(this);
}

bool ExtensionActionViewController::ExecuteAction(bool by_user,
                                                  InvocationSource source) {
  if (!ExtensionIsValid())
    return false;

  if (!IsEnabled(view_delegate_->GetCurrentWebContents())) {
    GetPreferredPopupViewController()
        ->view_delegate_->ShowContextMenuAsFallback();
    return false;
  }

  base::UmaHistogramEnumeration("Extensions.Toolbar.InvocationSource", source);
  return ExecuteAction(SHOW_POPUP, by_user);
}

void ExtensionActionViewController::UpdateState() {
  if (!ExtensionIsValid())
    return;

  view_delegate_->UpdateState();
}

bool ExtensionActionViewController::ExecuteAction(PopupShowAction show_action,
                                                  bool grant_tab_permissions) {
  if (!ExtensionIsValid())
    return false;

  content::WebContents* web_contents = view_delegate_->GetCurrentWebContents();
  ExtensionActionRunner* action_runner =
      ExtensionActionRunner::GetForWebContents(web_contents);
  if (!action_runner)
    return false;

  extensions_container_->CloseOverflowMenuIfOpen();

  if (action_runner->RunAction(extension(), grant_tab_permissions) ==
      extensions::ExtensionAction::ACTION_SHOW_POPUP) {
    GURL popup_url = extension_action_->GetPopupUrl(
        sessions::SessionTabHelper::IdForTab(web_contents).id());
    return GetPreferredPopupViewController()
        ->TriggerPopupWithUrl(show_action, popup_url, grant_tab_permissions);
  }
  return false;
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
  ExecuteAction(SHOW_POPUP_AND_INSPECT, true);
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

ExtensionActionViewController::PageInteractionStatus
ExtensionActionViewController::GetPageInteractionStatus(
    content::WebContents* web_contents) const {
  // The |web_contents| can be null, if TabStripModel::GetActiveWebContents()
  // returns null. In that case, default to kNone.
  if (!web_contents)
    return PageInteractionStatus::kNone;

  const int tab_id = sessions::SessionTabHelper::IdForTab(web_contents).id();
  const GURL& url = web_contents->GetLastCommittedURL();
  extensions::PermissionsData::PageAccess page_access =
      extension_->permissions_data()->GetPageAccess(url, tab_id,
                                                    /*error=*/nullptr);
  extensions::PermissionsData::PageAccess script_access =
      extension_->permissions_data()->GetContentScriptAccess(url, tab_id,
                                                             /*error=*/nullptr);
  if (page_access == extensions::PermissionsData::PageAccess::kAllowed ||
      script_access == extensions::PermissionsData::PageAccess::kAllowed) {
    return PageInteractionStatus::kActive;
  }
  // TODO(tjudkins): Investigate if we need to check HasBeenBlocked() for this
  // case. We do know that extensions that have been blocked should always be
  // marked pending, but those cases should be covered by the withheld page
  // access checks.
  if (page_access == extensions::PermissionsData::PageAccess::kWithheld ||
      script_access == extensions::PermissionsData::PageAccess::kWithheld ||
      HasBeenBlocked(web_contents) || HasActiveTabAndCanAccess(url)) {
    return PageInteractionStatus::kPending;
  }

  return PageInteractionStatus::kNone;
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
  if (extension_action_->action_type() == extensions::ActionInfo::TYPE_PAGE)
    return IsEnabled(view_delegate_->GetCurrentWebContents());
  return true;
}

std::unique_ptr<IconWithBadgeImageSource>
ExtensionActionViewController::GetIconImageSourceForTesting(
    content::WebContents* web_contents,
    const gfx::Size& size) {
  return GetIconImageSource(web_contents, size);
}

bool ExtensionActionViewController::HasBeenBlockedForTesting(
    content::WebContents* web_contents) const {
  return HasBeenBlocked(web_contents);
}

ExtensionActionViewController*
ExtensionActionViewController::GetPreferredPopupViewController() {
  return static_cast<ExtensionActionViewController*>(
      extensions_container_->GetActionForId(GetId()));
}

bool ExtensionActionViewController::TriggerPopupWithUrl(
    PopupShowAction show_action,
    const GURL& popup_url,
    bool grant_tab_permissions) {
  if (!ExtensionIsValid())
    return false;

  // Always hide the current popup, even if it's not owned by this extension.
  // Only one popup should be visible at a time.
  extensions_container_->HideActivePopup();

  std::unique_ptr<extensions::ExtensionViewHost> host =
      extensions::ExtensionViewHostFactory::CreatePopupHost(popup_url,
                                                            browser_);
  if (!host)
    return false;

  popup_host_ = host.get();
  popup_host_observation_.Observe(popup_host_);
  extensions_container_->SetPopupOwner(this);

  extensions_container_->CloseOverflowMenuIfOpen();
  extensions_container_->PopOutAction(
      this, show_action == SHOW_POPUP_AND_INSPECT,
      base::BindOnce(&ExtensionActionViewController::ShowPopup,
                     weak_factory_.GetWeakPtr(), std::move(host),
                     grant_tab_permissions, show_action));

  return true;
}

void ExtensionActionViewController::ShowPopup(
    std::unique_ptr<extensions::ExtensionViewHost> popup_host,
    bool grant_tab_permissions,
    PopupShowAction show_action) {
  // It's possible that the popup should be closed before it finishes opening
  // (since it can open asynchronously). Check before proceeding.
  if (!popup_host_)
    return;
  platform_delegate_->ShowPopup(std::move(popup_host), grant_tab_permissions,
                                show_action);
  view_delegate_->OnPopupShown(grant_tab_permissions);
}

void ExtensionActionViewController::OnPopupClosed() {
  DCHECK(popup_host_observation_.IsObservingSource(popup_host_));
  popup_host_observation_.Reset();
  popup_host_ = nullptr;
  extensions_container_->SetPopupOwner(nullptr);
  if (extensions_container_->GetPoppedOutAction() == this)
    extensions_container_->UndoPopOut();
  view_delegate_->OnPopupClosed();
}

std::unique_ptr<IconWithBadgeImageSource>
ExtensionActionViewController::GetIconImageSource(
    content::WebContents* web_contents,
    const gfx::Size& size) {
  int tab_id = sessions::SessionTabHelper::IdForTab(web_contents).id();
  std::unique_ptr<IconWithBadgeImageSource> image_source(
      new IconWithBadgeImageSource(size));

  image_source->SetIcon(icon_factory_.GetIcon(tab_id));

  std::unique_ptr<IconWithBadgeImageSource::Badge> badge;
  std::string badge_text = extension_action_->GetDisplayBadgeText(tab_id);
  if (!badge_text.empty()) {
    badge = std::make_unique<IconWithBadgeImageSource::Badge>(
        badge_text, extension_action_->GetBadgeTextColor(tab_id),
        extension_action_->GetBadgeBackgroundColor(tab_id));
  }
  image_source->SetBadge(std::move(badge));

  bool grayscale = false;
  bool was_blocked = false;
  bool action_is_visible = extension_action_->GetIsVisible(tab_id);
  PageInteractionStatus interaction_status =
      GetPageInteractionStatus(web_contents);
  // We only grayscale the icon if it cannot interact with the page and the icon
  // is disabled.
  grayscale =
      interaction_status == PageInteractionStatus::kNone && !action_is_visible;
  was_blocked = HasBeenBlocked(web_contents);

  image_source->set_grayscale(grayscale);
  image_source->set_paint_blocked_actions_decoration(was_blocked);

  return image_source;
}

bool ExtensionActionViewController::HasActiveTabAndCanAccess(
    const GURL& url) const {
  return extension_->permissions_data()->HasAPIPermission(
             extensions::mojom::APIPermissionID::kActiveTab) &&
         !extension_->permissions_data()->IsRestrictedUrl(url,
                                                          /*error=*/nullptr) &&
         (!url.SchemeIsFile() || extensions::util::AllowFileAccess(
                                     extension_->id(), browser_->profile()));
}

bool ExtensionActionViewController::HasBeenBlocked(
    content::WebContents* web_contents) const {
  ExtensionActionRunner* action_runner =
      ExtensionActionRunner::GetForWebContents(web_contents);
  return action_runner && action_runner->WantsToRun(extension());
}
