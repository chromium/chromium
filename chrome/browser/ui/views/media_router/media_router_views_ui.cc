// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/media_router/media_router_views_ui.h"

#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/media/router/media_router_metrics.h"
#include "chrome/browser/ui/media_router/media_sink_with_cast_modes.h"
#include "chrome/browser/ui/media_router/ui_media_sink.h"
#include "chrome/common/media_router/route_request_result.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace media_router {

namespace {

// Returns true if |issue| is associated with |ui_sink|.
bool IssueMatches(const Issue& issue, const UIMediaSink& ui_sink) {
  return issue.info().sink_id == ui_sink.id ||
         (!issue.info().route_id.empty() &&
          issue.info().route_id == ui_sink.route_id);
}

base::string16 GetSinkFriendlyName(const MediaSink& sink) {
  // Use U+2010 (HYPHEN) instead of ASCII hyphen to avoid problems with RTL
  // languages.
  const char* separator = u8" \u2010 ";
  return base::UTF8ToUTF16(sink.description() ? sink.name() + separator +
                                                    sink.description().value()
                                              : sink.name());
}

}  // namespace

MediaRouterViewsUI::MediaRouterViewsUI() = default;

MediaRouterViewsUI::~MediaRouterViewsUI() {
  for (CastDialogController::Observer& observer : observers_)
    observer.OnControllerInvalidated();
}

void MediaRouterViewsUI::AddObserver(CastDialogController::Observer* observer) {
  observers_.AddObserver(observer);
  // TODO(takumif): Update the header when this object is initialized instead.
  UpdateModelHeader();
}

void MediaRouterViewsUI::RemoveObserver(
    CastDialogController::Observer* observer) {
  observers_.RemoveObserver(observer);
}

void MediaRouterViewsUI::StartCasting(const std::string& sink_id,
                                      MediaCastMode cast_mode) {
  if (cast_mode == LOCAL_FILE) {
    local_file_sink_id_ = sink_id;
    OpenFileDialog();
  } else {
    CreateRoute(sink_id, cast_mode);
    UpdateSinks();
  }
}

void MediaRouterViewsUI::StopCasting(const std::string& route_id) {
  terminating_route_id_ = route_id;
  // |route_id| may become invalid after UpdateSinks(), so we cannot refer to
  // |route_id| below this line.
  UpdateSinks();
  TerminateRoute(terminating_route_id_.value());
}

std::vector<MediaSinkWithCastModes> MediaRouterViewsUI::GetEnabledSinks()
    const {
  std::vector<MediaSinkWithCastModes> sinks =
      MediaRouterUIBase::GetEnabledSinks();
  // Remove the pseudo-sink, since it's only used in the WebUI dialog.
  // TODO(takumif): Remove this once we've removed pseudo-sink from Cloud MRP.
  base::EraseIf(sinks, [](const MediaSinkWithCastModes& sink) {
    return base::StartsWith(sink.sink.id(),
                            "pseudo:", base::CompareCase::SENSITIVE);
  });
  return sinks;
}

void MediaRouterViewsUI::InitCommon(content::WebContents* initiator) {
  MediaRouterUIBase::InitCommon(initiator);
  // We don't start observing issues in MediaRouterUIBase::InitCommon() because
  // in the WebUI dialog, we need to wait for the WebUI to load before
  // starting to observe.
  StartObservingIssues();
}

void MediaRouterViewsUI::OnRoutesUpdated(
    const std::vector<MediaRoute>& routes,
    const std::vector<MediaRoute::Id>& joinable_route_ids) {
  MediaRouterUIBase::OnRoutesUpdated(routes, joinable_route_ids);
  if (terminating_route_id_ &&
      std::find_if(
          routes.begin(), routes.end(), [this](const MediaRoute& route) {
            return route.media_route_id() == terminating_route_id_.value();
          }) == routes.end()) {
    terminating_route_id_.reset();
  }
  UpdateSinks();
}

