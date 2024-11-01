// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/media_router/cast_browser_controller.h"

#include "base/containers/contains.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/media_router/media_router_ui_service.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "components/media_router/browser/media_router.h"
#include "components/media_router/browser/media_router_dialog_controller.h"
#include "components/media_router/browser/media_router_factory.h"
#include "components/media_router/browser/media_router_metrics.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/models/image_model.h"

namespace media_router {

namespace {
constexpr char kLoggerComponent[] = "CastBrowserController";
}

using Severity = media_router::IssueInfo::Severity;

CastBrowserController::CastBrowserController(Browser* browser)
    : CastBrowserController(
          browser,
          MediaRouterFactory::GetApiForBrowserContext(browser->profile())) {}

CastBrowserController::CastBrowserController(Browser* browser,
                                             MediaRouter* media_router)
    : IssuesObserver(media_router->GetIssueManager()),
      MediaRoutesObserver(media_router),
      browser_(browser),
      logger_(media_router->GetLogger()) {
  IssuesObserver::Init();
}

CastBrowserController::~CastBrowserController() {
  StopObservingMirroringMediaControllerHosts();
}

void CastBrowserController::OnIssue(const media_router::Issue& issue) {
  issue_severity_ = issue.info().severity;
  UpdateIcon();
}

void CastBrowserController::OnIssuesCleared() {
  issue_severity_.reset();
  UpdateIcon();
}

void CastBrowserController::OnRoutesUpdated(
    const std::vector<media_router::MediaRoute>& routes) {
  has_local_route_ =
      base::Contains(routes, true, &media_router::MediaRoute::is_local);
  StopObservingMirroringMediaControllerHosts();
  for (const auto& route : routes) {
    const auto& route_id = route.media_route_id();
    MirroringMediaControllerHost* mirroring_controller_host =
        MediaRouterFactory::GetApiForBrowserContext(browser_->profile())
            ->GetMirroringMediaControllerHost(route_id);
    if (mirroring_controller_host) {
      mirroring_controller_host->AddObserver(this);
      tracked_mirroring_routes_.emplace_back(route_id);
    }
  }
  UpdateIcon();
}

void CastBrowserController::OnFreezeInfoChanged() {
  UpdateIcon();
}

// TODO(crbug.com/375030079): Move this logic to the profile controller to avoid
// recalculating the icon for every browser.
void CastBrowserController::UpdateIcon() {
  auto* button = BrowserView::GetBrowserViewForBrowser(browser_)
                     ->toolbar()
                     ->GetCastButton();

  if (!button) {
    return;
  }

  if (features::IsToolbarPinningEnabled() &&
      base::FeatureList::IsEnabled(features::kPinnedCastButton)) {
    auto* action_item = static_cast<actions::StatefulImageActionItem*>(
        actions::ActionManager::Get().FindAction(
            kActionRouteMedia,
            browser_->browser_actions()->root_action_item()));
    const gfx::VectorIcon* new_icon = nullptr;

    bool is_frozen = false;
    for (const auto& route_id : tracked_mirroring_routes_) {
      MirroringMediaControllerHost* mirroring_controller_host =
          MediaRouterFactory::GetApiForBrowserContext(browser_->profile())
              ->GetMirroringMediaControllerHost(route_id);
      if (mirroring_controller_host) {
        is_frozen = is_frozen || mirroring_controller_host->IsFrozen();
      }
    }

    if ((!issue_severity_ || issue_severity_ == Severity::NOTIFICATION) &&
        !has_local_route_) {
      new_icon = &vector_icons::kMediaRouterIdleChromeRefreshIcon;
    } else if (issue_severity_ == Severity::WARNING) {
      new_icon = &vector_icons::kMediaRouterWarningChromeRefreshIcon;
    } else if (is_frozen) {
      new_icon = &vector_icons::kMediaRouterPausedIcon;
    } else {
      new_icon = &vector_icons::kMediaRouterActiveChromeRefreshIcon;
    }

    const auto& stateful_image = action_item->GetStatefulImage();
    if (stateful_image.IsVectorIcon() &&
        stateful_image.GetVectorIcon().vector_icon() == new_icon) {
      return;
    }

    LogIconChange(new_icon);

    // TODO(crbug.com/372907639): Use engaged state to account for
    // kColorMediaRouterIconActive replacement.
    action_item->SetStatefulImage(ui::ImageModel::FromVectorIcon(*new_icon));
  }

  button->UpdateIcon();
  UpdateLayoutInsetDelta();
}

CastToolbarButtonController* CastBrowserController::GetActionController()
    const {
  return MediaRouterUIService::Get(browser_->profile())->action_controller();
}

void CastBrowserController::UpdateLayoutInsetDelta() {
  // This icon is smaller than the touchable-UI expected 24dp, so we need to
  // pad the insets to match.
  auto* button = BrowserView::GetBrowserViewForBrowser(browser_)
                     ->toolbar()
                     ->GetCastButton();
  button->SetLayoutInsetDelta(
      gfx::Insets(ui::TouchUiController::Get()->touch_ui() ? 4 : 0));
}

void CastBrowserController::ToggleDialog() {
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

void CastBrowserController::LogIconChange(const gfx::VectorIcon* icon) {
  if (icon == &vector_icons::kMediaRouterIdleChromeRefreshIcon) {
    logger_->LogInfo(
        mojom::LogCategory::kUi, kLoggerComponent,
        "Cast toolbar icon indicates no active session nor issues.", "", "",
        "");
  } else if (icon == &vector_icons::kMediaRouterErrorIcon) {
    logger_->LogInfo(mojom::LogCategory::kUi, kLoggerComponent,
                     "Cast toolbar icon shows a fatal issue.", "", "", "");
  } else if (icon == &vector_icons::kMediaRouterWarningChromeRefreshIcon) {
    logger_->LogInfo(mojom::LogCategory::kUi, kLoggerComponent,
                     "Cast toolbar icon shows a warning issue.", "", "", "");
  } else if (icon == &vector_icons::kMediaRouterPausedIcon) {
    logger_->LogInfo(
        mojom::LogCategory::kUi, kLoggerComponent,
        "Cast toolbar icon indicated there is a paused mirroring session.", "",
        "", "");
  } else {
    CHECK_EQ(icon, &vector_icons::kMediaRouterActiveChromeRefreshIcon);
    logger_->LogInfo(mojom::LogCategory::kUi, kLoggerComponent,
                     "Cast toolbar icon is blue, indicating an active session.",
                     "", "", "");
  }
}

void CastBrowserController::StopObservingMirroringMediaControllerHosts() {
  for (const auto& route_id : tracked_mirroring_routes_) {
    media_router::MirroringMediaControllerHost* mirroring_controller_host =
        MediaRouterFactory::GetApiForBrowserContext(browser_->profile())
            ->GetMirroringMediaControllerHost(route_id);
    if (mirroring_controller_host) {
      mirroring_controller_host->RemoveObserver(this);
    }
  }
  tracked_mirroring_routes_.clear();
}

}  // namespace media_router
