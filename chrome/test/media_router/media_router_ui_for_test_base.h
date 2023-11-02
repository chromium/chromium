// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_MEDIA_ROUTER_MEDIA_ROUTER_UI_FOR_TEST_BASE_H_
#define CHROME_TEST_MEDIA_ROUTER_MEDIA_ROUTER_UI_FOR_TEST_BASE_H_

#include "base/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/test/scoped_feature_list.h"
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

class MediaRouterUiForTestBase {
 public:
  virtual void SetUp() = 0;

  // Cleans up after a test.
  void TearDown();

  virtual void ShowDialog() = 0;
  virtual bool IsDialogShown() const = 0;
  virtual void HideDialog() = 0;

  // Chooses the source type in the dialog. Requires that the dialog is shown.
  virtual void ChooseSourceType(CastDialogView::SourceType source_type) = 0;
  virtual CastDialogView::SourceType GetChosenSourceType() const = 0;

  // These methods require that the dialog is shown and the specified sink is
  // shown in the dialog.
  void StartCasting(const std::string& sink_name);
  void StopCasting(const std::string& sink_name);

  // Waits until a condition is met. Requires that the dialog is shown.
  virtual void WaitForSink(const std::string& sink_name) = 0;
  virtual void WaitForSinkAvailable(const std::string& sink_name) = 0;
  virtual void WaitForAnyIssue() = 0;
  virtual void WaitForAnyRoute() = 0;
  virtual void WaitForDialogShown() = 0;
  virtual void WaitForDialogHidden() = 0;

  // These methods require that the dialog is shown, and the sink specified by
  // |sink_name| is in the dialog.
  MediaRoute::Id GetRouteIdForSink(const std::string& sink_name) const;
  std::string GetStatusTextForSink(const std::string& sink_name) const;
  std::string GetIssueTextForSink(const std::string& sink_name) const;

  // Called by MediaRouterDialogControllerViews.
  virtual void OnDialogCreated();

  content::WebContents* web_contents() const { return web_contents_; }

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
  virtual ~MediaRouterUiForTestBase();
  void WaitForAnyDialogShown();

  void StartCasting(CastDialogSinkButton* sink_button);
  void StopCasting(CastDialogSinkButton* sink_button);

  static CastDialogSinkButton* GetSinkButtonWithName(
      const std::vector<CastDialogSinkButton*>& sink_buttons,
      const std::string& sink_name);

  virtual CastDialogSinkButton* GetSinkButton(
      const std::string& sink_name) const = 0;

  // Registers itself as an observer to the dialog, and waits until an event
  // of |watch_type| is observed. |sink_name| should be set only if observing
  // for a sink.
  virtual void ObserveDialog(
      WatchType watch_type,
      absl::optional<std::string> sink_name = absl::nullopt) = 0;

  const raw_ptr<content::WebContents> web_contents_;
  const raw_ptr<MediaRouterDialogControllerViews> dialog_controller_;
  absl::optional<std::string> watch_sink_name_;
  WatchType watch_type_ = WatchType::kNone;
  absl::optional<base::OnceClosure> watch_callback_;
  base::test::ScopedFeatureList feature_list_;
  base::WeakPtrFactory<MediaRouterUiForTestBase> weak_factory_{this};
};

}  // namespace media_router

#endif  // CHROME_TEST_MEDIA_ROUTER_MEDIA_ROUTER_UI_FOR_TEST_BASE_H_
