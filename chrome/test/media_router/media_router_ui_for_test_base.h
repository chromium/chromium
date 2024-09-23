// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_MEDIA_ROUTER_MEDIA_ROUTER_UI_FOR_TEST_BASE_H_
#define CHROME_TEST_MEDIA_ROUTER_MEDIA_ROUTER_UI_FOR_TEST_BASE_H_

#include <optional>

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/views/media_router/cast_dialog_view.h"
#include "chrome/browser/ui/views/media_router/media_router_dialog_controller_views.h"
#include "components/media_router/common/media_sink.h"
#include "components/media_router/common/media_source.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class WebContents;
}  // namespace content

namespace media_router {

class MediaRouterUiForTestBase {
 public:
  virtual void SetUp() = 0;

  // Destruction of the test helper happens in two phases: the user of this
  // helper must first call `TearDown()` before destroying it. `TearDown()`
  // itself needs to call virtual functions, so that logic cannot live in the
  // destructor itself: by the time the destructor of this base class runs, the
  // virtual methods will not work as expected, because the derived class's
  // destructor will have already completed.
  void TearDown();

  virtual void ShowDialog() = 0;
  virtual bool IsDialogShown() const = 0;
  virtual void HideDialog() = 0;

  // Chooses the source type in the dialog. Requires that the dialog is shown.
  virtual void ChooseSourceType(CastDialogView::SourceType source_type) = 0;
  virtual CastDialogView::SourceType GetChosenSourceType() const = 0;

  // These methods require that the dialog is shown and the specified sink is
  // shown in the dialog.
  virtual void StartCasting(const std::string& sink_name) = 0;
  virtual void StopCasting(const std::string& sink_name) = 0;

  // Waits until a condition is met. Requires that the dialog is shown.
  virtual void WaitForSink(const std::string& sink_name) = 0;
  virtual void WaitForSinkAvailable(const std::string& sink_name) = 0;
  virtual void WaitForAnyIssue() = 0;
  virtual void WaitForAnyRoute() = 0;
  virtual void WaitForDialogShown() = 0;
  virtual void WaitForDialogHidden() = 0;

  // These methods require that the dialog is shown, and the sink specified by
  // |sink_name| is in the dialog.
  virtual MediaRoute::Id GetRouteIdForSink(
      const std::string& sink_name) const = 0;
  virtual std::string GetStatusTextForSink(
      const std::string& sink_name) const = 0;
  virtual std::string GetIssueTextForSink(
      const std::string& sink_name) const = 0;

  // Called by MediaRouterDialogControllerViews.
  virtual void OnDialogCreated();

  content::WebContents* web_contents() const { return web_contents_; }

  virtual ~MediaRouterUiForTestBase();

 protected:
  enum class WatchType {
    kNone,
    kSink,           // Sink is found in any state.
    kSinkAvailable,  // Sink is found in the "Available" state.
    kAnyIssue,
    kAnyRoute,
    kDialogShown,
    kDialogHidden
  };

  explicit MediaRouterUiForTestBase(content::WebContents* web_contents);
  void WaitForAnyDialogShown();

  static void ClickOnButton(views::Button* button);

  virtual views::Button* GetSinkButton(const std::string& sink_name) const = 0;

  // Registers itself as an observer to the dialog, and waits until an event
  // of |watch_type| is observed. |sink_name| should be set only if observing
  // for a sink.
  virtual void ObserveDialog(
      WatchType watch_type,
      std::optional<std::string> sink_name = std::nullopt) = 0;

  const raw_ptr<content::WebContents> web_contents_;
  const raw_ptr<MediaRouterDialogControllerViews> dialog_controller_;
  std::optional<std::string> watch_sink_name_;
  WatchType watch_type_ = WatchType::kNone;
  std::optional<base::OnceClosure> watch_callback_;
  base::test::ScopedFeatureList feature_list_;
  bool torn_down_ = false;

  base::WeakPtrFactory<MediaRouterUiForTestBase> weak_factory_{this};
};

}  // namespace media_router

#endif  // CHROME_TEST_MEDIA_ROUTER_MEDIA_ROUTER_UI_FOR_TEST_BASE_H_
