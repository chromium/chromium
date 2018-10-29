// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/extensions/extension_action_view_controller.h"

#include <utility>

#include "base/logging.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/api/commands/command_service.h"
#include "chrome/browser/extensions/api/extension_action/extension_action_api.h"
#include "chrome/browser/extensions/extension_action.h"
#include "chrome/browser/extensions/extension_action_runner.h"
#include "chrome/browser/extensions/extension_view.h"
#include "chrome/browser/extensions/extension_view_host.h"
#include "chrome/browser/extensions/extension_view_host_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/extensions/accelerator_priority.h"
#include "chrome/browser/ui/extensions/extension_action_platform_delegate.h"
#include "chrome/browser/ui/extensions/icon_with_badge_image_source.h"
#include "chrome/browser/ui/toolbar/toolbar_action_view_delegate.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_bar.h"
#include "chrome/common/extensions/api/extension_action/action_info.h"
#include "chrome/grit/generated_resources.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/manifest_constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"

using extensions::ActionInfo;
using extensions::CommandService;
using extensions::ExtensionActionRunner;

ExtensionActionViewController::ExtensionActionViewController(
    const extensions::Extension* extension,
    Browser* browser,
    ExtensionAction* extension_action,
    ToolbarActionsBar* toolbar_actions_bar)
    : extension_(extension),
      browser_(browser),
      extension_action_(extension_action),
      toolbar_actions_bar_(toolbar_actions_bar),
      popup_host_(nullptr),
      view_delegate_(nullptr),
      platform_delegate_(ExtensionActionPlatformDelegate::Create(this)),
      icon_factory_(browser->profile(), extension, extension_action, this),
      extension_registry_(
          extensions::ExtensionRegistry::Get(browser_->profile())),
      popup_host_observer_(this),
      weak_factory_(this) {
  DCHECK(extension_action);
  DCHECK(extension_action->action_type() == ActionInfo::TYPE_PAGE ||
         extension_action->action_type() == ActionInfo::TYPE_BROWSER);
  DCHECK(extension);
}

ExtensionActionViewController::~ExtensionActionViewController() {
  DCHECK(!is_showing_popup());
}

std::string ExtensionActionViewController::GetId() const {
  return extension_->id();
}

void ExtensionActionViewController::SetDelegate(
    ToolbarActionViewDelegate* delegate) {
  DCHECK((delegate == nullptr) ^ (view_delegate_ == nullptr));
  if (delegate) {
    view_delegate_ = delegate;
    platform_delegate_->OnDelegateSet();
  } else {
    if (is_showing_popup())
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

base::string16 ExtensionActionViewController::GetActionName() const {
  if (!ExtensionIsValid())
    return base::string16();

  return base::UTF8ToUTF16(extension_->name());
}

base::string16 ExtensionActionViewController::GetAccessibleName(
    content::WebContents* web_contents) const {
  if (!ExtensionIsValid())
    return base::string16();

  // GetAccessibleName() can (surprisingly) be called during browser
  // teardown. Handle this gracefully.
  if (!web_contents)
    return base::UTF8ToUTF16(extension()->name());

  std::string title = extension_action()->GetTitle(
      SessionTabHelper::IdForTab(web_contents).id());

  base::string16 title_utf16 =
      base::UTF8ToUTF16(title.empty() ? extension()->name() : title);

  // With runtime host permissions, include a "host access" portion of the
  // tooltip if the extension has or wants access to the site.
  if (base::FeatureList::IsEnabled(
          extensions_features::kRuntimeHostPermissions)) {
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
          {title_utf16, base::UTF8ToUTF16("\n"),
           l10n_util::GetStringUTF16(interaction_status_description_id)});
    }
  }

  return title_utf16;
}

base::string16 ExtensionActionViewController::GetTooltip(
    content::WebContents* web_contents) const {
  return GetAccessibleName(web_contents);
}

bool ExtensionActionViewController::IsEnabled(
    content::WebContents* web_contents) const {
  if (!ExtensionIsValid())
    return false;

  return extension_action_->GetIsVisible(
             SessionTabHelper::IdForTab(web_contents).id()) ||
         HasBeenBlocked(web_contents);
}

bool ExtensionActionViewController::WantsToRun(
    content::WebContents* web_contents) const {
  return ExtensionIsValid() &&
         (PageActionWantsToRun(web_contents) || HasBeenBlocked(web_contents));
}

bool ExtensionActionViewController::HasPopup(
    content::WebContents* web_contents) const {
  if (!ExtensionIsValid())
    return false;

  SessionID tab_id = SessionTabHelper::IdForTab(web_contents);
  return tab_id.is_valid() ? extension_action_->HasPopup(tab_id.id()) : false;
}

