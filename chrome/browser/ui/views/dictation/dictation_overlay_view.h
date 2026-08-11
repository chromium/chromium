// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_DICTATION_DICTATION_OVERLAY_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_DICTATION_DICTATION_OVERLAY_VIEW_H_

#include <memory>

#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/views/dictation/ui_state.h"
#include "content/public/browser/global_dom_node_id.h"
#include "content/public/browser/weak_document_ptr.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/gfx/native_ui_types.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

namespace content {
class RenderFrameHost;
class RenderWidgetHostView;
}  // namespace content

namespace gfx {
class Point;
}  // namespace gfx

namespace views {
class Widget;
}  // namespace views

namespace dictation {

// Small overlay view containing a rounded button that follows the caret during
// an active dictation stream.
class DictationOverlayView : public views::BubbleDialogDelegate {
 public:
  DictationOverlayView(gfx::NativeView parent_window,
                       base::RepeatingClosure toggle_active_stream_callback);
  ~DictationOverlayView() override;

  DictationOverlayView(const DictationOverlayView&) = delete;
  DictationOverlayView& operator=(const DictationOverlayView&) = delete;

  void Show();
  void UpdatePosition(const gfx::Point& focus_selection_point);
  void OnStartedStream(content::GlobalDOMNodeId target_id);

  void SetState(UiState state);
  void UpdateAudioLevel(float audio_level);

  UiState state_for_testing() const;

  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kViewElementIdForTesting);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kMicButtonElementIdForTesting);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kWaveformElementIdForTesting);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kFinalizingImageElementIdForTesting);

 private:
  void OnFocusSelectionBoundsChanged(
      content::RenderWidgetHostView* render_widget_host_view);
  void UpdatePosition(content::RenderFrameHost* target_rfh);

  base::CallbackListSubscription focus_selection_bounds_changed_subscription_;
  content::WeakDocumentPtr last_target_document_;

  std::unique_ptr<views::Widget> widget_;
  UiState state_ = UiState::kInactive;
};

}  // namespace dictation

#endif  // CHROME_BROWSER_UI_VIEWS_DICTATION_DICTATION_OVERLAY_VIEW_H_
