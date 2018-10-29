// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toolbar/media_router_action.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "chrome/browser/media/router/media_router.h"
#include "chrome/browser/media/router/media_router_factory.h"
#include "chrome/browser/media/router/media_router_metrics.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/media_router/media_router_dialog_controller_impl_base.h"
#include "chrome/browser/ui/media_router/media_router_ui_service.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/toolbar/component_toolbar_actions_factory.h"
#include "chrome/browser/ui/toolbar/media_router_action_controller.h"
#include "chrome/browser/ui/toolbar/toolbar_action_view_delegate.h"
#include "chrome/common/media_router/issue.h"
#include "chrome/common/media_router/media_route.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icon_types.h"

using media_router::MediaRouterDialogControllerImplBase;

namespace {

media_router::MediaRouter* GetMediaRouter(Browser* browser) {
  return media_router::MediaRouterFactory::GetApiForBrowserContext(
      browser->profile());
}

}  // namespace

MediaRouterAction::MediaRouterAction(Browser* browser,
                                     ToolbarActionsBar* toolbar_actions_bar)
    : media_router::IssuesObserver(GetMediaRouter(browser)->GetIssueManager()),
      media_router::MediaRoutesObserver(GetMediaRouter(browser)),
      current_icon_(&vector_icons::kMediaRouterIdleIcon),
      has_local_display_route_(false),
      has_dialog_(false),
      delegate_(nullptr),
      browser_(browser),
      toolbar_actions_bar_(toolbar_actions_bar),
      tab_strip_model_observer_(this),
      toolbar_actions_bar_observer_(this),
      skip_close_overflow_menu_for_testing_(false),
      weak_ptr_factory_(this) {
  DCHECK(browser_);
  DCHECK(toolbar_actions_bar_);
  tab_strip_model_observer_.Add(browser_->tab_strip_model());
  toolbar_actions_bar_observer_.Add(toolbar_actions_bar_);
  IssuesObserver::Init();
}

MediaRouterAction::~MediaRouterAction() {
}

// static
SkColor MediaRouterAction::GetIconColor(const gfx::VectorIcon& icon_id) {
  if (&icon_id == &vector_icons::kMediaRouterIdleIcon)
    return gfx::kChromeIconGrey;
  else if (&icon_id == &vector_icons::kMediaRouterActiveIcon)
    return gfx::kGoogleBlue500;
  else if (&icon_id == &vector_icons::kMediaRouterWarningIcon)
    return gfx::kGoogleYellow700;
  else if (&icon_id == &vector_icons::kMediaRouterErrorIcon)
    return gfx::kGoogleRed700;

  NOTREACHED();
  return gfx::kPlaceholderColor;
}

std::string MediaRouterAction::GetId() const {
  return ComponentToolbarActionsFactory::kMediaRouterActionId;
}

void MediaRouterAction::SetDelegate(ToolbarActionViewDelegate* delegate) {
  delegate_ = delegate;

  // In cases such as opening a new browser window, SetDelegate() will be
  // called before the WebContents is set. In those cases, we register with the
  // dialog controller when ActiveTabChanged() is called.
  if (delegate_ && delegate_->GetCurrentWebContents())
    RegisterWithDialogController();
}

gfx::Image MediaRouterAction::GetIcon(content::WebContents* web_contents,
                                      const gfx::Size& size) {
  return gfx::Image(
      gfx::CreateVectorIcon(*current_icon_, GetIconColor(*current_icon_)));
}

base::string16 MediaRouterAction::GetActionName() const {
  return l10n_util::GetStringUTF16(IDS_MEDIA_ROUTER_TITLE);
}

base::string16 MediaRouterAction::GetAccessibleName(
    content::WebContents* web_contents) const {
  return GetTooltip(web_contents);
}

base::string16 MediaRouterAction::GetTooltip(
    content::WebContents* web_contents) const {
  return l10n_util::GetStringUTF16(IDS_MEDIA_ROUTER_ICON_TOOLTIP_TEXT);
}

bool MediaRouterAction::IsEnabled(
    content::WebContents* web_contents) const {
  return true;
}

bool MediaRouterAction::WantsToRun(
    content::WebContents* web_contents) const {
  return false;
}

bool MediaRouterAction::HasPopup(
    content::WebContents* web_contents) const {
  return true;
}

void MediaRouterAction::HidePopup() {
  GetMediaRouterDialogController()->HideMediaRouterDialog();
}

gfx::NativeView MediaRouterAction::GetPopupNativeView() {
  return nullptr;
}

ui::MenuModel* MediaRouterAction::GetContextMenu() {
  // If there is an existing context menu, destroy it before we instantiate a
  // new one.
  DestroyContextMenu();
  MediaRouterActionController* controller =
      media_router::MediaRouterUIService::Get(browser_->profile())
          ->action_controller();
  if (toolbar_actions_bar_->IsActionVisibleOnMainBar(this)) {
    contextual_menu_ =
        MediaRouterContextualMenu::CreateForToolbar(browser_, controller);
  } else {
    contextual_menu_ =
        MediaRouterContextualMenu::CreateForOverflowMenu(browser_, controller);
  }
  return contextual_menu_->menu_model();
}

void MediaRouterAction::OnContextMenuClosed() {
  if (toolbar_actions_bar_->popped_out_action() == this &&
      !GetMediaRouterDialogController()->IsShowingMediaRouterDialog()) {
    toolbar_actions_bar_->UndoPopOut();
  }
  // We must destroy the context menu asynchronously to prevent it from being
  // destroyed before the command execution.
  // TODO(takumif): Using task sequence to order operations is fragile. Consider
  // other ways to do so when we move the icon to the trusted area.
  base::PostTaskWithTraits(
      FROM_HERE, {content::BrowserThread::UI},
      base::BindOnce(&MediaRouterAction::DestroyContextMenu,
                     weak_ptr_factory_.GetWeakPtr()));
}

