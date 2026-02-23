// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/record_replay_page_action_controller.h"

#include "chrome/browser/record_replay/record_replay_client.h"
#include "chrome/browser/record_replay/record_replay_manager.h"
#include "chrome/browser/record_replay/recording_data_manager.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/record_replay/save_recording_bubble_controller_impl.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/page_action/page_action_container_view.h"
#include "chrome/browser/ui/views/page_action/page_action_controller.h"
#include "chrome/browser/ui/views/page_action/page_action_view.h"
#include "chrome/browser/ui/views/record_replay/save_recording_bubble_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/tabs/public/tab_interface.h"
#include "components/vector_icons/vector_icons.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/actions/actions.h"
#include "ui/base/models/image_model.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

using State = record_replay::RecordReplayManager::State;

DEFINE_USER_DATA(RecordReplayPageActionController);

RecordReplayPageActionController::RecordReplayPageActionController(
    tabs::TabInterface& tab,
    page_actions::PageActionController& page_action_controller)
    : tab_(tab), page_action_controller_(page_action_controller) {
  timer_.Start(
      FROM_HERE, base::Seconds(1),
      base::BindRepeating(&RecordReplayPageActionController::UpdateState,
                          base::Unretained(this)));
  UpdateState();
}

RecordReplayPageActionController::~RecordReplayPageActionController() = default;

void RecordReplayPageActionController::ExecuteAction(
    actions::ActionItem* item) {
  record_replay::RecordReplayClient* client =
      tab_->GetTabFeatures()->record_replay_client();
  if (!client) {
    return;
  }
  record_replay::RecordReplayManager& manager = client->GetManager();
  switch (manager.state()) {
    case State::kIdle:
      if (has_recording_) {
        manager.StartReplay();
      } else {
        manager.StartRecording();
      }
      break;
    case State::kRecording: {
      std::optional<record_replay::Recording> recording =
          manager.StopRecording();
      if (!recording) {
        break;
      }

      BrowserWindowInterface* bwi = tab_->GetBrowserWindowInterface();
      // bwi can be null if the tab is not attached to a window.
      if (!bwi) {
        break;
      }
      BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(bwi);
      if (!browser_view) {
        break;
      }
      views::View* anchor_view = browser_view->GetLocationBarView()
                                     ->page_action_container()
                                     ->GetPageActionView(kActionRecordReplay);
      // anchor_view can be null if the page action is not visible.
      if (!anchor_view) {
        break;
      }

      auto* rdm = client->GetRecordingDataManager();
      if (!rdm) {
        break;
      }

      record_replay::SaveRecordingBubbleView::Show(
          anchor_view, tab_->GetContents(),
          std::make_unique<record_replay::SaveRecordingBubbleControllerImpl>(
              std::move(*recording), rdm,
              base::BindOnce(&record_replay::RecordReplayManager::ReportToUser,
                             manager.GetWeakPtr()),
              base::BindOnce(&RecordReplayPageActionController::UpdateState,
                             base::Unretained(this))));
      break;
    }
    case State::kReplaying:
      manager.StopReplay();
      break;
  }
  UpdateState();
}

void RecordReplayPageActionController::UpdateState() {
  record_replay::RecordReplayClient* client =
      tab_->GetTabFeatures()->record_replay_client();
  if (!client) {
    page_action_controller_->Hide(kActionRecordReplay);
    return;
  }
  page_action_controller_->Show(kActionRecordReplay);

  client->GetManager().GetMatchingRecording(base::BindOnce(
      &RecordReplayPageActionController::OnRetrieveRecordingComplete,
      weak_ptr_factory_.GetWeakPtr()));
}

void RecordReplayPageActionController::OnRetrieveRecordingComplete(
    std::optional<record_replay::Recording> r) {
  has_recording_ = r.has_value();

  record_replay::RecordReplayClient* client =
      tab_->GetTabFeatures()->record_replay_client();
  if (!client) {
    return;
  }

  std::u16string tooltip_text;
  const gfx::VectorIcon* icon = &vector_icons::kScreenRecordIcon;
  SkColor color = {};

  switch (client->GetManager().state()) {
    case State::kIdle:
      if (has_recording_) {
        tooltip_text = u"Replay";
        icon = &vector_icons::kPlayArrowIcon;
        color = gfx::kGoogleGreen600;
      } else {
        tooltip_text = u"Record";
        icon = &vector_icons::kScreenRecordIcon;
        color = gfx::kGoogleRed600;
      }
      break;
    case State::kRecording:
      tooltip_text = u"Stop recording";
      icon = &vector_icons::kStopCircleIcon;
      color = gfx::kGoogleRed600;
      break;
    case State::kReplaying:
      tooltip_text = u"Stop replay";
      icon = &vector_icons::kStopCircleIcon;
      color = gfx::kGoogleGreen600;
      break;
  }

  page_action_controller_->OverrideTooltip(kActionRecordReplay, tooltip_text);
  page_action_controller_->OverrideAccessibleName(kActionRecordReplay,
                                                  tooltip_text);
  page_action_controller_->OverrideImage(
      kActionRecordReplay, ui::ImageModel::FromVectorIcon(*icon, color, 16),
      page_actions::PageActionColorSource::kForeground);
}
