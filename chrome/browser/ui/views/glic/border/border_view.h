// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_GLIC_BORDER_BORDER_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_GLIC_BORDER_BORDER_VIEW_H_

#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/compositor/compositor_animation_observer.h"
#include "ui/views/view.h"
#include "ui/views/view_observer.h"

class Profile;

namespace content {
class WebContents;
}  // namespace content

namespace gfx {
class Canvas;
}  // namespace gfx

namespace glic {

class BorderView : public views::View,
                   public views::ViewObserver,
                   public ui::CompositorAnimationObserver {
  METADATA_HEADER(BorderView, views::View)

 public:
  // Helper function to find the `BorderView` for `web_contents`. Returns null
  // if there isn't a browser / browser view for `web_contents`.
  static BorderView* FindBorderForWebContents(
      content::WebContents* web_contents);

  static void CancelAllAnimationsForProfile(Profile* profile);

  BorderView();
  BorderView(const BorderView&) = delete;
  BorderView& operator=(const BorderView&) = delete;
  ~BorderView() override;

  // `views::View`:
  void OnPaint(gfx::Canvas* canvas) override;

  // `views::ViewObserver`:
  void OnChildViewAdded(views::View* observed_view,
                        views::View* child) override;

  // `ui::CompositorAnimationObserver`:
  void OnAnimationStep(base::TimeTicks timestamp) override;
  void OnCompositingShuttingDown(ui::Compositor* compositor) override;

  void StartAnimation();

  void CancelAnimation();
};

}  // namespace glic

#endif  // CHROME_BROWSER_UI_VIEWS_GLIC_BORDER_BORDER_VIEW_H_
