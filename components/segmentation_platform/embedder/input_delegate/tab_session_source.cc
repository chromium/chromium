// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/input_delegate/tab_session_source.h"

#include "base/strings/string_number_conversions_win.h"
#include "base/strings/string_piece.h"
#include "base/time/time.h"
#include "components/segmentation_platform/internal/execution/processing/feature_processor_state.h"
#include "components/segmentation_platform/public/types/processed_value.h"
#include "components/sessions/core/session_id.h"
#include "components/sessions/core/session_types.h"
#include "components/sync_sessions/open_tabs_ui_delegate.h"
#include "components/sync_sessions/synced_session.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/page_transition_types.h"

namespace segmentation_platform::processing {
namespace {

absl::optional<ProcessedValue> GetMetadataArgument(
    const FeatureProcessorState& feature_processor_state,
    base::StringPiece arg_name) {
  const auto& args = feature_processor_state.input_context()->metadata_args;
  auto it = args.find(arg_name);
  if (it == args.end()) {
    return absl::nullopt;
  }
  return it->second;
}

}  // namespace

TabSessionSource::TabSessionSource(
    sync_sessions::SessionSyncService* session_sync_service)
    : session_sync_service_(session_sync_service) {}

TabSessionSource::~TabSessionSource() = default;

void TabSessionSource::Process(
    const proto::CustomInput& input,
    const FeatureProcessorState& feature_processor_state,
    ProcessedCallback callback) {
  auto tab_id_val = GetMetadataArgument(feature_processor_state, "tab_id");
  auto session_tag_val =
      GetMetadataArgument(feature_processor_state, "session_tag");
  if (!session_tag_val || !tab_id_val) {
    std::move(callback).Run(/*error=*/true, {});
    return;
  }

  CHECK_EQ(tab_id_val->type, ProcessedValue::Type::INT);
  CHECK_EQ(session_tag_val->type, ProcessedValue::STRING);
  SessionID::id_type tab_id = tab_id_val->int_val;
  std::string session_tag = session_tag_val->str_val;

  // Fetch the tab. The tab might have been closed between the time was fetched
  // and the model is run asynchronously.
  sync_sessions::OpenTabsUIDelegate* open_tab_delegate =
      session_sync_service_->GetOpenTabsUIDelegate();
  const sessions::SessionTab* session_tab = nullptr;
  if (!open_tab_delegate ||
      !open_tab_delegate->GetForeignTab(
          session_tag, SessionID::FromSerializedValue(tab_id), &session_tab)) {
    std::move(callback).Run(/*error=*/true, Tensor());
    return;
  }

  Tensor inputs(kNumInputs, ProcessedValue(0.0f));

  AddTabInfo(session_tab, inputs);
  AddTabRanks(session_tag, session_tab, inputs);

  std::move(callback).Run(false, std::move(inputs));
}

void TabSessionSource::AddTabInfo(const sessions::SessionTab* session_tab,
                                  Tensor& inputs) {
  base::TimeDelta time_since_modified =
      base::Time::Now() - session_tab->timestamp;

  // The navigation index could be invalid if it is larger than the navigation
  // list, so pick the index when available, else pick the last navigation
  // available.
  int navigation_index =
      static_cast<size_t>(session_tab->current_navigation_index) >=
              session_tab->navigations.size()
          ? session_tab->navigations.size() - 1
          : session_tab->current_navigation_index;
  base::TimeDelta time_since_last_nav;
  base::TimeDelta time_since_first_nav;
  ui::PageTransition last_transition = ui::PAGE_TRANSITION_TYPED;

  if (session_tab->navigations.size() > 0) {
    const auto& current_navigation = session_tab->navigations[navigation_index];
    time_since_last_nav = base::Time::Now() - current_navigation.timestamp();
    time_since_first_nav =
        base::Time::Now() - session_tab->navigations[0].timestamp();
    last_transition = current_navigation.transition_type();
  }
  int password_used_count = 0;
  for (const auto& navigation : session_tab->navigations) {
    if (navigation.password_state() == sessions::SerializedNavigationEntry::
                                           PasswordState::HAS_PASSWORD_FIELD) {
      password_used_count++;
    }
  }

  inputs[kInputTimeSinceModifiedSec] =
      ProcessedValue::FromFloat(time_since_modified.InSeconds());
  inputs[kInputTimeSinceLastNavSec] =
      ProcessedValue::FromFloat(time_since_last_nav.InSeconds());
  inputs[kInputTimeSinceFirstNavSec] =
      ProcessedValue::FromFloat(time_since_first_nav.InSeconds());
  inputs[kInputLastTransitionType] = ProcessedValue::FromFloat(last_transition);
  inputs[kInputPasswordFieldCount] =
      ProcessedValue::FromFloat(password_used_count);
}

void TabSessionSource::AddTabRanks(const std::string& session_tag,
                                   const sessions::SessionTab* session_tab,
                                   Tensor& inputs) {
  sync_sessions::OpenTabsUIDelegate* open_tab_delegate =
      session_sync_service_->GetOpenTabsUIDelegate();
  std::vector<const sessions::SessionWindow*> windows;
  int tab_rank_in_session = 0;
  if (open_tab_delegate->GetForeignSession(session_tag, &windows)) {
    for (const auto* window : windows) {
      for (const auto& tab : window->tabs) {
        if (tab->timestamp > session_tab->timestamp) {
          tab_rank_in_session++;
        }
      }
    }
  }
  std::vector<const sync_sessions::SyncedSession*> sessions;
  int session_rank_overall = 0;
  if (open_tab_delegate->GetAllForeignSessions(&sessions)) {
    for (const auto* session : sessions) {
      if (session->GetModifiedTime() > session_tab->timestamp) {
        session_rank_overall++;
      }
    }
  }

  inputs[kInputTabRankInSession] =
      ProcessedValue::FromFloat(tab_rank_in_session);
  inputs[kInputSessionRank] = ProcessedValue::FromFloat(session_rank_overall);
}

}  // namespace segmentation_platform::processing
