// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/media_router/cast_toolbar_button.h"

#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/media_router/media_router_ui_service.h"
#include "chrome/grit/generated_resources.h"
#include "components/media_router/browser/media_router.h"
#include "components/media_router/browser/media_router_dialog_controller.h"
#include "components/media_router/browser/media_router_factory.h"
#include "components/media_router/browser/media_router_metrics.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/menu_model.h"
#include "ui/base/pointer/touch_ui_controller.h"
#include "ui/base/theme_provider.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/controls/button/button_controller.h"
#include "ui/views/metadata/metadata_impl_macros.h"

namespace media_router {

namespace {
constexpr char kLoggerComponent[] = "CastToolbarButton";
}

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
    : ToolbarButton(base::BindRepeating(&CastToolbarButton::ButtonPressed,
                                        base::Unretained(this)),
                    context_menu->CreateMenuModel(),
                    /** tab_strip_model*/ nullptr,
                    /** trigger_menu_on_long_press */ false),
      IssuesObserver(media_router->GetIssueManager()),
      MediaRoutesObserver(media_router),
      browser_(browser),
      profile_(browser_->profile()),
      context_menu_(std::move(context_menu)),
      logger_(media_router->GetLogger()) {
  button_controller()->set_notify_action(
      views::ButtonController::NotifyAction::kOnPress);

  SetFlipCanvasOnPaintForRTLUI(false);
  SetTooltipText(l10n_util::GetStringUTF16(IDS_MEDIA_ROUTER_ICON_TOOLTIP_TEXT));

  IssuesObserver::Init();

  DCHECK(GetActionController());
  GetActionController()->AddObserver(this);
  SetVisible(GetActionController()->ShouldEnableAction());
}

CastToolbarButton::~CastToolbarButton() {
  if (GetActionController())
    GetActionController()->RemoveObserver(this);
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

void CastToolbarButton::UpdateIcon() {
  using Severity = media_router::IssueInfo::Severity;
  const auto severity =
      current_issue_ ? current_issue_->severity : Severity::NOTIFICATION;
  const gfx::VectorIcon* new_icon = nullptr;
  SkColor icon_color;

  if (severity == Severity::NOTIFICATION && !has_local_display_route_) {
    new_icon = &vector_icons::kMediaRouterIdleIcon;
    icon_color = gfx::kPlaceholderColor;
  } else if (severity == Severity::FATAL) {
    new_icon = &vector_icons::kMediaRouterErrorIcon;
    icon_color = GetNativeTheme()->GetSystemColor(
        ui::NativeTheme::kColorId_AlertSeverityHigh);
  } else if (severity == Severity::WARNING) {
    new_icon = &vector_icons::kMediaRouterWarningIcon;
    icon_color = GetNativeTheme()->GetSystemColor(
        ui::NativeTheme::kColorId_AlertSeverityMedium);
  } else {
    new_icon = &vector_icons::kMediaRouterActiveIcon;
    icon_color = gfx::kGoogleBlue500;
  }

  // This function is called when system theme changes. If an idle icon is
  // present, its color needs update.
  if (icon_color == gfx::kPlaceholderColor) {
    UpdateIconsWithStandardColors(*new_icon);
  }
  if (icon_ == new_icon)
    return;

  icon_ = new_icon;
  LogIconChange(icon_);
  if (icon_color != gfx::kPlaceholderColor) {
    for (auto state : kButtonStates)
      SetImageModel(state, ui::ImageModel::FromVectorIcon(*icon_, icon_color));
  }
  UpdateLayoutInsetDelta();
}

MediaRouterActionController* CastToolbarButton::GetActionController() const {
  return MediaRouterUIService::Get(profile_)->action_controller();
}

void CastToolbarButton::UpdateLayoutInsetDelta() {
  // This icon is smaller than the touchable-UI expected 24dp, so we need to pad
  // the insets to match.
  SetLayoutInsetDelta(
      gfx::Insets(ui::TouchUiController::Get()->touch_ui() ? 4 : 0));
}

void CastToolbarButton::ButtonPressed() {
  MediaRouterDialogController* dialog_controller =
      MediaRouterDialogController::GetOrCreateForWebContents(
          browser_->tab_strip_model()->GetActiveWebContents());
  if (dialog_controller->IsShowingMediaRouterDialog()) {
    dialog_controller->HideMediaRouterDialog();
  } else {
    dialog_controller->ShowMediaRouterDialog(
        MediaRouterDialogOpenOrigin::TOOLBAR);
    MediaRouterMetrics::RecordMediaRouterDialogOrigin(
        MediaRouterDialogOpenOrigin::TOOLBAR);
  }
}

void CastToolbarButton::LogIconChange(const gfx::VectorIcon* icon) {
  if (icon_ == &vector_icons::kMediaRouterIdleIcon) {
    logger_->LogInfo(
        mojom::LogCategory::kUi, kLoggerComponent,
        "Cast toolbar icon indicates no active session nor issues.", "", "",
        "");
  } else if (icon_ == &vector_icons::kMediaRouterErrorIcon) {
    logger_->LogInfo(mojom::LogCategory::kUi, kLoggerComponent,
                     "Cast toolbar icon shows a fatal issue.", "", "", "");
  } else if (icon_ == &vector_icons::kMediaRouterWarningIcon) {
    logger_->LogInfo(mojom::LogCategory::kUi, kLoggerComponent,
                     "Cast toolbar icon shows a warning issue.", "", "", "");
  } else if (icon_ == &vector_icons::kMediaRouterActiveIcon) {
    logger_->LogInfo(mojom::LogCategory::kUi, kLoggerComponent,
                     "Cast toolbar icon is blue, indicating an active session.",
                     "", "", "");
  } else {
    NOTREACHED();
  }
}

BEGIN_METADATA(CastToolbarButton, ToolbarButton)
END_METADATA

}  // namespace media_router