bool MediaRouterAction::ExecuteAction(bool by_user) {
  base::RecordAction(base::UserMetricsAction("MediaRouter_Icon_Click"));

  if (GetMediaRouterDialogController()->IsShowingMediaRouterDialog()) {
    GetMediaRouterDialogController()->HideMediaRouterDialog();
    return false;
  }

  GetMediaRouterDialogController()->ShowMediaRouterDialog();
  if (!skip_close_overflow_menu_for_testing_) {
    // TODO(karandeepb): Instead of checking the return value of
    // CloseOverflowMenuIfOpen, just check
    // ToolbarActionsBar::IsActionVisibleOnMainBar.
    media_router::MediaRouterMetrics::RecordMediaRouterDialogOrigin(
        toolbar_actions_bar_->CloseOverflowMenuIfOpen()
            ? media_router::MediaRouterDialogOpenOrigin::OVERFLOW_MENU
            : media_router::MediaRouterDialogOpenOrigin::TOOLBAR);
  }
  return true;
}

void MediaRouterAction::UpdateState() {
  delegate_->UpdateState();
}

bool MediaRouterAction::DisabledClickOpensMenu() const {
  return false;
}

void MediaRouterAction::OnIssue(const media_router::Issue& issue) {
  current_issue_ = std::make_unique<media_router::IssueInfo>(issue.info());
  MaybeUpdateIcon();
}

void MediaRouterAction::OnIssuesCleared() {
  current_issue_.reset();
  MaybeUpdateIcon();
}

void MediaRouterAction::OnRoutesUpdated(
    const std::vector<media_router::MediaRoute>& routes,
    const std::vector<media_router::MediaRoute::Id>& joinable_route_ids) {
  has_local_display_route_ =
      std::find_if(routes.begin(), routes.end(),
                   [](const media_router::MediaRoute& route) {
                     return route.is_local() && route.for_display();
                   }) != routes.end();
  MaybeUpdateIcon();
}

void MediaRouterAction::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  if (!selection.active_tab_changed() || tab_strip_model->empty())
    return;

  RegisterWithDialogController();
  UpdateDialogState();
}

void MediaRouterAction::OnToolbarActionsBarAnimationEnded() {
  UpdateDialogState();
}

void MediaRouterAction::OnDialogHidden() {
  if (has_dialog_) {
    has_dialog_ = false;
    delegate_->OnPopupClosed();
  }
}

void MediaRouterAction::OnDialogShown() {
  if (!has_dialog_) {
    has_dialog_ = true;
    // We depress the action regardless of whether ExecuteAction() was user
    // initiated.
    delegate_->OnPopupShown(true);
  }
}

void MediaRouterAction::RegisterWithDialogController() {
  MediaRouterDialogControllerImplBase* controller =
      GetMediaRouterDialogController();

  if (!controller)
    return;

  // |controller| keeps track of |this| if |this| was created with the browser
  // window or ephemerally by activating the Cast functionality. If |this| was
  // created in overflow mode, it will be destroyed when the overflow menu is
  // closed.
  if (!toolbar_actions_bar_->in_overflow_mode())
    controller->SetMediaRouterAction(weak_ptr_factory_.GetWeakPtr());
}

void MediaRouterAction::UpdateDialogState() {
  // The WebContents may be null during browser test shutdown, in which case we
  // cannot call GetMediaRouterDialogController().
  if (!delegate_->GetCurrentWebContents())
    return;

  if (GetMediaRouterDialogController()->IsShowingMediaRouterDialog())
    OnDialogShown();
  else
    OnDialogHidden();
}

MediaRouterDialogControllerImplBase*
MediaRouterAction::GetMediaRouterDialogController() {
  DCHECK(delegate_);
  content::WebContents* web_contents = delegate_->GetCurrentWebContents();
  DCHECK(web_contents);
  return MediaRouterDialogControllerImplBase::GetOrCreateForWebContents(
      web_contents);
}

void MediaRouterAction::MaybeUpdateIcon() {
  const gfx::VectorIcon& new_icon = GetCurrentIcon();

  // Update the current state if it has changed.
  if (&new_icon != current_icon_) {
    current_icon_ = &new_icon;

    // Tell the associated view to update its icon to reflect the change made
    // above. If MaybeUpdateIcon() was called as a result of instantiating
    // |this|, then |delegate_| may not be set yet.
    if (delegate_)
      UpdateState();
  }
}

const gfx::VectorIcon& MediaRouterAction::GetCurrentIcon() const {
  // Highest priority is to indicate whether there's an issue.
  if (current_issue_) {
    media_router::IssueInfo::Severity severity = current_issue_->severity;
    if (severity == media_router::IssueInfo::Severity::FATAL)
      return vector_icons::kMediaRouterErrorIcon;
    if (severity == media_router::IssueInfo::Severity::WARNING)
      return vector_icons::kMediaRouterWarningIcon;
    // Fall through for Severity::NOTIFICATION.
  }

  return has_local_display_route_ ? vector_icons::kMediaRouterActiveIcon
                                  : vector_icons::kMediaRouterIdleIcon;
}

void MediaRouterAction::DestroyContextMenu() {
  contextual_menu_.reset();
}
