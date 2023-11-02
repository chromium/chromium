// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_MEDIA_ROUTER_MEDIA_ROUTER_GMC_UI_FOR_TEST_H_
#define CHROME_TEST_MEDIA_ROUTER_MEDIA_ROUTER_GMC_UI_FOR_TEST_H_

#include "base/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/views/global_media_controls/media_dialog_ui_for_test.h"
#include "chrome/browser/ui/views/media_router/cast_dialog_view.h"
#include "chrome/browser/ui/views/media_router/media_router_dialog_controller_views.h"
#include "chrome/test/media_router/media_router_ui_for_test_base.h"
#include "components/media_router/common/media_sink.h"
#include "components/media_router/common/media_source.h"
#include "content/public/browser/web_contents_user_data.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class Browser;

namespace media_router {

class MediaRouterGmcUiForTest
    : public MediaRouterUiForTestBase,
      public content::WebContentsUserData<MediaRouterGmcUiForTest> {
 public:
  static MediaRouterGmcUiForTest* GetOrCreateForWebContents(
      content::WebContents* web_contents);

  MediaRouterGmcUiForTest(const MediaRouterGmcUiForTest&) = delete;
  MediaRouterGmcUiForTest& operator=(const MediaRouterGmcUiForTest&) = delete;

  ~MediaRouterGmcUiForTest() override;

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

 private:
  friend class content::WebContentsUserData<MediaRouterGmcUiForTest>;

  explicit MediaRouterGmcUiForTest(content::WebContents* web_contents);

  // MediaRouterUiForTestBase:
  CastDialogSinkButton* GetSinkButton(
      const std::string& sink_name) const override;

  void ObserveDialog(
      WatchType watch_type,
      absl::optional<std::string> sink_name = absl::nullopt) override;

  Browser* browser() const { return browser_; }

  const raw_ptr<Browser> browser_;
  MediaDialogUiForTest dialog_ui_{
      base::BindRepeating(&MediaRouterGmcUiForTest::browser,
                          base::Unretained(this))};
  base::WeakPtrFactory<MediaRouterGmcUiForTest> weak_factory_{this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace media_router

#endif  // CHROME_TEST_MEDIA_ROUTER_MEDIA_ROUTER_GMC_UI_FOR_TEST_H_
