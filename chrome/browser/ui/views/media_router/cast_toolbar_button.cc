// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/media_router/cast_toolbar_button.h"

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "chrome/browser/media/router/discovery/access_code/access_code_cast_feature.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/media_router/media_router_ui_service.h"
#include "chrome/grit/generated_resources.h"
#include "components/media_router/browser/media_router.h"
#include "components/media_router/browser/media_router_dialog_controller.h"
#include "components/media_router/browser/media_router_factory.h"
#include "components/media_router/browser/media_router_metrics.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/base/models/menu_model.h"
#include "ui/base/pointer/touch_ui_controller.h"
#include "ui/base/theme_provider.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/controls/button/button_controller.h"

namespace media_router {

namespace {
constexpr char kLoggerComponent[] = "CastToolbarButton";
}

using Severity = media_router::IssueInfo::Severity;

// static
std::unique_ptr<CastToolbarButton> CastToolbarButton::Create(Browser* browser) {
  // These objects may be null in tests.
  if (!MediaRouterUIService::Get(browser->profile()) ||
      !MediaRouterFactory::GetApiForBrowserContext(browser->profile())) {
    return nullptr;
  }

  std::unique_ptr<CastContextualMenu> context_menu = CastContextualMenu::Create(
      browser,
      MediaRouterUIService::Get(browser->profile())->action_controller());
  return std::make_unique<CastToolbarButton>(
      browser, MediaRouterFactory::GetApiForBrowserContext(browser->profile()),
      std::move(context_menu));
}

CastToolbarButton::CastToolbarButton(
    Browser* browser,
    MediaRouter* media_router,
    std::unique_ptr<CastContextualMenu> context_menu)
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
  StopObservingMirroringMediaControllerHosts();
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
  views::InkDrop::Get(this)->AnimateToState(views::InkDropState::ACTIVATED,
                                            nullptr);
}

void CastToolbarButton::DeactivateIcon() {
  views::InkDrop::Get(this)->AnimateToState(views::InkDropState::DEACTIVATED,
                                            nullptr);
}

void CastToolbarButton::OnIssue(const media_router::Issue& issue) {
  issue_severity_ = issue.info().severity;
  UpdateIcon();
}

void CastToolbarButton::OnIssuesCleared() {
  issue_severity_.reset();
  UpdateIcon();
}

void CastToolbarButton::OnRoutesUpdated(
    const std::vector<media_router::MediaRoute>& routes) {
  has_local_route_ =
      base::Contains(routes, true, &media_router::MediaRoute::is_local);
  StopObservingMirroringMediaControllerHosts();
  for (const auto& route : routes) {
    const auto& route_id = route.media_route_id();
    MirroringMediaControllerHost* mirroring_controller_host =
        MediaRouterFactory::GetApiForBrowserContext(profile_)
            ->GetMirroringMediaControllerHost(route_id);
    if (mirroring_controller_host) {
      mirroring_controller_host->AddObserver(this);
      tracked_mirroring_routes_.emplace_back(route_id);
    }
  }
  UpdateIcon();
}

void CastToolbarButton::OnFreezeInfoChanged() {
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
    case ui::EventType::kGestureTapDown:
      GetActionController()->KeepIconShownOnPressed();
      break;
    case ui::EventType::kGestureEnd:
    case ui::EventType::kGestureTapCancel:
      GetActionController()->MaybeHideIconOnReleased();
      break;
    default:
      break;
  }
  ToolbarButton::OnGestureEvent(event);
}

void CastToolbarButton::OnThemeChanged() {
  ToolbarButton::OnThemeChanged();
  UpdateIcon();
}

void CastToolbarButton::UpdateIcon() {
  if (!GetWidget())
    return;
  bool is_frozen = false;
  for (const auto& route_id : tracked_mirroring_routes_) {
    MirroringMediaControllerHost* mirroring_controller_host =
        MediaRouterFactory::GetApiForBrowserContext(profile_)
            ->GetMirroringMediaControllerHost(route_id);
    if (mirroring_controller_host) {
      is_frozen = is_frozen || mirroring_controller_host->IsFrozen();
    }
  }
  const gfx::VectorIcon* new_icon = nullptr;
  SkColor icon_color;

  const auto* const color_provider = GetColorProvider();
  if ((!issue_severity_ || issue_severity_ == Severity::NOTIFICATION) &&
      !has_local_route_) {
    new_icon = &vector_icons::kMediaRouterIdleChromeRefreshIcon;
    icon_color = gfx::kPlaceholderColor;
  } else if (issue_severity_ == Severity::WARNING) {
    new_icon = &vector_icons::kMediaRouterWarningChromeRefreshIcon;
    icon_color = gfx::kPlaceholderColor;
  } else if (is_frozen) {
    new_icon = &vector_icons::kMediaRouterPausedIcon;
    icon_color = gfx::kPlaceholderColor;
  } else {
    new_icon = &vector_icons::kMediaRouterActiveChromeRefreshIcon;
    icon_color = color_provider->GetColor(kColorMediaRouterIconActive);
  }

  // This function is called when system theme changes. If an idle icon is
  // present, its color needs update.
  if (icon_color == gfx::kPlaceholderColor) {
    for (auto state : kButtonStates) {
      SetImageModel(state, ui::ImageModel::FromVectorIcon(
                               *new_icon, GetForegroundColor(state)));
    }
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

CastToolbarButtonController* CastToolbarButton::GetActionController() const {
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
        MediaRouterDialogActivationLocation::TOOLBAR);
  }
}

void CastToolbarButton::LogIconChange(const gfx::VectorIcon* icon) {
  if (icon_ == &vector_icons::kMediaRouterIdleChromeRefreshIcon) {
    logger_->LogInfo(
        mojom::LogCategory::kUi, kLoggerComponent,
        "Cast toolbar icon indicates no active session nor issues.", "", "",
        "");
  } else if (icon_ == &vector_icons::kMediaRouterErrorIcon) {
    logger_->LogInfo(mojom::LogCategory::kUi, kLoggerComponent,
                     "Cast toolbar icon shows a fatal issue.", "", "", "");
  } else if (icon_ == &vector_icons::kMediaRouterWarningChromeRefreshIcon) {
    logger_->LogInfo(mojom::LogCategory::kUi, kLoggerComponent,
                     "Cast toolbar icon shows a warning issue.", "", "", "");
  } else if (icon_ == &vector_icons::kMediaRouterPausedIcon) {
    logger_->LogInfo(
        mojom::LogCategory::kUi, kLoggerComponent,
        "Cast toolbar icon indicated there is a paused mirroring session.", "",
        "", "");
  } else {
    CHECK_EQ(icon_, &vector_icons::kMediaRouterActiveChromeRefreshIcon);
    logger_->LogInfo(mojom::LogCategory::kUi, kLoggerComponent,
                     "Cast toolbar icon is blue, indicating an active session.",
                     "", "", "");
  }
}

void CastToolbarButton::StopObservingMirroringMediaControllerHosts() {
  for (const auto& route_id : tracked_mirroring_routes_) {
    media_router::MirroringMediaControllerHost* mirroring_controller_host =
        MediaRouterFactory::GetApiForBrowserContext(profile_)
            ->GetMirroringMediaControllerHost(route_id);
    if (mirroring_controller_host) {
      mirroring_controller_host->RemoveObserver(this);
    }
  }
  tracked_mirroring_routes_.clear();
}

BEGIN_METADATA(CastToolbarButton)
END_METADATA

}  // namespace media_router
