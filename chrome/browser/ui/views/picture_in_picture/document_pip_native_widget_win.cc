// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/picture_in_picture/document_pip_native_widget_win.h"

#include <windows.h>

#include "chrome/browser/ui/ui_features.h"
#include "ui/views/widget/desktop_aura/desktop_native_widget_aura.h"
#include "ui/views/widget/desktop_aura/desktop_window_tree_host_win.h"

namespace {

class DocumentPipDesktopWindowTreeHostWin
    : public views::DesktopWindowTreeHostWin {
 public:
  DocumentPipDesktopWindowTreeHostWin(
      views::Widget* widget,
      views::DesktopNativeWidgetAura* native_widget)
      : views::DesktopWindowTreeHostWin(widget, native_widget) {}
  DocumentPipDesktopWindowTreeHostWin(
      const DocumentPipDesktopWindowTreeHostWin&) = delete;
  DocumentPipDesktopWindowTreeHostWin& operator=(
      const DocumentPipDesktopWindowTreeHostWin&) = delete;
  ~DocumentPipDesktopWindowTreeHostWin() override = default;

 protected:
  // views::DesktopWindowTreeHostWin:
  bool UsesNativeSystemMenu() const override {
    return !features::IsMenuSimplificationEnabled();
  }

  bool PreHandleMSG(UINT message,
                    WPARAM w_param,
                    LPARAM l_param,
                    LRESULT* result) override {
    if (!UsesNativeSystemMenu() &&
        ((message == WM_SYSCOMMAND && (w_param & 0xFFF0) == SC_KEYMENU &&
          l_param == ' ') ||
         (message == WM_SYSCHAR && w_param == VK_SPACE))) {
      *result = 0;
      return true;
    }
    return views::DesktopWindowTreeHostWin::PreHandleMSG(message, w_param,
                                                         l_param, result);
  }
};

}  // namespace

std::unique_ptr<views::Widget> CreateDocumentPipWidgetWin(
    views::Widget::InitParams& params) {
  auto widget = std::make_unique<views::Widget>();
  auto* native_widget = new views::DesktopNativeWidgetAura(widget.get());
  params.native_widget = native_widget;
  params.desktop_window_tree_host =
      new DocumentPipDesktopWindowTreeHostWin(widget.get(), native_widget);
  return widget;
}