void ExtensionActionViewController::HidePopup() {
  if (is_showing_popup()) {
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

  extensions::ExtensionContextMenuModel::ButtonVisibility visibility =
      extensions::ExtensionContextMenuModel::VISIBLE;
  if (toolbar_actions_bar_) {
    if (toolbar_actions_bar_->popped_out_action() == this)
      visibility = extensions::ExtensionContextMenuModel::TRANSITIVELY_VISIBLE;
    else if (!toolbar_actions_bar_->IsActionVisibleOnMainBar(this))
      visibility = extensions::ExtensionContextMenuModel::OVERFLOWED;
    // Else, VISIBLE is correct.
  }
  // Reconstruct the menu every time because the menu's contents are dynamic.
  context_menu_model_.reset(new extensions::ExtensionContextMenuModel(
      extension(), browser_, visibility, this));
  return context_menu_model_.get();
}

void ExtensionActionViewController::OnContextMenuClosed() {
  if (toolbar_actions_bar_ &&
      toolbar_actions_bar_->popped_out_action() == this &&
      !is_showing_popup()) {
    toolbar_actions_bar_->UndoPopOut();
  }
}

bool ExtensionActionViewController::ExecuteAction(bool by_user) {
  if (!ExtensionIsValid())
    return false;

  if (!IsEnabled(view_delegate_->GetCurrentWebContents())) {
    if (DisabledClickOpensMenu())
      GetPreferredPopupViewController()->platform_delegate_->ShowContextMenu();
    return false;
  }

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

  if (action_runner->RunAction(extension(), grant_tab_permissions) ==
      ExtensionAction::ACTION_SHOW_POPUP) {
    GURL popup_url = extension_action_->GetPopupUrl(
        SessionTabHelper::IdForTab(web_contents).id());
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

bool ExtensionActionViewController::DisabledClickOpensMenu() const {
  return true;
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
    const extensions::ExtensionHost* host) {
  OnPopupClosed();
}

ExtensionActionViewController::PageInteractionStatus
ExtensionActionViewController::GetPageInteractionStatus(
    content::WebContents* web_contents) const {
  // The |web_contents| can be null, if TabStripModel::GetActiveWebContents()
  // returns null. In that case, default to kNone.
  if (!web_contents)
    return PageInteractionStatus::kNone;

  // We give priority to kPending, because it's the one that's most important
  // for users to see.
  if (HasBeenBlocked(web_contents))
    return PageInteractionStatus::kPending;

  // NOTE(devlin): We could theoretically adjust this to only be considered
  // active if the extension *did* act on the page, rather than if it *could*.
  // This is a bit more complex, and it's unclear if this is a better UX, since
  // it would lead to much less determinism in terms of what extensions look
  // like on a given host.
  const int tab_id = SessionTabHelper::IdForTab(web_contents).id();
  const GURL& url = web_contents->GetLastCommittedURL();
  if (extension_->permissions_data()->GetPageAccess(url, tab_id,
                                                    /*error=*/nullptr) ==
          extensions::PermissionsData::PageAccess::kAllowed ||
      extension_->permissions_data()->GetContentScriptAccess(
          url, tab_id, /*error=*/nullptr) ==
          extensions::PermissionsData::PageAccess::kAllowed) {
    return PageInteractionStatus::kActive;
  }

  return PageInteractionStatus::kNone;
}

bool ExtensionActionViewController::ExtensionIsValid() const {
  return extension_registry_->enabled_extensions().Contains(extension_->id());
}

void ExtensionActionViewController::HideActivePopup() {
  if (toolbar_actions_bar_) {
    toolbar_actions_bar_->HideActivePopup();
  } else {
    DCHECK_EQ(ActionInfo::TYPE_PAGE, extension_action_->action_type());
    // In the traditional toolbar, page actions only know how to close their own
    // popups.
    HidePopup();
  }
}

bool ExtensionActionViewController::GetExtensionCommand(
    extensions::Command* command) {
  DCHECK(command);
  if (!ExtensionIsValid())
    return false;

  CommandService* command_service = CommandService::Get(browser_->profile());
  if (extension_action_->action_type() == ActionInfo::TYPE_PAGE) {
    return command_service->GetPageActionCommand(
        extension_->id(), CommandService::ACTIVE, command, NULL);
  }
  return command_service->GetBrowserActionCommand(
      extension_->id(), CommandService::ACTIVE, command, NULL);
}

std::unique_ptr<IconWithBadgeImageSource>
ExtensionActionViewController::GetIconImageSourceForTesting(
    content::WebContents* web_contents,
    const gfx::Size& size) {
  return GetIconImageSource(web_contents, size);
}

ExtensionActionViewController*
ExtensionActionViewController::GetPreferredPopupViewController() {
  if (toolbar_actions_bar_ && toolbar_actions_bar_->in_overflow_mode()) {
    return static_cast<ExtensionActionViewController*>(
        toolbar_actions_bar_->GetMainControllerForAction(this));
  }

  return this;
}

bool ExtensionActionViewController::TriggerPopupWithUrl(
    PopupShowAction show_action,
    const GURL& popup_url,
    bool grant_tab_permissions) {
  if (!ExtensionIsValid())
    return false;

  bool already_showing = is_showing_popup();

  // Always hide the current popup, even if it's not owned by this extension.
  // Only one popup should be visible at a time.
  HideActivePopup();

  // If we were showing a popup already, then we treat the action to open the
  // same one as a desire to close it (like clicking a menu button that was
  // already open).
  if (already_showing)
    return false;

  std::unique_ptr<extensions::ExtensionViewHost> host(
      extensions::ExtensionViewHostFactory::CreatePopupHost(popup_url,
                                                            browser_));
  if (!host)
    return false;

  popup_host_ = host.get();
  popup_host_observer_.Add(popup_host_);
  if (toolbar_actions_bar_)
    toolbar_actions_bar_->SetPopupOwner(this);

  if (toolbar_actions_bar_ &&
      !toolbar_actions_bar_->IsActionVisibleOnMainBar(this)) {
    toolbar_actions_bar_->CloseOverflowMenuIfOpen();
    toolbar_actions_bar_->PopOutAction(
        this,
        show_action == SHOW_POPUP_AND_INSPECT,
        base::Bind(&ExtensionActionViewController::ShowPopup,
                   weak_factory_.GetWeakPtr(), base::Passed(std::move(host)),
                   grant_tab_permissions, show_action));
  } else {
    ShowPopup(std::move(host), grant_tab_permissions, show_action);
  }

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
  popup_host_observer_.Remove(popup_host_);
  popup_host_ = nullptr;
  if (toolbar_actions_bar_) {
    toolbar_actions_bar_->SetPopupOwner(nullptr);
    if (toolbar_actions_bar_->popped_out_action() == this &&
        !view_delegate_->IsMenuRunning())
      toolbar_actions_bar_->UndoPopOut();
  }
  view_delegate_->OnPopupClosed();
}

std::unique_ptr<IconWithBadgeImageSource>
ExtensionActionViewController::GetIconImageSource(
    content::WebContents* web_contents,
    const gfx::Size& size) {
  int tab_id = SessionTabHelper::IdForTab(web_contents).id();
  std::unique_ptr<IconWithBadgeImageSource> image_source(
      new IconWithBadgeImageSource(size));

  image_source->SetIcon(icon_factory_.GetIcon(tab_id));

  std::unique_ptr<IconWithBadgeImageSource::Badge> badge;
  std::string badge_text = extension_action_->GetBadgeText(tab_id);
  if (!badge_text.empty()) {
    badge.reset(new IconWithBadgeImageSource::Badge(
            badge_text,
            extension_action_->GetBadgeTextColor(tab_id),
            extension_action_->GetBadgeBackgroundColor(tab_id)));
  }
  image_source->SetBadge(std::move(badge));

  bool grayscale = false;
  bool was_blocked = false;
  bool action_is_visible = extension_action_->GetIsVisible(tab_id);
  if (base::FeatureList::IsEnabled(
          extensions_features::kRuntimeHostPermissions)) {
    PageInteractionStatus interaction_status =
        GetPageInteractionStatus(web_contents);
    // With the runtime host permissions feature, we only grayscale the icon if
    // it cannot interact with the page and the icon is disabled.
    grayscale = interaction_status == PageInteractionStatus::kNone &&
                !action_is_visible;
    was_blocked = interaction_status == PageInteractionStatus::kPending;
  } else {
    // Without runtime host permissions enabled, grayscaling is purely used to
    // indicate "clickability", and not any kind of access.
    grayscale = !action_is_visible;
    // was_blocked is always false without runtime host permissions.
  }

  image_source->set_grayscale(grayscale);
  image_source->set_paint_blocked_actions_decoration(was_blocked);

  // If the action has an active page action on the web contents and is also
  // overflowed, we add a decoration so that the user can see which overflowed
  // action wants to run (since they wouldn't be able to see the change from
  // grayscale to color).
  bool is_overflow =
      toolbar_actions_bar_ && toolbar_actions_bar_->in_overflow_mode();
  image_source->set_paint_page_action_decoration(
      !was_blocked && is_overflow && PageActionWantsToRun(web_contents));

  return image_source;
}

bool ExtensionActionViewController::PageActionWantsToRun(
    content::WebContents* web_contents) const {
  return extension_action_->action_type() ==
             extensions::ActionInfo::TYPE_PAGE &&
         extension_action_->GetIsVisible(
             SessionTabHelper::IdForTab(web_contents).id());
}

bool ExtensionActionViewController::HasBeenBlocked(
    content::WebContents* web_contents) const {
  ExtensionActionRunner* action_runner =
      ExtensionActionRunner::GetForWebContents(web_contents);
  return action_runner && action_runner->WantsToRun(extension());
}
