// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_LENS_LENS_OVERLAY_CONTROLLER_H_
#define CHROME_BROWSER_UI_LENS_LENS_OVERLAY_CONTROLLER_H_

#include "base/memory/raw_ptr.h"

#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/lens/core/mojom/lens.mojom.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/views/widget/unique_widget_ptr.h"

class TabStripModel;

namespace tabs {
class TabModel;
}  // namespace tabs

namespace views {
class View;
}  // namespace views

namespace content {
class WebUI;
}  // namespace content

// Manages all state associated with the lens overlay.
// This class is not thread safe. It should only be used from the browser
// thread.
class LensOverlayController : public TabStripModelObserver,
                              public lens::mojom::LensPageHandler {
 public:
  explicit LensOverlayController(tabs::TabModel* tab_model);
  ~LensOverlayController() override;

  // This is entry point for showing the overlay UI. This has no effect if state
  // is not kOff. This has no effect if the tab is not in the foreground.
  void ShowUI();

  // Closes the overlay UI and sets state to kOff.
  void CloseUI();

  // This method is used to set up communication between this instance and the
  // overlay WebUI. This is called by the WebUIController when the WebUI is
  // executing javascript and ready to bind.
  static void BindOverlay(
      content::WebUI* web_ui,
      mojo::PendingReceiver<lens::mojom::LensPageHandler> receiver,
      mojo::PendingRemote<lens::mojom::LensPage> page);

  // Internal state machine. States are mutually exclusive. Exposed for testing.
  enum class State {
    // This is the default state. There should be no performance overhead as
    // this state will apply to all tabs.
    kOff,

    // In the process of taking a screenshot to transition to kOverlay.
    kScreenshot,

    // In the process of starting the overlay WebUI.
    kStartingWebUI,

    // Showing an overlay without results.
    kOverlay,

    // Showing an overlay with results.
    kOverlayAndResults,
  };
  State state() { return state_; }

  // Returns the screenshot currently being displayed on this overlay. If no
  // screenshot is showing, will return nullptr.
  const SkBitmap& current_screenshot() { return current_screenshot_; }

  // Testing helper method for checking widget.
  raw_ptr<views::Widget> GetOverlayWidgetForTesting();

 private:
  // Called once a screenshot has been captured. This should trigger transition
  // to kOverlay. As this process is asynchronous, there are edge cases that can
  // result in multiple in-flight screenshot attempts. We record the
  // `attempt_id` for each attempt so we can ignore all but the most recent
  // attempt.
  void DidCaptureScreenshot(int attempt_id, const SkBitmap& bitmap);

  // Called when the UI needs to create the overlay widget.
  void ShowOverlayWidget();

  // Creates InitParams for the overlay widget based on the window bounds.
  views::Widget::InitParams CreateWidgetInitParams();

  // Called when the UI needs to create the view to show in the overlay.
  std::unique_ptr<views::View> CreateViewForOverlay();

  // Overridden from TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;

  // Called when the associated tab enters the foreground.
  void TabForegrounded();

  // Called when the associated tab enters the background.
  void TabBackgrounded();

  // See BindOverlay(), this is the instance-specific version.
  void BindOverlay(mojo::PendingReceiver<lens::mojom::LensPageHandler> receiver,
                   mojo::PendingRemote<lens::mojom::LensPage> page);

  // lens::mojom::LensPageHandler overrides.
  void CloseRequestedByOverlay() override;

  // Owns this class.
  raw_ptr<tabs::TabModel> tab_model_;

  // A monotonically increasing id. This is used to differentiate between
  // different screenshot attempts.
  int screenshot_attempt_id_ = 0;

  // Tracks the internal state machine.
  State state_ = State::kOff;

  // Pointer to the overlay widget.
  views::UniqueWidgetPtr overlay_widget_;

  // Pointer to the WebContents that is hosting the overlay WebUI. Only valid
  // while `overlay_widget_` is showing.
  raw_ptr<content::WebContents> overlay_web_contents_;

  // The screenshot that is currently being rendered by the WebUI.
  SkBitmap current_screenshot_;

  // Connections to and from the overlay WebUI. Only valid while
  // `overlay_widget_` is showing, and after the WebUI has started executing JS
  // and has bound the connection.
  mojo::Receiver<lens::mojom::LensPageHandler> receiver_{this};
  mojo::Remote<lens::mojom::LensPage> page_;

  // Must be the last member.
  base::WeakPtrFactory<LensOverlayController> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_LENS_LENS_OVERLAY_CONTROLLER_H_