void MediaRouterViewsUI::UpdateSinks() {
  std::vector<UIMediaSink> media_sinks;
  for (const MediaSinkWithCastModes& sink : GetEnabledSinks()) {
    auto route_it = std::find_if(
        routes().begin(), routes().end(), [&sink](const MediaRoute& route) {
          return route.media_sink_id() == sink.sink.id();
        });
    const MediaRoute* route = route_it == routes().end() ? nullptr : &*route_it;
    media_sinks.push_back(ConvertToUISink(sink, route, issue_));
  }
  model_.set_media_sinks(std::move(media_sinks));
  for (CastDialogController::Observer& observer : observers_)
    observer.OnModelUpdated(model_);
}

UIMediaSink MediaRouterViewsUI::ConvertToUISink(
    const MediaSinkWithCastModes& sink,
    const MediaRoute* route,
    const base::Optional<Issue>& issue) {
  UIMediaSink ui_sink;
  ui_sink.id = sink.sink.id();
  ui_sink.friendly_name = GetSinkFriendlyName(sink.sink);
  ui_sink.icon_type = sink.sink.icon_type();

  if (route) {
    ui_sink.status_text = base::UTF8ToUTF16(route->description());
    ui_sink.route_id = route->media_route_id();
    ui_sink.state = terminating_route_id_ && route->media_route_id() ==
                                                 terminating_route_id_.value()
                        ? UIMediaSinkState::DISCONNECTING
                        : UIMediaSinkState::CONNECTED;
  } else {
    ui_sink.state = current_route_request() &&
                            sink.sink.id() == current_route_request()->sink_id
                        ? UIMediaSinkState::CONNECTING
                        : UIMediaSinkState::AVAILABLE;
    ui_sink.cast_modes = sink.cast_modes;
  }
  if (ui_sink.icon_type == SinkIconType::HANGOUT &&
      ui_sink.state == UIMediaSinkState::AVAILABLE && sink.sink.domain()) {
    ui_sink.status_text = base::UTF8ToUTF16(*sink.sink.domain());
  }
  if (issue && IssueMatches(*issue, ui_sink))
    ui_sink.issue = issue;
  return ui_sink;
}

void MediaRouterViewsUI::OnIssue(const Issue& issue) {
  issue_ = issue;
  UpdateSinks();
}

void MediaRouterViewsUI::OnIssueCleared() {
  issue_ = base::nullopt;
  UpdateSinks();
}

void MediaRouterViewsUI::OnDefaultPresentationChanged(
    const content::PresentationRequest& presentation_request) {
  // This sets the default cast mode to presentation when the dialog is opened.
  // So we need to update the header to reflect that.
  MediaRouterUIBase::OnDefaultPresentationChanged(presentation_request);
  UpdateModelHeader();
}

void MediaRouterViewsUI::OnDefaultPresentationRemoved() {
  MediaRouterUIBase::OnDefaultPresentationRemoved();
  UpdateModelHeader();
}

void MediaRouterViewsUI::UpdateModelHeader() {
  const std::string source_name = GetTruncatedPresentationRequestSourceName();
  const base::string16 header_text =
      source_name.empty()
          ? l10n_util::GetStringUTF16(IDS_MEDIA_ROUTER_CAST_DIALOG_TITLE)
          : l10n_util::GetStringFUTF16(IDS_MEDIA_ROUTER_PRESENTATION_CAST_MODE,
                                       base::UTF8ToUTF16(source_name));
  model_.set_dialog_header(header_text);
  for (CastDialogController::Observer& observer : observers_)
    observer.OnModelUpdated(model_);
}

void MediaRouterViewsUI::FileDialogFileSelected(
    const ui::SelectedFileInfo& file_info) {
  CreateRoute(local_file_sink_id_.value(), LOCAL_FILE);
  local_file_sink_id_.reset();
}

void MediaRouterViewsUI::FileDialogSelectionFailed(const IssueInfo& issue) {
  MediaRouterUIBase::FileDialogSelectionFailed(issue);
  local_file_sink_id_.reset();
}

}  // namespace media_router
