// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/media_router/media_router_gmc_ui_for_test.h"

#include "base/notreached.h"
#include "base/run_loop.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/views/global_media_controls/media_dialog_view.h"
#include "chrome/browser/ui/views/global_media_controls/media_item_ui_device_selector_view.h"
#include "chrome/browser/ui/views/global_media_controls/media_toolbar_button_view.h"
#include "chrome/browser/ui/views/media_router/media_router_dialog_controller_views.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "components/global_media_controls/public/views/media_item_ui_view.h"

namespace media_router {

MediaRouterGmcUiForTest::MediaRouterGmcUiForTest(
    content::WebContents* web_contents)
    : MediaRouterUiForTestBase(web_contents),
      browser_(chrome::FindBrowserWithTab(web_contents)) {
  DCHECK(browser_);
}

MediaRouterGmcUiForTest::~MediaRouterGmcUiForTest() {
  CHECK(!watch_callback_);
}

void MediaRouterGmcUiForTest::SetUp() {
  feature_list_.InitAndEnableFeature(kGlobalMediaControlsCastStartStop);
}

void MediaRouterGmcUiForTest::ShowDialog() {
  dialog_ui_.ClickToolbarIcon();
  CHECK(dialog_ui_.WaitForDialogOpened());
}

bool MediaRouterGmcUiForTest::IsDialogShown() const {
  return MediaDialogView::IsShowing();
}

void MediaRouterGmcUiForTest::HideDialog() {
  return MediaDialogView::HideDialog();
}

void MediaRouterGmcUiForTest::ChooseSourceType(
    CastDialogView::SourceType source_type) {
  NOTIMPLEMENTED();
}

CastDialogView::SourceType MediaRouterGmcUiForTest::GetChosenSourceType()
    const {
  NOTIMPLEMENTED();
  return CastDialogView::SourceType();
}

void MediaRouterGmcUiForTest::StartCasting(const std::string& sink_name) {
  ClickOnView(GetSinkButton(sink_name));
}

void MediaRouterGmcUiForTest::StopCasting(const std::string& sink_name) {
  NOTIMPLEMENTED();
}

std::string MediaRouterGmcUiForTest::GetRouteIdForSink(
    const std::string& sink_name) const {
  NOTIMPLEMENTED();
  return "";
}

std::string MediaRouterGmcUiForTest::GetStatusTextForSink(
    const std::string& sink_name) const {
  auto* device_view = GetDeviceView(sink_name);
  if (!device_view) {
    return "";
  }
  return device_view->GetStatusTextForTest();
}

std::string MediaRouterGmcUiForTest::GetIssueTextForSink(
    const std::string& sink_name) const {
  return GetStatusTextForSink(sink_name);
}

void MediaRouterGmcUiForTest::WaitForSink(const std::string& sink_name) {
  ObserveDialog(WatchType::kSink, sink_name);
}

void MediaRouterGmcUiForTest::WaitForSinkAvailable(
    const std::string& sink_name) {
  ObserveDialog(WatchType::kSinkAvailable, sink_name);
}

void MediaRouterGmcUiForTest::WaitForAnyIssue() {
  ObserveDialog(WatchType::kAnyIssue);
}

void MediaRouterGmcUiForTest::WaitForAnyRoute() {
  ObserveDialog(WatchType::kAnyRoute);
}

void MediaRouterGmcUiForTest::WaitForDialogShown() {
  CHECK(dialog_ui_.WaitForDialogOpened());
}

void MediaRouterGmcUiForTest::WaitForDialogHidden() {
  NOTIMPLEMENTED();
}

views::View* MediaRouterGmcUiForTest::GetSinkButton(
    const std::string& sink_name) const {
  return GetDeviceView(sink_name);
}

CastDeviceEntryView* MediaRouterGmcUiForTest::GetDeviceView(
    const std::string& device_name) const {
  DCHECK(IsDialogShown());
  auto items = MediaDialogView::GetDialogViewForTesting()->GetItemsForTesting();
  global_media_controls::MediaItemUIView* view = items.begin()->second;
  auto* device_selector = static_cast<MediaItemUIDeviceSelectorView*>(
      view->device_selector_view_for_testing());
  auto device_views = device_selector->GetCastDeviceEntryViewsForTesting();
  for (auto* device_view : device_views) {
    if (device_view->device_name() == device_name) {
      return device_view;
    }
  }
  return nullptr;
}

void MediaRouterGmcUiForTest::ObserveDialog(
    WatchType watch_type,
    absl::optional<std::string> sink_name) {
  CHECK(!watch_sink_name_);
  CHECK(!watch_callback_);
  CHECK_EQ(watch_type_, WatchType::kNone);
  base::RunLoop run_loop;
  watch_sink_name_ = std::move(sink_name);
  watch_callback_ = run_loop.QuitClosure();
  watch_type_ = watch_type;
  std::move(*watch_callback_).Run();
  watch_callback_.reset();
  watch_sink_name_.reset();
  watch_type_ = WatchType::kNone;
  base::RunLoop().RunUntilIdle();
}

}  // namespace media_router
