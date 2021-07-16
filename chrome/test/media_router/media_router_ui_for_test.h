// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_MEDIA_ROUTER_MEDIA_ROUTER_UI_FOR_TEST_H_
#define CHROME_TEST_MEDIA_ROUTER_MEDIA_ROUTER_UI_FOR_TEST_H_

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/views/media_router/cast_dialog_view.h"
#include "chrome/browser/ui/views/media_router/media_router_dialog_controller_views.h"
#include "components/media_router/common/media_sink.h"
#include "components/media_router/common/media_source.h"
#include "content/public/browser/web_contents_user_data.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

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

  // TODO(crbug.com/1229305): Move Zenith-related code into its own class.
  void ShowCastDialog();
  bool IsCastDialogShown() const;
  bool IsGMCDialogShown() const;
  void HideCastDialog();
  void HideGMCDialog();

  // Chooses the source type in the dialog. Requires that the dialog is shown.
  void ChooseSourceType(CastDialogView::SourceType source_type);
  CastDialogView::SourceType GetChosenSourceType() const;

  // These methods require that the dialog is shown and the specified sink is
  // shown in the dialog.
  void StartCastingFromCastDialog(const std::string& sink_name);
  void StartCastingFromGMCDialog(const std::string& sink_name);
  void StopCastingFromCastDialog(const std::string& sink_name);
  void StopCastingFromGMCDialog(const std::string& sink_name);

  // Waits until a condition is met. Requires that the dialog is shown.
  void WaitForSink(const std::string& sink_name);
  void WaitForSinkAvailable(const std::string& sink_name);
  void WaitForAnyIssue();
  void WaitForAnyRoute();
  void WaitForCastDialogShown();
  void WaitForGMCDialogShown();
  void WaitForCastDialogHidden();
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

  void StartCasting(CastDialogSinkButton* sink_button);
  void StopCasting(CastDialogSinkButton* sink_button);
  void WaitForDialogShown();

  CastDialogSinkButton* GetSinkButton(const std::string& sink_name) const;
  CastDialogSinkButton* GetSinkButtonFromCastDialog(
      const std::string& sink_name) const;
  CastDialogSinkButton* GetSinkButtonFromGMCDialog(
      const std::string& sink_name) const;

  // Registers itself as an observer to the dialog, and waits until an event
  // of |watch_type| is observed. |sink_name| should be set only if observing
  // for a sink.
  void ObserveDialog(WatchType watch_type,
                     absl::optional<std::string> sink_name = absl::nullopt);

  content::WebContents* web_contents_;
  MediaRouterDialogControllerViews* dialog_controller_;

  absl::optional<std::string> watch_sink_name_;
  absl::optional<base::OnceClosure> watch_callback_;
  WatchType watch_type_ = WatchType::kNone;

  base::WeakPtrFactory<MediaRouterUiForTest> weak_factory_{this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();

  DISALLOW_COPY_AND_ASSIGN(MediaRouterUiForTest);
};

}  // namespace media_router

#endif  // CHROME_TEST_MEDIA_ROUTER_MEDIA_ROUTER_UI_FOR_TEST_H_
