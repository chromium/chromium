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

  // Checks if a new bubble should currently be suppressed due to a recent
  // closure on blur. Note that this also returns true if the bubble is
  // currently showing.
  bool ShouldSuppress() const;

  // views::WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override;

  void CloseForTesting();

  // Uses views::kMinimumTimeBetweenButtonClicks by default.
  void SetSuppressionThresholdForTesting(base::TimeDelta threshold);

 private:
  std::optional<base::TimeTicks> last_close_time_;
  std::optional<base::TimeDelta> suppression_threshold_;
  base::ScopedObservation<views::Widget, views::WidgetObserver> observation_{
      this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_BUBBLE_WEBUI_BUBBLE_REOPEN_SUPPRESSOR_H_
