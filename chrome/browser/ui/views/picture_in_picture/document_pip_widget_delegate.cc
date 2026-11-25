// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/picture_in_picture/document_pip_widget_delegate.h"

#include <utility>

#include "base/check_deref.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "chrome/browser/ui/profiles/profile_view_utils.h"
#include "chrome/browser/ui/views/picture_in_picture/document_pip_contents_view.h"
#include "chrome/browser/ui/views/picture_in_picture/document_pip_frame_view.h"
#include "chrome/browser/ui/views/picture_in_picture/document_pip_host.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/view_utils.h"

#if BUILDFLAG(IS_LINUX)
#include "chrome/browser/ui/views/picture_in_picture/document_pip_frame_view_linux.h"
#include "ui/aura/window_tree_host_platform.h"
#include "ui/platform_window/platform_window.h"  // nogncheck
#endif

DocumentPipWidgetDelegate::DocumentPipWidgetDelegate(
    DocumentPipHost* host,
    std::unique_ptr<content::WebContents> child_web_contents)
    : host_(CHECK_DEREF(host)) {
  SetCanResize(true);
  SetCanMaximize(false);
  SetCanMinimize(false);
  SetCanFullscreen(false);

  // On Aura (Linux/Windows/ChromeOS), ensure ChromeViewsDelegate creates a
  // DesktopNativeWidgetAura for this top-level widget without needing
  // params.context.
  set_use_desktop_widget_override(true);

  SetContentsView(std::make_unique<DocumentPipContentsView>(
      host_->GetProfile(), std::move(child_web_contents)));
}

DocumentPipWidgetDelegate::~DocumentPipWidgetDelegate() = default;

DocumentPipContentsView*
DocumentPipWidgetDelegate::GetDocumentPipContentsView() {
  return views::AsViewClass<DocumentPipContentsView>(GetContentsView());
}

std::u16string DocumentPipWidgetDelegate::GetWindowTitle() const {
  content::WebContents* opener = host_->GetOpenerWebContents();
  // The opener observer is detached before `host_` is destroyed.
  if (!opener) {
    return std::u16string();
  }
  std::u16string title = opener->GetTitle();
  // Match WindowMetadataController::FormatTitleForDisplay without depending on
  // the Browser-backed window metadata controller.
  base::RemoveChars(title, u"\n", &title);
  return title;
}

std::u16string DocumentPipWidgetDelegate::GetAccessibleWindowTitle() const {
  if (!host_->GetOpenerWebContents()) {
    return std::u16string();
  }

  // Match BrowserView's profile/session annotations without changing the
  // visible window title.
  std::u16string title = GetWindowTitle();
  if (title.empty()) {
    // Browser-backed Document PiP falls back to the default tab title.
    title = l10n_util::GetStringUTF16(IDS_DEFAULT_TAB_TITLE);
  }
  return AnnotateAccessibleWindowTitleForProfile(title, *host_->GetProfile());
}

std::unique_ptr<views::FrameView> DocumentPipWidgetDelegate::CreateFrameView(
    views::Widget* widget) {
#if BUILDFLAG(IS_LINUX)
  auto* platform_window =
      aura::WindowTreeHostPlatform::GetHostForWindow(widget->GetNativeWindow())
          ->platform_window();
  return std::make_unique<DocumentPipFrameViewLinux>(
      &host_.get(), platform_window->CanSetDecorationInsets() &&
                        views::Widget::IsWindowCompositingSupported());
#else
  return std::make_unique<DocumentPipFrameView>(&host_.get());
#endif
}
