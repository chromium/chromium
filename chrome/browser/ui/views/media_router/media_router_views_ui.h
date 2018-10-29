// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_MEDIA_ROUTER_MEDIA_ROUTER_VIEWS_UI_H_
#define CHROME_BROWSER_UI_VIEWS_MEDIA_ROUTER_MEDIA_ROUTER_VIEWS_UI_H_

#include "base/macros.h"
#include "base/observer_list.h"
#include "chrome/browser/ui/media_router/cast_dialog_controller.h"
#include "chrome/browser/ui/media_router/cast_dialog_model.h"
#include "chrome/browser/ui/media_router/media_router_ui_base.h"

namespace media_router {

// Functions as an intermediary between MediaRouter and Views Cast dialog.
class MediaRouterViewsUI : public MediaRouterUIBase,
                           public CastDialogController {
 public:
  MediaRouterViewsUI();
  ~MediaRouterViewsUI() override;

  // CastDialogController:
  void AddObserver(CastDialogController::Observer* observer) override;
  void RemoveObserver(CastDialogController::Observer* observer) override;
  void StartCasting(const std::string& sink_id,
                    MediaCastMode cast_mode) override;
  void StopCasting(const std::string& route_id) override;

  // MediaRouterUIBase:
  std::vector<MediaSinkWithCastModes> GetEnabledSinks() const override;

 private:
  FRIEND_TEST_ALL_PREFIXES(MediaRouterViewsUITest, NotifyObserver);
  FRIEND_TEST_ALL_PREFIXES(MediaRouterViewsUITest, SinkFriendlyName);
  FRIEND_TEST_ALL_PREFIXES(MediaRouterViewsUITest, SetDialogHeader);
  FRIEND_TEST_ALL_PREFIXES(MediaRouterViewsUITest, RemovePseudoSink);
  FRIEND_TEST_ALL_PREFIXES(MediaRouterViewsUITest, ConnectingState);
  FRIEND_TEST_ALL_PREFIXES(MediaRouterViewsUITest, DisconnectingState);
  FRIEND_TEST_ALL_PREFIXES(MediaRouterViewsUITest, AddAndRemoveIssue);
  FRIEND_TEST_ALL_PREFIXES(MediaRouterViewsUITest, ShowDomainForHangouts);

  // MediaRouterUIBase:
  void InitCommon(content::WebContents* initiator) override;
  void OnRoutesUpdated(
      const std::vector<MediaRoute>& routes,
      const std::vector<MediaRoute::Id>& joinable_route_ids) override;
  void UpdateSinks() override;
  void OnIssue(const Issue& issue) override;
  void OnIssueCleared() override;
  void OnDefaultPresentationChanged(
      const content::PresentationRequest& presentation_request) override;
  void OnDefaultPresentationRemoved() override;

  // Update the header text in the dialog model and notify observers.
  void UpdateModelHeader();

  UIMediaSink ConvertToUISink(const MediaSinkWithCastModes& sink,
                              const MediaRoute* route,
                              const base::Optional<Issue>& issue);

  // MediaRouterFileDialogDelegate:
  void FileDialogFileSelected(const ui::SelectedFileInfo& file_info) override;
  void FileDialogSelectionFailed(const IssueInfo& issue) override;

  // This value is set whenever there is an outstanding issue.
  base::Optional<Issue> issue_;

  // Contains up-to-date data to show in the dialog.
  CastDialogModel model_;

  // This value is set when the user opens a file picker, and used when a file
  // is selected and casting starts.
  base::Optional<MediaSink::Id> local_file_sink_id_;

  // This value is set when the UI requests a route to be terminated, and gets
  // reset when the route is removed.
  base::Optional<MediaRoute::Id> terminating_route_id_;

  // Observers for dialog model updates.
  // TODO(takumif): CastDialogModel should manage the observers.
  base::ObserverList<CastDialogController::Observer>::Unchecked observers_;

  DISALLOW_COPY_AND_ASSIGN(MediaRouterViewsUI);
};

}  // namespace media_router

#endif  // CHROME_BROWSER_UI_VIEWS_MEDIA_ROUTER_MEDIA_ROUTER_VIEWS_UI_H_
