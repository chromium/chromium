// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/constrained_window/native_web_contents_modal_dialog_manager_views_mac.h"

#import <Cocoa/Cocoa.h>

#include "components/constrained_window/constrained_window_views.h"
#include "components/guest_view/browser/guest_view_base.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/browser/web_contents.h"
#import "ui/gfx/mac/coordinate_conversion.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

using web_modal::WebContentsModalDialogManager;
using web_modal::SingleWebContentsDialogManager;

namespace {

// Sets visibility and mouse events for a Cocoa NSWindow* and an attached sheet.
void SetSheetVisible(gfx::NativeWindow native_window, bool visible) {
  NSWindow* overlay = native_window.GetNativeNSWindow();
  CGFloat alpha = visible ? 1.0 : 0.0;
  BOOL ignore_events = visible ? NO : YES;

  // Don't allow interaction with the tab underneath the overlay.
  [overlay setIgnoresMouseEvents:ignore_events];

  [[overlay attachedSheet] setAlphaValue:alpha];
  [[overlay attachedSheet] setIgnoresMouseEvents:ignore_events];
}

}  // namespace

namespace constrained_window {

NativeWebContentsModalDialogManagerViewsMac::
    NativeWebContentsModalDialogManagerViewsMac(
        gfx::NativeWindow dialog,
        web_modal::SingleWebContentsDialogManagerDelegate* native_delegate)
    : NativeWebContentsModalDialogManagerViews(dialog, native_delegate) {}

// NativeWebContentsModalDialogManagerViews:
void NativeWebContentsModalDialogManagerViewsMac::OnPositionRequiresUpdate() {
  NativeWebContentsModalDialogManagerViews::OnPositionRequiresUpdate();

  views::Widget* widget = GetWidget(dialog());
  // Because the animation of SFCertificatePanel will change depending on the
  // size of the parent, i.e. |widget|, make sure its size is the same as the
  // area under the Chrome UI. The origin of the dialog then also needs to be
  // updated to position the certificate viewer in the middle horizontally.
  content::WebContents* web_contents = native_delegate()->GetWebContents();
  // Note: Can't use WebContents container bounds here because it doesn't
  // include the DevTool panel width.
  CGFloat window_width = NSWidth(
      [web_contents->GetTopLevelNativeWindow().GetNativeNSWindow() frame]);
  gfx::Rect tab_view_size = web_contents->GetContainerBounds();
  widget->SetBounds(gfx::Rect(tab_view_size.x(),
                              widget->GetWindowBoundsInScreen().y(),
                              window_width, tab_view_size.height()));
}

void NativeWebContentsModalDialogManagerViewsMac::ShowWidget(
    views::Widget* widget) {
  NSWindow* dialog_window = widget->GetNativeWindow().GetNativeNSWindow();
  [dialog_window setAlphaValue:0.0];
  // Because |dialog_window| is transparent, it won't accept mouse events until
  // ignoresMouseEvents is set. NSWindows start off accepting mouse events only
  // in non-transparent areas - setting this explicitly will make the NSWindow
  // accept mouse events everywhere regardless of window transparency.
  [dialog_window setIgnoresMouseEvents:NO];

  // Detect whether this is the first call to open the dialog. If yes, do this
  // via the normal views method. If not, overlay and sheet are both already
  // opened, and should be invisible, so return the sheet to full opacity.
  if (![dialog_window attachedSheet]) {
    NativeWebContentsModalDialogManagerViews::ShowWidget(widget);
    // Make sure the dialog is sized correctly for the correct animations.
    OnPositionRequiresUpdate();
    return;
  }

  // Account for window resizes that happen while another tab is open.
  OnPositionRequiresUpdate();
  SetSheetVisible(dialog_window, true);
}

void NativeWebContentsModalDialogManagerViewsMac::HideWidget(
    views::Widget* widget) {
  NSWindow* dialog_window = widget->GetNativeWindow().GetNativeNSWindow();
  // Avoid views::Widget::Hide(), as a call to orderOut: on a NSWindow with an
  // attached sheet will close the sheet. Instead, just set the sheet to 0
  // opacity and don't accept click events.
  SetSheetVisible(dialog_window, false);
}

}  // namespace constrained_window
