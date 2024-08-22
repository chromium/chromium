// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEB_APPS_FRAME_TOOLBAR_WEB_APP_ORIGIN_TEXT_H_
#define CHROME_BROWSER_UI_VIEWS_WEB_APPS_FRAME_TOOLBAR_WEB_APP_ORIGIN_TEXT_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "content/public/browser/web_contents_observer.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/views/view.h"

class Browser;

namespace views {
class Label;
}

// A URL's origin text with a fade in/out animation.
class WebAppOriginText : public views::View,
                         public ui::LayerAnimationObserver,
                         public TabStripModelObserver,
                         public content::WebContentsObserver {
  METADATA_HEADER(WebAppOriginText, views::View)

 public:
  explicit WebAppOriginText(Browser* browser);
  WebAppOriginText(const WebAppOriginText&) = delete;
  WebAppOriginText& operator=(const WebAppOriginText&) = delete;
  ~WebAppOriginText() override;

  // If `show_text` is true, the text will be shown for a few seconds.
  void SetTextColor(SkColor color, bool show_text_);

  void SetAllowedToAnimate(bool allowed);

  // Fades the text in and out.
  void StartFadeAnimation();

  // ui::LayerAnimationObserver:
  void OnLayerAnimationStarted(ui::LayerAnimationSequence* sequence) override {}
  void OnLayerAnimationEnded(ui::LayerAnimationSequence* sequence) override;
  void OnLayerAnimationAborted(ui::LayerAnimationSequence* sequence) override {}
  void OnLayerAnimationScheduled(
      ui::LayerAnimationSequence* sequence) override {}

  const std::u16string& GetLabelTextForTesting();

 private:
  friend class WebAppFrameToolbarTestHelper;

  // TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;

  // content::WebContentsObserver:
  void DidFinishNavigation(content::NavigationHandle* handle) override;

  void UpdateAccessibleName();

  // origin_text_ is populated by ReadyToCommitNavigation.
  std::u16string origin_text_;

  // Disallow animation until the parent view animates for the first time. This
  // helps respect the animation start delay in WebAppToolbarButtonContainer.
  bool allowed_to_animate_ = false;

  // Owned by the views hierarchy.
  raw_ptr<views::Label, DanglingUntriaged> label_ = nullptr;

  base::CallbackListSubscription label_text_changed_callback_;

  base::WeakPtrFactory<WebAppOriginText> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_WEB_APPS_FRAME_TOOLBAR_WEB_APP_ORIGIN_TEXT_H_
