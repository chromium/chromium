// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/picture_in_picture/document_pip_widget_delegate.h"

#include <utility>

#include "chrome/browser/ui/views/picture_in_picture/document_pip_contents_view.h"
#include "chrome/browser/ui/views/picture_in_picture/document_pip_frame_view.h"
#include "content/public/browser/web_contents.h"
#include "ui/views/view_utils.h"

DocumentPipWidgetDelegate::DocumentPipWidgetDelegate(
    Profile* profile,
    std::unique_ptr<content::WebContents> child_web_contents) {
  SetCanResize(true);
  SetCanMaximize(false);
  SetCanMinimize(false);
  SetCanFullscreen(false);

  // On Aura (Linux/Windows/ChromeOS), ensure ChromeViewsDelegate creates a
  // DesktopNativeWidgetAura for this top-level widget without needing
  // params.context.
  set_use_desktop_widget_override(true);

  SetContentsView(std::make_unique<DocumentPipContentsView>(
      profile, std::move(child_web_contents)));
}

DocumentPipWidgetDelegate::~DocumentPipWidgetDelegate() = default;

DocumentPipContentsView*
DocumentPipWidgetDelegate::GetDocumentPipContentsView() {
  return views::AsViewClass<DocumentPipContentsView>(GetContentsView());
}

std::unique_ptr<views::FrameView> DocumentPipWidgetDelegate::CreateFrameView(
    views::Widget* widget) {
  return std::make_unique<DocumentPipFrameView>(widget);
}
