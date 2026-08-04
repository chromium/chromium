// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_BUBBLE_WEBUI_BUBBLE_REOPEN_SUPPRESSOR_H_
#define CHROME_BROWSER_UI_VIEWS_BUBBLE_WEBUI_BUBBLE_REOPEN_SUPPRESSOR_H_

#include <optional>

#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

// A helper class to prevent WebUI buttons from repeatedly reopening bubbles
// that were just closed due to losing focus.
//
// WebUI buttons suffer from a race condition where a native bubble closes
// synchronously on blur, but the mouse click arriving from the WebUI is
// handled asynchronously. By the time the WebUI click reaches C++, the original
// bubble has finished closing and acts as if it wasn't open, leading to an
// unintentional reopen.
//
// Attach this to the bubble's Widget by calling `Observe(widget)` when
// spawning the bubble.
// Check `ShouldSuppress()` in your button's click handler before opening a new
// bubble.
class WebUIBubbleReopenSuppressor : public views::WidgetObserver {
 public:
  WebUIBubbleReopenSuppressor();
  WebUIBubbleReopenSuppressor(const WebUIBubbleReopenSuppressor&) = delete;
  WebUIBubbleReopenSuppressor& operator=(const WebUIBubbleReopenSuppressor&) =
      delete;
  ~WebUIBubbleReopenSuppressor() override;

  // Observe the given widget for closure. Call this when spawning the bubble.
  void Observe(views::Widget* widget);

  // Returns true if the bubble is currently open.
  bool IsShowing() const;

  // Returns the widget being observed, or null if it's not showing.
  views::Widget* GetWidget();

  // Closes the current bubble if it is showing.
  void Close(views::Widget::ClosedReason reason =
                 views::Widget::ClosedReason::kUnspecified);

  // Called when a mouse press occurs over the invoking UI element. Calling this
  // primes the suppressor so that the associated asynchronous mouse click will
  // not inadvertently reopen a bubble that was just closed.
  // `extra_suppress_condition`: If true, the subsequent mouse click will be
  // forcefully suppressed regardless of the bubble's timing state.
  void OnMousePressed(bool extra_suppress_condition = false);

  // Uses the internal state locked in during `OnPointerDown()` to
  // definitively check whether the bubble show attempt should be suppressed
  // (returning true) or permitted (returning false). Call this within your UI
  // element's activation (click) handler. Evaluating this resets the internal
  // state.
  // `is_pointer_interaction` differentiates between pointer and keyboard
  // clicks, as keyboard interactions do not suffer from focus-loss race
  // conditions and should rarely be suppressed.
  // TODO(crbug.com/532609175): Native views implementation only considers
  // mouse, but WebUI considers mouse/touch/pen.
  bool ShouldSuppressBubbleShow(bool is_pointer_interaction);

  // Returns true if the time elapsed since the widget was closed is less than
  // the suppression threshold, or if the widget is currently showing. Use this
  // directly ONLY if you lack the native interception capabilities to use
  // `OnMousePressed()` / `ShouldSuppressBubbleShow()`.
  // TODO(crbug.com/532609175): Rename this and change it to private. UI
  // elements' click handlers should call ShouldSuppressBubbleShow instead.
  bool ShouldSuppress() const;

  // views::WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override;

  void CloseForTesting();

  // Uses views::kMinimumTimeBetweenButtonClicks by default.
  void SetSuppressionThresholdForTesting(base::TimeDelta threshold);

 private:
  // Flag armed by `OnMousePressed()` indicating that the subsequent bubble show
  // attempt triggered by the same physical click should be suppressed.
  bool suppress_next_bubble_show_ = false;

  // The timestamp when the associated bubble widget was last closed.
  std::optional<base::TimeTicks> last_close_time_;

  // The maximum time elapsed since the bubble closed where a new show attempt
  // will be suppressed. This duration covers the window where asynchronous
  // WebUI clicks continue arriving after the bubble has already lost focus and
  // closed.
  base::TimeDelta suppression_threshold_;
  base::ScopedObservation<views::Widget, views::WidgetObserver> observation_{
      this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_BUBBLE_WEBUI_BUBBLE_REOPEN_SUPPRESSOR_H_
