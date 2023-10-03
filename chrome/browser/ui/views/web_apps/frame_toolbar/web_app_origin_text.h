// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEB_APPS_FRAME_TOOLBAR_WEB_APP_ORIGIN_TEXT_H_
#define CHROME_BROWSER_UI_VIEWS_WEB_APPS_FRAME_TOOLBAR_WEB_APP_ORIGIN_TEXT_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/views/view.h"

class Browser;

namespace views {
class Label;
}

// A URL's origin text with a fade in/out animation.
class WebAppOriginText : public views::View, public ui::LayerAnimationObserver {
 public:
  METADATA_HEADER(WebAppOriginText);
  explicit WebAppOriginText(Browser* browser);
  WebAppOriginText(const WebAppOriginText&) = delete;
  WebAppOriginText& operator=(const WebAppOriginText&) = delete;
  ~WebAppOriginText() override;

  // If `show_text` is true, the text will be shown for a few seconds.
  void SetTextColor(SkColor color, bool show_text_);

  // Fades the text in and out.
  void StartFadeAnimation();

  // views::View:
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;

  // ui::LayerAnimationObserver:
  void OnLayerAnimationStarted(ui::LayerAnimationSequence* sequence) override {}
  void OnLayerAnimationEnded(ui::LayerAnimationSequence* sequence) override;
  void OnLayerAnimationAborted(ui::LayerAnimationSequence* sequence) override {}
  void OnLayerAnimationScheduled(
      ui::LayerAnimationSequence* sequence) override {}

 private:
  // Owned by the views hierarchy.
  raw_ptr<views::Label, DanglingUntriaged> label_ = nullptr;

  base::WeakPtrFactory<WebAppOriginText> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_WEB_APPS_FRAME_TOOLBAR_WEB_APP_ORIGIN_TEXT_H_
