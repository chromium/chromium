// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/media_router/cast_toolbar_button.h"

#include "chrome/browser/media/router/media_router.h"
#include "chrome/browser/media/router/media_router_dialog_controller.h"
#include "chrome/browser/media/router/media_router_factory.h"
#include "chrome/browser/media/router/media_router_metrics.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/media_router/media_router_ui_service.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/base/models/menu_model.h"
#include "ui/base/theme_provider.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/controls/button/button_controller.h"

namespace media_router {

// static
std::unique_ptr<CastToolbarButton> CastToolbarButton::Create(Browser* browser) {
  // These objects may be null in tests.
  if (!MediaRouterUIService::Get(browser->profile()) ||
      !MediaRouterFactory::GetApiForBrowserContext(browser->profile())) {
    return nullptr;
  }

  std::unique_ptr<MediaRouterContextualMenu> context_menu =
      MediaRouterContextualMenu::Create(
          browser,
          MediaRouterUIService::Get(browser->profile())->action_controller());
  return std::make_unique<CastToolbarButton>(
      browser, MediaRouterFactory::GetApiForBrowserContext(browser->profile()),
      std::move(context_menu));
}

CastToolbarButton::CastToolbarButton(
    Browser* browser,
    MediaRouter* media_router,
    std::unique_ptr<MediaRouterContextualMenu> context_menu)
    : ToolbarButton(this,
                    context_menu->TakeMenuModel(),
                    /** tab_strip_model*/ nullptr,
                    /** trigger_menu_on_long_press */ false),
      IssuesObserver(media_router->GetIssueManager()),
      MediaRoutesObserver(media_router),
      browser_(browser),
      profile_(browser_->profile()),
      context_menu_(std::move(context_menu)) {
  button_controller()->set_notify_action(
      views::ButtonController::NotifyAction::kOnPress);

  EnableCanvasFlippingForRTLUI(false);
  SetTooltipText(l10n_util::GetStringUTF16(IDS_MEDIA_ROUTER_ICON_TOOLTIP_TEXT));

  ToolbarButton::Init();
  IssuesObserver::Init();

  DCHECK(GetActionController());
  GetActionController()->AddObserver(this);
  SetVisible(GetActionController()->ShouldEnableAction());
}

CastToolbarButton::~CastToolbarButton() {
  if (GetActionController())
    GetActionController()->RemoveObserver(this);
}

const gfx::VectorIcon& CastToolbarButton::GetCurrentIcon() const {
  // Highest priority is to indicate whether there's an issue.
  if (current_issue_) {
    media_router::IssueInfo::Severity severity = current_issue_->severity;
    switch (severity) {
      case media_router::IssueInfo::Severity::FATAL:
        return ::vector_icons::kMediaRouterErrorIcon;
      case media_router::IssueInfo::Severity::WARNING:
        return ::vector_icons::kMediaRouterWarningIcon;
      case media_router::IssueInfo::Severity::NOTIFICATION:
        // There is no icon specific to notification issues.
        break;
    }
  }
  return has_local_display_route_ ? ::vector_icons::kMediaRouterActiveIcon
                                  : ::vector_icons::kMediaRouterIdleIcon;
}

void CastToolbarButton::ShowIcon() {
  SetVisible(true);
  PreferredSizeChanged();
}

void CastToolbarButton::HideIcon() {
  SetVisible(false);
  PreferredSizeChanged();
}

void CastToolbarButton::ActivateIcon() {
  AnimateInkDrop(views::InkDropState::ACTIVATED, nullptr);
}

void CastToolbarButton::DeactivateIcon() {
  AnimateInkDrop(views::InkDropState::DEACTIVATED, nullptr);
}

void CastToolbarButton::OnIssue(const media_router::Issue& issue) {
  current_issue_ = std::make_unique<media_router::IssueInfo>(issue.info());
  UpdateIcon();
}

void CastToolbarButton::OnIssuesCleared() {
  if (current_issue_)
    current_issue_.reset();
  UpdateIcon();
}

void CastToolbarButton::OnRoutesUpdated(
    const std::vector<media_router::MediaRoute>& routes,
    const std::vector<media_router::MediaRoute::Id>& joinable_route_ids) {
  has_local_display_route_ =
      std::find_if(routes.begin(), routes.end(),
                   [](const media_router::MediaRoute& route) {
                     return route.is_local() && route.for_display();
                   }) != routes.end();
  UpdateIcon();
}

bool CastToolbarButton::OnMousePressed(const ui::MouseEvent& event) {
  if (event.IsRightMouseButton() && GetActionController())
    GetActionController()->KeepIconShownOnPressed();
  return ToolbarButton::OnMousePressed(event);
}

void CastToolbarButton::OnMouseReleased(const ui::MouseEvent& event) {
  ToolbarButton::OnMouseReleased(event);
  if (event.IsRightMouseButton() && GetActionController())
    GetActionController()->MaybeHideIconOnReleased();
}

void CastToolbarButton::OnGestureEvent(ui::GestureEvent* event) {
  switch (event->type()) {
    case ui::ET_GESTURE_TAP_DOWN:
      GetActionController()->KeepIconShownOnPressed();
      break;
    case ui::ET_GESTURE_END:
    case ui::ET_GESTURE_TAP_CANCEL:
      GetActionController()->MaybeHideIconOnReleased();
      break;
    default:
      break;
  }
  ToolbarButton::OnGestureEvent(event);
}

void CastToolbarButton::ButtonPressed(views::Button* sender,
                                      const ui::Event& event) {
  MediaRouterDialogController* dialog_controller =
      MediaRouterDialogController::GetOrCreateForWebContents(
          browser_->tab_strip_model()->GetActiveWebContents());
  if (dialog_controller->IsShowingMediaRouterDialog()) {
    dialog_controller->HideMediaRouterDialog();
  } else {
    dialog_controller->ShowMediaRouterDialog();
    MediaRouterMetrics::RecordMediaRouterDialogOrigin(
        MediaRouterDialogOpenOrigin::TOOLBAR);
  }
}

void CastToolbarButton::AddedToWidget() {
  ToolbarButton::AddedToWidget();
  UpdateIcon();
}

void CastToolbarButton::UpdateIcon() {
  // If widget isn't set, the button doesn't have access to the theme provider
  // to set colors. Defer updating until AddedToWidget()
  if (!GetWidget())
    return;
  const gfx::VectorIcon& icon = GetCurrentIcon();
  SetImage(views::Button::STATE_NORMAL,
           gfx::CreateVectorIcon(icon, GetIconColor(&icon)));
  // This icon is smaller than the touchable-UI expected 24dp, so we need to pad
  // the insets to match.
  SetLayoutInsetDelta(
      gfx::Insets(ui::MaterialDesignController::touch_ui() ? 4 : 0));
}

MediaRouterActionController* CastToolbarButton::GetActionController() const {
  return MediaRouterUIService::Get(profile_)->action_controller();
}

SkColor CastToolbarButton::GetIconColor(const gfx::VectorIcon* icon_id) const {
  if (icon_id == &::vector_icons::kMediaRouterIdleIcon) {
    return GetThemeProvider()->GetColor(
        ThemeProperties::COLOR_TOOLBAR_BUTTON_ICON);
  } else if (icon_id == &::vector_icons::kMediaRouterActiveIcon) {
    return gfx::kGoogleBlue500;
  } else if (icon_id == &::vector_icons::kMediaRouterWarningIcon) {
    return GetNativeTheme()->GetSystemColor(
        ui::NativeTheme::kColorId_AlertSeverityMedium);
  } else if (icon_id == &::vector_icons::kMediaRouterErrorIcon) {
    return GetNativeTheme()->GetSystemColor(
        ui::NativeTheme::kColorId_AlertSeverityHigh);
  }
  NOTREACHED();
  return gfx::kPlaceholderColor;
}

}  // namespace media_router
