// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/media_router/cast_dialog_view.h"

#include "chrome/browser/ui/media_router/cast_dialog_controller.h"
#include "chrome/browser/ui/media_router/cast_dialog_model.h"
#include "chrome/browser/ui/media_router/media_route_starter.h"
#include "chrome/browser/ui/media_router/ui_media_sink.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/media_router/cast_dialog_coordinator.h"
#include "components/media_router/browser/presentation/start_presentation_context.h"
#include "components/media_router/common/mojom/media_router.mojom.h"
#include "content/public/test/browser_test.h"

namespace {

media_router::UIMediaSink CreateAvailableSink() {
  media_router::UIMediaSink sink{
      media_router::mojom::MediaRouteProviderId::CAST};
  sink.id = "sink_available";
  sink.friendly_name = u"TestAvailableSink";
  sink.state = media_router::UIMediaSinkState::AVAILABLE;
  sink.cast_modes = {media_router::TAB_MIRROR, media_router::DESKTOP_MIRROR};
  return sink;
}

media_router::UIMediaSink CreateConnectedSink() {
  media_router::UIMediaSink sink{
      media_router::mojom::MediaRouteProviderId::CAST};
  sink.id = "sink_connected";
  sink.friendly_name = u"TestConnectedSink";
  sink.state = media_router::UIMediaSinkState::CONNECTED;
  sink.cast_modes = {media_router::TAB_MIRROR, media_router::DESKTOP_MIRROR};
  sink.route = media_router::MediaRoute(
      "route_id", media_router::MediaSource("https://example.com"), sink.id, "",
      true);
  return sink;
}

media_router::UIMediaSink CreateUnavailableSink() {
  media_router::UIMediaSink sink{
      media_router::mojom::MediaRouteProviderId::CAST};
  sink.id = "sink_unavailable";
  sink.friendly_name = u"TestUnavailableSink";
  sink.state = media_router::UIMediaSinkState::UNAVAILABLE;
  sink.cast_modes = {media_router::TAB_MIRROR, media_router::DESKTOP_MIRROR};
  return sink;
}

media_router::CastDialogModel CreateModelWithSinks(
    std::vector<media_router::UIMediaSink> sinks) {
  media_router::CastDialogModel model;
  model.set_dialog_header(u"Dialog header");
  model.set_media_sinks(std::move(sinks));
  return model;
}

class MockCastDialogController : public media_router::CastDialogController {
 public:
  void AddObserver(Observer* observer) override {}
  void RemoveObserver(Observer* observer) override {}
  void StartCasting(const media_router::MediaSink::Id& sink_id,
                    media_router::MediaCastMode cast_mode) override {}
  void StopCasting(const media_router::MediaRoute::Id& route_id) override {}
  void ClearIssue(const media_router::Issue::Id& issue_id) override {}
  void FreezeRoute(const media_router::MediaRoute::Id& route_id) override {}
  void UnfreezeRoute(const media_router::MediaRoute::Id& route_id) override {}
  std::unique_ptr<media_router::MediaRouteStarter> TakeMediaRouteStarter()
      override {
    return nullptr;
  }
  void RegisterDestructor(base::OnceClosure destructor) override {}
};

}  // namespace

class CastDialogViewBrowserTest : public DialogBrowserTest {
 public:
  CastDialogViewBrowserTest()
      : controller_(std::make_unique<MockCastDialogController>()) {}

  CastDialogViewBrowserTest(const CastDialogViewBrowserTest&) = delete;
  CastDialogViewBrowserTest& operator=(const CastDialogViewBrowserTest&) =
      delete;

  // DialogBrowserTest:
  void PreShow() override {
    cast_dialog_coordinator_.ShowDialogCenteredForBrowserWindow(
        controller_.get(), browser(), base::Time::Now(),
        media_router::MediaRouterDialogActivationLocation::TOOLBAR);
  }

  void ShowUi(const std::string& name) override {
    media_router::CastDialogModel model;
    if (name == "Available") {
      model = CreateModelWithSinks({CreateAvailableSink()});
    } else if (name == "Connected") {
      model = CreateModelWithSinks({CreateConnectedSink()});
    } else if (name == "Unavailable") {
      model = CreateModelWithSinks({CreateUnavailableSink()});
    } else if (name == "Mixed") {
      model = CreateModelWithSinks({
          CreateAvailableSink(),
          CreateConnectedSink(),
          CreateUnavailableSink(),
      });
    } else {
      CHECK_EQ(name, "NoSinks");
      model = CreateModelWithSinks({});
    }
    media_router::CastDialogView* dialog =
        cast_dialog_coordinator_.GetCastDialogView();
    dialog->OnModelUpdated(model);
  }

 private:
  media_router::CastDialogCoordinator cast_dialog_coordinator_;
  std::unique_ptr<MockCastDialogController> controller_;
};

IN_PROC_BROWSER_TEST_F(CastDialogViewBrowserTest, InvokeUi_Available) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(CastDialogViewBrowserTest, InvokeUi_Connected) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(CastDialogViewBrowserTest, InvokeUi_Unavailable) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(CastDialogViewBrowserTest, InvokeUi_Mixed) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(CastDialogViewBrowserTest, InvokeUi_NoSinks) {
  ShowAndVerifyUi();
}
