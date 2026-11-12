// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/omnibox_everywhere/omnibox_everywhere_file_selector.h"

#include <utility>

#include "build/build_config.h"
#include "content/public/browser/web_contents.h"

#if BUILDFLAG(IS_MAC)
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "ui/gfx/geometry/rect.h"

namespace {

// Fallback timeout when waiting on macOS for the WebUI to collapse its
// dropdown (`is-pending-context-upload`) and report updated widget bounds via
// `ResizeDueToAutoResize()` -> `OnWidgetBoundsChanged()`. If the widget was
// already at its collapsed height (so no bounds change occurs) or the renderer
// is busy, this bounds the wait before presenting `NSOpenPanel`.
constexpr base::TimeDelta kWidgetResizeTimeout = base::Milliseconds(150);

}  // namespace
#endif  // BUILDFLAG(IS_MAC)

OmniboxEverywhereFileSelector::OmniboxEverywhereFileSelector(
    gfx::NativeWindow owning_window,
    base::RepeatingClosure opened_callback,
    base::RepeatingClosure closed_callback,
    base::RepeatingClosure canceled_callback)
    : OmniboxPopupFileSelector(owning_window),
      opened_callback_(std::move(opened_callback)),
      closed_callback_(std::move(closed_callback)),
      canceled_callback_(std::move(canceled_callback))
#if BUILDFLAG(IS_MAC)
      ,
      owning_window_(owning_window)
#endif
{
  set_file_chooser_closed_callback(closed_callback_);
}

OmniboxEverywhereFileSelector::~OmniboxEverywhereFileSelector() = default;

void OmniboxEverywhereFileSelector::OpenFileUploadDialog(
    content::WebContents* web_contents,
    bool is_image,
    OmniboxEditModel* edit_model,
    std::optional<lens::ImageEncodingOptions> image_encoding_options,
    bool was_ai_mode_open) {
#if BUILDFLAG(IS_MAC)
  ResetPendingShow();
#endif

  // Notify `OmniboxEverywhereUI::OnFileChooserOpened()` immediately so the
  // WebUI transitions to the collapsed Composebox state
  // (`is_pending_context_upload = true`) before the native file chooser opens.
  if (opened_callback_) {
    opened_callback_.Run();
  }

#if BUILDFLAG(IS_MAC)
  // On macOS, `SelectFileDialog` attaches `NSOpenPanel` as a window-modal sheet
  // directly to the Loomnibox `NSWindow`. Defer presenting the sheet until
  // `OnWidgetBoundsChanged()` observes the post-collapse height (or
  // `kWidgetResizeTimeout` expires if the widget was already collapsed) so the
  // sheet does not jump mid-animation.
  pending_show_callback_ = base::BindOnce(
      &OmniboxEverywhereFileSelector::ShowFileUploadDialog,
      weak_ptr_factory_.GetWeakPtr(),
      web_contents ? web_contents->GetWeakPtr() : nullptr,
      web_contents != nullptr, is_image, std::move(image_encoding_options),
      was_ai_mode_open);

  gfx::NativeWindow top_window =
      web_contents && web_contents->GetTopLevelNativeWindow()
          ? web_contents->GetTopLevelNativeWindow()
          : owning_window_;
  views::Widget* widget =
      top_window ? views::Widget::GetWidgetForNativeWindow(top_window)
                 : nullptr;
  if (!widget) {
    RunPendingShowCallback();
    return;
  }

  initial_widget_height_ = widget->GetWindowBoundsInScreen().height();
  widget_observation_.Observe(widget);
  show_timer_.Start(FROM_HERE, kWidgetResizeTimeout, this,
                    &OmniboxEverywhereFileSelector::RunPendingShowCallback);
#else
  OmniboxPopupFileSelector::OpenFileUploadDialog(
      web_contents, is_image, edit_model, std::move(image_encoding_options),
      was_ai_mode_open);
#endif
}

void OmniboxEverywhereFileSelector::FileSelectionCanceled() {
#if BUILDFLAG(IS_MAC)
  ResetPendingShow();
#endif
  OmniboxPopupFileSelector::FileSelectionCanceled();
  if (canceled_callback_) {
    canceled_callback_.Run();
  }
}

void OmniboxEverywhereFileSelector::Cancel() {
#if BUILDFLAG(IS_MAC)
  const bool had_pending_show = !pending_show_callback_.is_null();
  ResetPendingShow();
#else
  const bool had_pending_show = false;
#endif
  const bool had_open_dialog = IsFileDialogOpen();
  CancelFileSelection();
  if (had_pending_show && closed_callback_) {
    closed_callback_.Run();
  }
  if ((had_pending_show || had_open_dialog) && canceled_callback_) {
    canceled_callback_.Run();
  }
}

#if BUILDFLAG(IS_MAC)
void OmniboxEverywhereFileSelector::OnWidgetBoundsChanged(
    views::Widget* widget,
    const gfx::Rect& new_bounds) {
  if (new_bounds.height() != initial_widget_height_) {
    RunPendingShowCallback();
  }
}

void OmniboxEverywhereFileSelector::OnWidgetDestroying(views::Widget* widget) {
  ResetPendingShow();
}

void OmniboxEverywhereFileSelector::ShowFileUploadDialog(
    base::WeakPtr<content::WebContents> weak_web_contents,
    bool has_web_contents,
    bool is_image,
    std::optional<lens::ImageEncodingOptions> image_encoding_options,
    bool was_ai_mode_open) {
  if (has_web_contents && !weak_web_contents) {
    return;
  }
  OmniboxPopupFileSelector::OpenFileUploadDialog(
      weak_web_contents.get(), is_image, /*edit_model=*/nullptr,
      std::move(image_encoding_options), was_ai_mode_open);
}

void OmniboxEverywhereFileSelector::RunPendingShowCallback() {
  base::OnceClosure callback = std::move(pending_show_callback_);
  ResetPendingShow();
  if (callback) {
    std::move(callback).Run();
  }
}

void OmniboxEverywhereFileSelector::ResetPendingShow() {
  show_timer_.Stop();
  widget_observation_.Reset();
  pending_show_callback_.Reset();
}
#endif  // BUILDFLAG(IS_MAC)
