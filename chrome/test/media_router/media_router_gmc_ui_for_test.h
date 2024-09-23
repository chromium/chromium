// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_MEDIA_ROUTER_MEDIA_ROUTER_GMC_UI_FOR_TEST_H_
#define CHROME_TEST_MEDIA_ROUTER_MEDIA_ROUTER_GMC_UI_FOR_TEST_H_

#include <optional>

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/views/global_media_controls/media_dialog_ui_for_test.h"
#include "chrome/browser/ui/views/media_router/cast_dialog_view.h"
#include "chrome/browser/ui/views/media_router/media_router_dialog_controller_views.h"
#include "chrome/test/media_router/media_router_ui_for_test_base.h"
#include "components/media_router/common/media_sink.h"
#include "components/media_router/common/media_source.h"

class Browser;

namespace media_router {

class MediaRouterGmcUiForTest : public MediaRouterUiForTestBase {
 public:
  MediaRouterGmcUiForTest(const MediaRouterGmcUiForTest&) = delete;
  MediaRouterGmcUiForTest& operator=(const MediaRouterGmcUiForTest&) = delete;

  explicit MediaRouterGmcUiForTest(content::WebContents* web_contents);
  ~MediaRouterGmcUiForTest() override;

  // MediaRouterUiForTestBase:
  void SetUp() override;
  void ShowDialog() override;
  bool IsDialogShown() const override;
  void HideDialog() override;
  void ChooseSourceType(CastDialogView::SourceType source_type) override;
  CastDialogView::SourceType GetChosenSourceType() const override;
  void StartCasting(const std::string& sink_name) override;
  void StopCasting(const std::string& sink_name) override;
  std::string GetRouteIdForSink(const std::string& sink_name) const override;
  std::string GetStatusTextForSink(const std::string& sink_name) const override;
  std::string GetIssueTextForSink(const std::string& sink_name) const override;
  void WaitForSink(const std::string& sink_name) override;
  void WaitForSinkAvailable(const std::string& sink_name) override;
  void WaitForAnyIssue() override;
  void WaitForAnyRoute() override;
  void WaitForDialogShown() override;
  void WaitForDialogHidden() override;

 private:
  // MediaRouterUiForTestBase:
  views::Button* GetSinkButton(const std::string& sink_name) const override;

  void ObserveDialog(
      WatchType watch_type,
      std::optional<std::string> sink_name = std::nullopt) override;

  Browser* browser() const { return browser_; }

  const raw_ptr<Browser> browser_;
  MediaDialogUiForTest dialog_ui_{
      base::BindRepeating(&MediaRouterGmcUiForTest::browser,
                          base::Unretained(this))};
  base::WeakPtrFactory<MediaRouterGmcUiForTest> weak_factory_{this};
};

}  // namespace media_router

#endif  // CHROME_TEST_MEDIA_ROUTER_MEDIA_ROUTER_GMC_UI_FOR_TEST_H_
