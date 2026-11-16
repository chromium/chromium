// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OMNIBOX_EVERYWHERE_OMNIBOX_EVERYWHERE_FILE_SELECTOR_H_
#define CHROME_BROWSER_UI_WEBUI_OMNIBOX_EVERYWHERE_OMNIBOX_EVERYWHERE_FILE_SELECTOR_H_

#include <optional>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "chrome/browser/ui/views/location_bar/omnibox_popup_file_selector.h"
#include "components/lens/lens_bitmap_processing.h"
#include "ui/gfx/native_ui_types.h"

#if BUILDFLAG(IS_MAC)
#include "base/scoped_observation.h"
#include "base/timer/timer.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

namespace gfx {
class Rect;
}  // namespace gfx
#endif  // BUILDFLAG(IS_MAC)

namespace content {
class WebContents;
}  // namespace content

class OmniboxEditModel;

// On macOS, defers presenting the native file chooser until the Loomnibox
// widget has finished collapsing to its Composebox height (`OnWidgetBoundsChanged`)
// so AppKit does not attach the `NSOpenPanel` sheet to the expanded dropdown
// bounds and jump mid-animation when `ResizeDueToAutoResize` shrinks the
// window.
class OmniboxEverywhereFileSelector : public OmniboxPopupFileSelector
#if BUILDFLAG(IS_MAC)
    ,
                                      public views::WidgetObserver
#endif
{
 public:
  OmniboxEverywhereFileSelector(gfx::NativeWindow owning_window,
                                base::RepeatingClosure opened_callback,
                                base::RepeatingClosure closed_callback,
                                base::RepeatingClosure canceled_callback);
  OmniboxEverywhereFileSelector(const OmniboxEverywhereFileSelector&) = delete;
  OmniboxEverywhereFileSelector& operator=(
      const OmniboxEverywhereFileSelector&) = delete;
  ~OmniboxEverywhereFileSelector() override;

  // OmniboxPopupFileSelector:
  void OpenFileUploadDialog(
      content::WebContents* web_contents,
      bool is_image,
      OmniboxEditModel* edit_model,
      std::optional<lens::ImageEncodingOptions> image_encoding_options,
      bool was_ai_mode_open) override;
  void FileSelectionCanceled() override;

  // Cancels any pending or open file chooser dialog and runs the closed and
  // canceled callbacks.
  void Cancel();

#if BUILDFLAG(IS_MAC)
  // views::WidgetObserver:
  void OnWidgetBoundsChanged(views::Widget* widget,
                             const gfx::Rect& new_bounds) override;
  void OnWidgetDestroying(views::Widget* widget) override;
#endif

 private:
#if BUILDFLAG(IS_MAC)
  void ShowFileUploadDialog(
      base::WeakPtr<content::WebContents> weak_web_contents,
      bool has_web_contents,
      bool is_image,
      std::optional<lens::ImageEncodingOptions> image_encoding_options,
      bool was_ai_mode_open);
  void RunPendingShowCallback();
  void ResetPendingShow();
#endif

  base::RepeatingClosure opened_callback_;
  base::RepeatingClosure closed_callback_;
  base::RepeatingClosure canceled_callback_;
#if BUILDFLAG(IS_MAC)
  gfx::NativeWindow owning_window_;
  int initial_widget_height_ = 0;
  base::OnceClosure pending_show_callback_;
  base::OneShotTimer show_timer_;
  base::ScopedObservation<views::Widget, views::WidgetObserver>
      widget_observation_{this};
  base::WeakPtrFactory<OmniboxEverywhereFileSelector> weak_ptr_factory_{this};
#endif
};

#endif  // CHROME_BROWSER_UI_WEBUI_OMNIBOX_EVERYWHERE_OMNIBOX_EVERYWHERE_FILE_SELECTOR_H_
