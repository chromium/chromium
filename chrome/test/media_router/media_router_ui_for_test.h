// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_MEDIA_ROUTER_MEDIA_ROUTER_UI_FOR_TEST_H_
#define CHROME_TEST_MEDIA_ROUTER_MEDIA_ROUTER_UI_FOR_TEST_H_

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "chrome/browser/ui/views/media_router/cast_dialog_view.h"
#include "chrome/browser/ui/views/media_router/media_router_dialog_controller_views.h"
#include "chrome/common/media_router/media_sink.h"
#include "chrome/common/media_router/media_source.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class WebContents;
}  // namespace content

namespace media_router {

class MediaRouterUiForTest
    : public content::WebContentsUserData<MediaRouterUiForTest>,
      public CastDialogView::Observer {
 public:
  static MediaRouterUiForTest* GetOrCreateForWebContents(
      content::WebContents* web_contents);

  ~MediaRouterUiForTest() override;

  // Cleans up after a test.
  void TearDown();

  void ShowDialog();
  void HideDialog();
  bool IsDialogShown() const;

  // Chooses the source type in the dialog. Requires that the dialog is shown.
  void ChooseSourceType(CastDialogView::SourceType source_type);
  CastDialogView::SourceType GetChosenSourceType() const;

  // These methods require that the dialog is shown and the specified sink is
  // shown in the dialog.
  void StartCasting(const std::string& sink_name);
  void StopCasting(const std::string& sink_name);
  // Stops casting to the first active sink found on the sink list. Requires
  // that such a sink exists.
  void StopCasting();

  // Waits until a condition is met. Requires that the dialog is shown.
  void WaitForSink(const std::string& sink_name);
  void WaitForSinkAvailable(const std::string& sink_name);
  void WaitForAnyIssue();
  void WaitForAnyRoute();
  void WaitForDialogShown();
  void WaitForDialogHidden();
  void WaitUntilNoRoutes();

  // These methods require that the dialog is shown, and the sink specified by
  // |sink_name| is in the dialog.
  MediaRoute::Id GetRouteIdForSink(const std::string& sink_name) const;
  std::string GetStatusTextForSink(const std::string& sink_name) const;
  std::string GetIssueTextForSink(const std::string& sink_name) const;

  // Sets up a mock file picker that returns |file_url| as the selected file.
  void SetLocalFile(const GURL& file_url);
  // Sets up a mock file picker that fails with |issue|.
  void SetLocalFileSelectionIssue(const IssueInfo& issue);

  content::WebContents* web_contents() const { return web_contents_; }

 private:
  friend class content::WebContentsUserData<MediaRouterUiForTest>;

  enum class WatchType {
    kNone,
    kSink,           // Sink is found in any state.
    kSinkAvailable,  // Sink is found in the "Available" state.
    kAnyIssue,
    kAnyRoute,
    kDialogShown,
    kDialogHidden
  };

  explicit MediaRouterUiForTest(content::WebContents* web_contents);

  // CastDialogView::Observer:
  void OnDialogModelUpdated(CastDialogView* dialog_view) override;
  void OnDialogWillClose(CastDialogView* dialog_view) override;

  // Called by MediaRouterDialogControllerViews.
  void OnDialogCreated();

  CastDialogSinkButton* GetSinkButton(const std::string& sink_name) const;

  // Registers itself as an observer to the dialog, and waits until an event
  // of |watch_type| is observed. |sink_name| should be set only if observing
  // for a sink.
  void ObserveDialog(WatchType watch_type,
                     base::Optional<std::string> sink_name = base::nullopt);

  content::WebContents* web_contents_;
  MediaRouterDialogControllerViews* dialog_controller_;

  base::Optional<std::string> watch_sink_name_;
  base::Optional<base::OnceClosure> watch_callback_;
  WatchType watch_type_ = WatchType::kNone;

  base::WeakPtrFactory<MediaRouterUiForTest> weak_factory_{this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();

  DISALLOW_COPY_AND_ASSIGN(MediaRouterUiForTest);
};

}  // namespace media_router

#endif  // CHROME_TEST_MEDIA_ROUTER_MEDIA_ROUTER_UI_FOR_TEST_H_
