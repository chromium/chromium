// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/privacy_sandbox/base_dialog_handler.h"

#include "chrome/browser/ui/webui/privacy_sandbox/base_dialog_ui.h"

namespace privacy_sandbox {

using dialog::mojom::BaseDialogPage;
using dialog::mojom::BaseDialogPageHandler;
using notice::mojom::PrivacySandboxNotice;
using enum notice::mojom::PrivacySandboxNotice;
using notice::mojom::PrivacySandboxNoticeEvent;

BaseDialogHandler::BaseDialogHandler(
    mojo::PendingReceiver<BaseDialogPageHandler> receiver,
    mojo::PendingRemote<BaseDialogPage> page,
    DesktopViewManagerInterface* view_manager,
    BaseDialogUIDelegate* delegate)
    : receiver_(this, std::move(receiver)),
      page_(std::move(page)),
      delegate_(delegate),
      view_manager_(view_manager) {
  CHECK(view_manager_);
  desktop_view_manager_observation_.Observe(view_manager_);
}

BaseDialogHandler::~BaseDialogHandler() = default;

void BaseDialogHandler::ResizeDialog(uint32_t height) {
  if (!delegate_) {
    return;
  }
  CHECK(!has_resized);
  delegate_->ResizeNativeView(height);
  has_resized = true;
}

void BaseDialogHandler::ShowDialog() {
  if (!delegate_) {
    return;
  }
  // A callback is passed to the delegate to ensure it is invoked only once the
  // PrivacySandboxDialogView confirms it's fully visible. This is crucial to
  // accurately track the dialog's 'shown' state.
  delegate_->ShowNativeView(
      base::BindOnce(&BaseDialogHandler::NativeDialogShownCallback,
                     weak_ptr_factory_.GetWeakPtr()));
}

// Handles specific settings-related events by delegating to the appropriate
// Privacy Sandbox settings page opening method.
void BaseDialogHandler::HandleSettingsEvent(PrivacySandboxNotice notice) {
  if (!delegate_) {
    return;
  }
  switch (notice) {
    case kProtectedAudienceMeasurementNotice:
    case kThreeAdsApisNotice:
      delegate_->OpenPrivacySandboxSettings();
      break;
    case kMeasurementNotice:
      delegate_->OpenPrivacySandboxAdMeasurementSettings();
      break;
    default:
      break;
  }
}

// Dispatches an event to the View Manager.
// This is a helper for EventOccurred and NativeDialogShownCallback to
// centralize event handling logic.
void BaseDialogHandler::DispatchEvent(PrivacySandboxNotice notice,
                                      PrivacySandboxNoticeEvent event) {
  if (event == PrivacySandboxNoticeEvent::kSettings) {
    HandleSettingsEvent(notice);
  }
  view_manager_->OnEventOccurred(notice, event);
}

// Events are either sent to the view manager immediately if the dialog is
// already confirmed as visible, or if no delegate exists (implying the event
// should not wait for visibility confirmation). Otherwise, events are queued to
// ensure they are processed only after the dialog's 'shown' state is confirmed.
void BaseDialogHandler::EventOccurred(PrivacySandboxNotice notice,
                                      PrivacySandboxNoticeEvent event) {
  if (native_dialog_shown_ || !delegate_) {
    DispatchEvent(notice, event);
  } else {
    events_queue_.push({notice, event});
  }
}

// Callback invoked by PrivacySandboxDialogView once the dialog is confirmed
// as visible. This function updates the handler's internal 'shown' state
// and dispatches any events that were queued prior to the dialog being visible
// in the correct order.
void BaseDialogHandler::NativeDialogShownCallback() {
  native_dialog_shown_ = true;
  for (; !events_queue_.empty(); events_queue_.pop()) {
    auto [notice, event] = events_queue_.front();
    DispatchEvent(notice, event);
  }
}

void BaseDialogHandler::MaybeNavigateToNextStep(
    std::optional<PrivacySandboxNotice> next_id) {
  if (!delegate_) {
    return;
  }
  if (!next_id) {
    delegate_->CloseNativeView();
  } else {
    page_->NavigateToNextStep(*next_id);
    delegate_->SetPrivacySandboxNotice(*next_id);
  }
}

bool BaseDialogHandler::IsNativeDialogShownForTesting() {
  return native_dialog_shown_;
}

BrowserWindowInterface* BaseDialogHandler::GetBrowser() {
  return delegate_->GetBrowser();
}

}  // namespace privacy_sandbox
