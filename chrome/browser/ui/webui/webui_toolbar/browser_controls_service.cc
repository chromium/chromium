// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/webui_toolbar/browser_controls_service.h"

#include <optional>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "base/types/expected_macros.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/waap/waap_ui_metrics_service.h"
#include "components/browser_apis/browser_controls/browser_controls_api.mojom.h"
#include "content/public/browser/render_frame_host.h"
#include "mojo/public/mojom/base/error.mojom.h"
#include "ui/base/window_open_disposition_utils.h"
#include "ui/events/event_constants.h"

namespace {

using Code = mojo_base::mojom::Code;
using Error = mojo_base::mojom::Error;

}  // namespace

namespace browser_controls_api {

// static
base::expected<ui::EventFlags, mojo_base::mojom::ErrorPtr>
BrowserControlsService::ToUiEventFlags(
    const std::vector<browser_controls_api::mojom::EventDispositionFlag>&
        flags) {
  using browser_controls_api::mojom::EventDispositionFlag;
  ui::EventFlags event_flags = 0;
  for (auto& flag : flags) {
    switch (flag) {
      case EventDispositionFlag::kMiddleMouseButton: {
        event_flags |= ui::EF_MIDDLE_MOUSE_BUTTON;
        break;
      }
      case EventDispositionFlag::kAltKeyDown: {
        event_flags |= ui::EF_ALT_DOWN;
        break;
      }
      case EventDispositionFlag::kMetaKeyDown: {
        event_flags |= ui::EF_COMMAND_DOWN;
        break;
      }
      case EventDispositionFlag::kShiftKeyDown: {
        event_flags |= ui::EF_SHIFT_DOWN;
        break;
      }
      case EventDispositionFlag::kControlKeyDown: {
        event_flags |= ui::EF_CONTROL_DOWN;
        break;
      }
      case EventDispositionFlag::kAltGrKeyDown: {
        event_flags |= ui::EF_ALTGR_DOWN;
        break;
      }
      case EventDispositionFlag::kUnspecified:
        return base::unexpected(Error::New(
            Code::kInvalidArgument, "invalid event disposition flag received"));
    }
  }
  return event_flags;
}

BrowserControlsService::BrowserControlsService(
    mojo::PendingReceiver<mojom::BrowserControlsService> service,
    std::unique_ptr<BrowserControlsAdapter> browser_adapter,
    BrowserControlsServiceDelegate* delegate,
    content::RenderFrameHost* toolbar_rfh)
    : service_(&bridge_, std::move(service)),
      browser_adapter_(std::move(browser_adapter)),
      delegate_(delegate),
      toolbar_rfh_(toolbar_rfh) {
  CHECK(browser_adapter_);
}

BrowserControlsService::~BrowserControlsService() = default;

BrowserControlsService::ReloadFromClickResult
BrowserControlsService::ReloadFromClick(
    bool bypass_cache,
    const std::vector<browser_controls_api::mojom::EventDispositionFlag>&
        click_flags,
    mojom::ReloadInteractionMetadataPtr metadata) {
  const base::TimeTicks now = base::TimeTicks::Now();

  base::TimeTicks reconstructed_ticks;
  bool validation_succeeded = false;

  if (metadata) {
    auto result = ReconstructAndValidateInteractionTime(*metadata, now);
    if (!result.has_value()) {
      VLOG(1) << "Interaction time validation failed: "
              << result.error()->message;
    } else {
      reconstructed_ticks = *result;
      validation_succeeded = true;
    }
  }

  // This is called in order to signal that external protocol dialogs are
  // allowed to show due to a user action, which are likely to happen on the
  // next page load after the reload button is clicked.
  // Ideally, the browser UI's event system would notify ExternalProtocolHandler
  // that a user action occurred and we are OK to open the dialog, but for some
  // reason that isn't happening every time the reload button is clicked. See
  // http://crbug.com/40180927
  if (delegate_) {
    delegate_->PermitLaunchUrl();
  }

  ASSIGN_OR_RETURN(auto converted, ToUiEventFlags(click_flags));

  browser_adapter_->Reload(bypass_cache,
                           ui::DispositionFromEventFlags(converted));

  if (metadata && validation_succeeded) {
    MaybeRecordInteractionToReloadMetric(reconstructed_ticks,
                                         metadata->input_type);
  }
  return std::monostate();
}

BrowserControlsService::StopLoadResult BrowserControlsService::StopLoad() {
  browser_adapter_->Stop();
  return std::monostate();
}

BrowserControlsService::BackResult BrowserControlsService::Back(
    const std::vector<browser_controls_api::mojom::EventDispositionFlag>&
        flags) {
  ASSIGN_OR_RETURN(auto converted, ToUiEventFlags(flags));
  browser_adapter_->Back(ui::DispositionFromEventFlags(converted));
  return std::monostate();
}

BrowserControlsService::ForwardResult BrowserControlsService::Forward(
    const std::vector<browser_controls_api::mojom::EventDispositionFlag>&
        flags) {
  ASSIGN_OR_RETURN(auto converted, ToUiEventFlags(flags));
  browser_adapter_->Forward(ui::DispositionFromEventFlags(converted));
  return std::monostate();
}

BrowserControlsService::BackButtonHoveredResult
BrowserControlsService::BackButtonHovered() {
  browser_adapter_->BackButtonHovered();

  return std::monostate();
}

BrowserControlsService::SplitActiveTabResult
BrowserControlsService::SplitActiveTab() {
  // TODO: need to check IsActiveTabInSplit() first.
  browser_adapter_->CreateNewSplitTab();
  return std::monostate();
}

BrowserControlsService::NavigateHomeResult BrowserControlsService::NavigateHome(
    const std::vector<browser_controls_api::mojom::EventDispositionFlag>&
        click_flags) {
  ASSIGN_OR_RETURN(auto converted, ToUiEventFlags(click_flags));
  browser_adapter_->NavigateHome(ui::DispositionFromEventFlags(converted));
  return std::monostate();
}

BrowserControlsService::NavigateResult BrowserControlsService::Navigate(
    const GURL& url) {
  browser_adapter_->Navigate(url);
  return std::monostate();
}

BrowserControlsService::NavigateTextResult BrowserControlsService::NavigateText(
    const std::string& text) {
  browser_adapter_->NavigateText(text);
  return std::monostate();
}

void BrowserControlsService::SetDelegate(
    BrowserControlsServiceDelegate* delegate) {
  delegate_ = delegate;
}


base::expected<base::TimeTicks, mojo_base::mojom::ErrorPtr>
BrowserControlsService::ReconstructAndValidateInteractionTime(
    const mojom::ReloadInteractionMetadata& metadata,
    base::TimeTicks evaluation_time) {
  // 1. Monotonicity check: Time delta offset must be positive.
  if (metadata.interaction_time_offset.is_negative()) {
    return base::unexpected(
        Error::New(Code::kInvalidArgument, "negative offset"));
  }

  // 2. User intent check: Enforce that the user actually interacted with the
  // page to prevent a compromised renderer from sending programmatic clicks.
  if (!toolbar_rfh_->HasTransientUserActivation()) {
    return base::unexpected(
        Error::New(Code::kFailedPrecondition, "no transient activation"));
  }

  // 3. Baseline check: Query the delegate on-demand. This retrieves the
  // shared document-scoped navigation start ticks from the active WebUI
  // controller.
  base::TimeTicks navigation_start_ticks = delegate_->GetNavigationStartTicks();
  if (navigation_start_ticks.is_null()) {
    return base::unexpected(
        Error::New(Code::kFailedPrecondition, "no navigation start ticks"));
  }

  base::TimeTicks reconstructed_ticks =
      navigation_start_ticks + metadata.interaction_time_offset;

  // 4. Future check: The interaction cannot happen in the future (allowing
  // 1ms for clock drift / scheduling jitter).
  if (reconstructed_ticks > evaluation_time + base::Milliseconds(1)) {
    return base::unexpected(
        Error::New(Code::kFailedPrecondition, "future interaction"));
  }

  // 5. Staleness check: Reject old telemetry to prevent stale data from
  // polluting metrics or replay of old events. A 10-second threshold is chosen
  // to avoid discarding valid metrics during heavy jank, aligning with the
  // downstream UMA histogram's maximum range of 10s.
  if (evaluation_time - reconstructed_ticks > base::Seconds(10)) {
    return base::unexpected(
        Error::New(Code::kFailedPrecondition, "stale interaction"));
  }

  // 6. Monotonicity / Replay check: Enforce that interaction ticks strictly
  // increase to prevent replay of duplicate telemetry events.
  if (reconstructed_ticks <= last_processed_interaction_ticks_) {
    return base::unexpected(
        Error::New(Code::kFailedPrecondition, "duplicate interaction"));
  }

  last_processed_interaction_ticks_ = reconstructed_ticks;
  return reconstructed_ticks;
}

void BrowserControlsService::MaybeRecordInteractionToReloadMetric(
    base::TimeTicks interaction_ticks,
    mojom::ReloadInputType input_type) const {
  auto* profile =
      Profile::FromBrowserContext(toolbar_rfh_->GetBrowserContext());
  auto* metrics_service = WaapUIMetricsService::Get(profile);
  if (!metrics_service) {
    return;
  }
  std::optional<WaapUIMetricsRecorder::ReloadButtonInputType> target_input_type;
  switch (input_type) {
    case mojom::ReloadInputType::kUnspecified:
      break;
    case mojom::ReloadInputType::kMouseRelease:
      target_input_type =
          WaapUIMetricsRecorder::ReloadButtonInputType::kMouseRelease;
      break;
    case mojom::ReloadInputType::kKeyPress:
      target_input_type =
          WaapUIMetricsRecorder::ReloadButtonInputType::kKeyPress;
      break;
  }

  if (target_input_type) {
    metrics_service->RecordReloadButtonInteractionToReload(
        interaction_ticks, base::TimeTicks::Now(), *target_input_type);
  }
}

}  // namespace browser_controls_api
