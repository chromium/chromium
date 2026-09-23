// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PICTURE_IN_PICTURE_DOCUMENT_PIP_BASE_WINDOW_H_
#define CHROME_BROWSER_UI_VIEWS_PICTURE_IN_PICTURE_DOCUMENT_PIP_BASE_WINDOW_H_

#include "base/memory/raw_ref.h"
#include "ui/base/base_window.h"

namespace views {
class Widget;
}

// The host must destroy this adapter before destroying its Widget.
class DocumentPipBaseWindow final : public ui::BaseWindow {
 public:
  explicit DocumentPipBaseWindow(views::Widget& widget);
  DocumentPipBaseWindow(const DocumentPipBaseWindow&) = delete;
  DocumentPipBaseWindow& operator=(const DocumentPipBaseWindow&) = delete;
  ~DocumentPipBaseWindow();

  // ui::BaseWindow:
  bool IsActive() const override;
  bool IsMaximized() const override;
  bool IsMinimized() const override;
  bool IsFullscreen() const override;
  gfx::NativeWindow GetNativeWindow() const override;
  gfx::Rect GetRestoredBounds() const override;
  ui::mojom::WindowShowState GetRestoredState() const override;
  gfx::Rect GetBounds() const override;
  void Show() override;
  void Hide() override;
  bool IsVisible() const override;
  void ShowInactive() override;
  void Close() override;
  void Activate() override;
  void Deactivate() override;
  void Maximize() override;
  void Minimize() override;
  void Restore() override;
  void SetBounds(const gfx::Rect& bounds) override;
  void FlashFrame(bool flash) override;
  ui::ZOrderLevel GetZOrderLevel() const override;
  void SetZOrderLevel(ui::ZOrderLevel level) override;

 private:
  const raw_ref<views::Widget> widget_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PICTURE_IN_PICTURE_DOCUMENT_PIP_BASE_WINDOW_H_
