// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_MEDIA_ROUTER_MEDIA_ROUTER_CAST_UI_FOR_TEST_H_
#define CHROME_TEST_MEDIA_ROUTER_MEDIA_ROUTER_CAST_UI_FOR_TEST_H_

#include "base/functional/callback_forward.h"
#include "chrome/browser/ui/views/media_router/media_router_dialog_controller_views.h"
#include "chrome/test/media_router/media_router_ui_for_test_base.h"
#include "components/media_router/common/media_sink.h"
#include "components/media_router/common/media_source.h"
#include "content/public/browser/web_contents_user_data.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace media_router {

class MediaRouterCastUiForTest
    : public MediaRouterUiForTestBase,
      public content::WebContentsUserData<MediaRouterCastUiForTest>,
      public CastDialogView::Observer {
 public:
  static MediaRouterCastUiForTest* GetOrCreateForWebContents(
      content::WebContents* web_contents);

  MediaRouterCastUiForTest(const MediaRouterCastUiForTest&) = delete;
  MediaRouterCastUiForTest& operator=(const MediaRouterCastUiForTest&) = delete;

  ~MediaRouterCastUiForTest() override;

  // MediaRouterUiForTestBase:
  void SetUp() override;
  void ShowDialog() override;
  bool IsDialogShown() const override;
  void HideDialog() override;
  void ChooseSourceType(CastDialogView::SourceType source_type) override;
  CastDialogView::SourceType GetChosenSourceType() const override;
  void WaitForSink(const std::string& sink_name) override;
  void WaitForSinkAvailable(const std::string& sink_name) override;
  void WaitForAnyIssue() override;
  void WaitForAnyRoute() override;
  void WaitForDialogShown() override;
  void WaitForDialogHidden() override;
  void OnDialogCreated() override;

 private:
  friend class content::WebContentsUserData<MediaRouterCastUiForTest>;

  explicit MediaRouterCastUiForTest(content::WebContents* web_contents);

  // CastDialogView::Observer:
  void OnDialogModelUpdated(CastDialogView* dialog_view) override;
  void OnDialogWillClose(CastDialogView* dialog_view) override;

  // MediaRouterUiForTestBase:
  CastDialogSinkButton* GetSinkButton(
      const std::string& sink_name) const override;

  // Registers itself as an observer to the dialog, and waits until an event
  // of |watch_type| is observed. |sink_name| should be set only if observing
  // for a sink.
  void ObserveDialog(
      WatchType watch_type,
      absl::optional<std::string> sink_name = absl::nullopt) override;

  const CastDialogView* GetDialogView() const;
  CastDialogView* GetDialogView();

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace media_router

#endif  // CHROME_TEST_MEDIA_ROUTER_MEDIA_ROUTER_CAST_UI_FOR_TEST_H_
