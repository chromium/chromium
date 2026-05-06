// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_POPUP_PRESENTER_BASE_H_
#define CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_POPUP_PRESENTER_BASE_H_

#include <memory>
#include <optional>
#include <string_view>

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/ui/webui/searchbox/webui_omnibox_handler.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/view.h"
#include "ui/views/view_observer.h"
#include "ui/views/widget/widget.h"

class LocationBar;
class OmniboxPopupWebUIBaseContent;
class OmniboxPopupPresenterDelegate;
class RoundedOmniboxResultsFrame;
class OmniboxController;

namespace content {
class WebContents;
}  // namespace content

namespace omnibox {
extern const void* kOmniboxWebUIPopupWidgetId;
}  // namespace omnibox

// A base assistant class for OmniboxPopupViewWebUI, this manages "n" WebViews
// and a Widget to present the WebUI. This class is an implementation detail and
// is not expected to grow or change much with omnibox changes.  The concern of
// this class is presentation only, i.e. Views and Widgets.  For omnibox logic
// concerns and communication between native omnibox code and the WebUI code,
// work with OmniboxPopupViewWebUI directly.
class OmniboxPopupPresenterBase {
 public:
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kRoundedResultsFrame);
  // Arguments must outlast this.
  explicit OmniboxPopupPresenterBase(
      LocationBar* location_bar,
      OmniboxPopupPresenterDelegate& presenter_delegate,
      OmniboxController* controller);
  OmniboxPopupPresenterBase(const OmniboxPopupPresenterBase&) = delete;
  OmniboxPopupPresenterBase& operator=(const OmniboxPopupPresenterBase&) =
      delete;
  virtual ~OmniboxPopupPresenterBase();

  // Show or hide the popup widget with web view.
  virtual void Show();
  virtual void Hide();

  // Tells whether the popup widget exists.
  bool IsShown() const;

  // Caches the height of the WebUI content, which is then used to compute the
  // popup widget bounds.
  void OnContentHeightChanged(int content_height);

  // Returns the currently "active" Popup content, whichever one is visible or
  // going to be visible within the popup.
  OmniboxPopupWebUIBaseContent* GetWebUIContent() const;

  // Override to enable deferred showing until the WebUI has painted a new
  // frame.
  virtual bool ShouldDeferUntilVisualStateReady() const;

  virtual std::string_view GetPopupMetricPrefix() const = 0;

  OmniboxPopupPresenterDelegate& delegate() const {
    return *presenter_delegate_;
  }

 protected:
  inline static constexpr std::string_view kWebUIPopupMetricPrefix =
      "Omnibox.Popup.WebUI";
  inline static constexpr std::string_view kAimPopupMetricPrefix =
      "Omnibox.Popup.Aim";

  // The container for the WebUI WebView.
  views::View* GetUIContainer() const;

  // Sets the webview content reference.
  void SetWebUIContent(
      std::unique_ptr<OmniboxPopupWebUIBaseContent> webui_content);

  void EnsureWidgetCreated();

  // Called when the widget has just been destroyed.
  virtual void WidgetDestroyed() {}

  // Returns whether or not the popup should include the location bar cutout.
  virtual bool ShouldShowLocationBarCutout() const;

  // Returns whether the WebUI content view receive focus.
  virtual bool ShouldReceiveFocus() const;

  LocationBar* location_bar() const { return location_bar_.get(); }

  views::Widget* GetWidget() const { return widget_.get(); }

  OmniboxController* controller() const;

  // The height of the popup content. Can be 0 if not specified.
  int content_height_ = 0;

 private:
  friend class OmniboxPopupViewWebUITest;
  friend class OmniboxWebUiInteractiveTest;

  // Synchronize the popup widget's bounds to its anchor (location bar view).
  void SynchronizePopupBounds();

  void OnWidgetClosed(views::Widget::ClosedReason closed_reason);

  // Shows the popup widget immediately, called after stale frame fix deferral
  // if enabled.
  void ShowWidget(base::TimeTicks show_widget_time);

  // Callback for when the visual state is ready.
  void OnVisualStateReady(base::TimeTicks show_widget_time,
                          bool from_fallback,
                          bool success);

  // Callback for when the visual state is ready.
  // This is specifically for metrics logging and is distinct from the
  // OnVisualStateReady deferral callback.
  void OnVisualStateReadyForMetrics(base::TimeTicks result_ready_time,
                                    bool success);

  void LogResultToContentReadyMetric(content::WebContents* web_contents);

  // Remove observation and reset widget, optionally requesting it to close.
  void ReleaseWidget();

  // Returns the frame view of the widget if it exists. CHECKs if no widget
  // created
  RoundedOmniboxResultsFrame* GetResultsFrame() const;

  // The location bar that owns `this`.
  const raw_ptr<LocationBar> location_bar_;

  const raw_ref<OmniboxPopupPresenterDelegate> presenter_delegate_;

  // The container for both the WebUI suggestions list and other WebUI
  // containers
  std::unique_ptr<views::View> owned_omnibox_popup_webui_container_;

  // The WebUI content WebView. Owned by the container.
  raw_ptr<OmniboxPopupWebUIBaseContent> omnibox_popup_webui_content_ = nullptr;

  // The popup widget that contains this WebView. Created and closed by `this`;
  // owned and destroyed by the OS.
  std::unique_ptr<views::Widget> widget_;

  const raw_ptr<OmniboxController> controller_;

  // True if `ShowWidget()` execution is currently being deferred until the
  // WebUI has produced a new frame.
  bool is_deferred_ = false;

  // Whether the first content ready metric of the popup has been logged.
  bool has_logged_first_content_ready_ = false;

  // Whether the content ready metric has been logged since the popup was
  // opened.
  // This should be reset at the beginning of the Show() method.
  bool has_logged_content_ready_since_open_ = false;

  base::WeakPtrFactory<OmniboxPopupPresenterBase> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_POPUP_PRESENTER_BASE_H_
