// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PICTURE_IN_PICTURE_DOCUMENT_PIP_FRAME_VIEW_LINUX_H_
#define CHROME_BROWSER_UI_VIEWS_PICTURE_IN_PICTURE_DOCUMENT_PIP_FRAME_VIEW_LINUX_H_

#include "base/callback_list.h"
#include "base/memory/raw_ref.h"
#include "chrome/browser/ui/views/picture_in_picture/document_pip_frame_view.h"
#include "ui/gfx/shadow_value.h"

class Profile;
class SkRRect;

namespace ui {
class WindowFrameProvider;
}  // namespace ui

namespace views {
class FrameBackground;
}  // namespace views

// Matches the client-side decorations of PictureInPictureBrowserFrameViewLinux
// without depending on a Browser or BrowserWidget.
class DocumentPipFrameViewLinux : public DocumentPipFrameView {
  METADATA_HEADER(DocumentPipFrameViewLinux, DocumentPipFrameView)

 public:
  DocumentPipFrameViewLinux(DocumentPipHost* host,
                            bool supports_client_frame_shadow);
  DocumentPipFrameViewLinux(const DocumentPipFrameViewLinux&) = delete;
  DocumentPipFrameViewLinux& operator=(const DocumentPipFrameViewLinux&) =
      delete;
  ~DocumentPipFrameViewLinux() override;

  // views::FrameView:
  gfx::Rect GetNonDecoratedClientAreaBoundsInScreen() const override;

  // views::View:
  void OnPaint(gfx::Canvas* canvas) override;
  void OnThemeChanged() override;

  gfx::Insets RestoredMirroredFrameBorderInsets() const;
  gfx::Insets GetInputInsets() const;
  SkRRect GetRestoredClipRegion() const;
  void SetTiled(bool tiled);

 protected:
  // DocumentPipFrameView:
  gfx::Insets FrameBorderInsets() const override;
  gfx::Insets ResizeBorderInsets() const override;

 private:
  static gfx::ShadowValues GetShadowValues();
  void UpdateFrameProvider();

  const raw_ref<Profile> profile_;
  const bool supports_client_frame_shadow_;
  bool tiled_ = false;
  raw_ptr<ui::WindowFrameProvider> window_frame_provider_ = nullptr;
  std::unique_ptr<views::FrameBackground> frame_background_;
  base::CallbackListSubscription paint_as_active_subscription_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PICTURE_IN_PICTURE_DOCUMENT_PIP_FRAME_VIEW_LINUX_H_
