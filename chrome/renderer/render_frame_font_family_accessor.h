// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_RENDER_FRAME_FONT_FAMILY_ACCESSOR_H_
#define CHROME_RENDERER_RENDER_FRAME_FONT_FAMILY_ACCESSOR_H_

#include <optional>

#include "chrome/common/font_prewarmer.mojom.h"
#include "content/public/renderer/render_frame_observer.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "third_party/blink/public/web/win/web_font_family_names.h"

namespace content {
class RenderFrame;
}

// Waits for the render frame to generate fcp and copies the font names so that
// they can be supplied back to browser via
// chrome::mojom::RenderFrameFontFamilyAccessor .
class RenderFrameFontFamilyAccessor
    : public chrome::mojom::RenderFrameFontFamilyAccessor,
      public content::RenderFrameObserver {
 public:
  RenderFrameFontFamilyAccessor(const RenderFrameFontFamilyAccessor&) = delete;
  RenderFrameFontFamilyAccessor& operator=(
      const RenderFrameFontFamilyAccessor&) = delete;

  static void Bind(
      content::RenderFrame* render_frame,
      mojo::PendingAssociatedReceiver<
          chrome::mojom::RenderFrameFontFamilyAccessor> pending_receiver);

  // chrome::mojom::RenderFrameFontFamilyAccessor:
  void GetFontFamilyNames(GetFontFamilyNamesCallback callback) override;

  // content::RenderFrameObserver:
  void OnDestruct() override;
  void DidChangePerformanceTiming() override;
  void ReadyToCommitNavigation(
      blink::WebDocumentLoader* document_loader) override;

 private:
  RenderFrameFontFamilyAccessor(
      content::RenderFrame* render_frame,
      mojo::PendingAssociatedReceiver<
          chrome::mojom::RenderFrameFontFamilyAccessor> pending_receiver);
  ~RenderFrameFontFamilyAccessor() override;

  // Returns true if the font names should be obtained.
  bool ShouldGetFontNames() const;

  // Copies the fonts from blink.
  void GetFontNames();

  // Returns true if the font names have been copied.
  bool got_font_names() const { return family_names_.has_value(); }

  void RunCallback(GetFontFamilyNamesCallback callback);

  std::optional<blink::WebFontFamilyNames> family_names_;
  GetFontFamilyNamesCallback callback_;
  mojo::AssociatedReceiver<chrome::mojom::RenderFrameFontFamilyAccessor>
      receiver_;
  // Whether ReadyToCommitNavigation() has been called.
  bool got_commit_ = false;
};

#endif  // CHROME_RENDERER_RENDER_FRAME_FONT_FAMILY_ACCESSOR_H_
