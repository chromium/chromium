// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_COLOR_CHOOSER_AURA_H_
#define CHROME_BROWSER_UI_VIEWS_COLOR_CHOOSER_AURA_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "content/public/browser/color_chooser.h"
#include "ui/views/color_chooser/color_chooser_listener.h"
#include "ui/views/widget/unique_widget_ptr.h"

namespace content {
class WebContents;
}

namespace views {
class ColorChooser;
}

// TODO(mukai): rename this as -Ash and move to c/b/ui/ash after Linux-aura
// switches to its native color chooser.
class ColorChooserAura : public content::ColorChooser,
                         public views::ColorChooserListener {
 public:
  static ColorChooserAura* Open(content::WebContents* web_contents,
                                SkColor initial_color);

 private:
  ColorChooserAura(content::WebContents* web_contents, SkColor initial_color);
  ~ColorChooserAura() override;

  // content::ColorChooser overrides:
  void End() override;
  void SetSelectedColor(SkColor color) override;

  // views::ColorChooserListener overrides:
  void OnColorChosen(SkColor color) override;
  void OnColorChooserDialogClosed() override;

  void DidEndColorChooser();

  std::unique_ptr<views::ColorChooser> chooser_;
  views::UniqueWidgetPtr chooser_widget_;

  // The web contents invoking the color chooser.  No ownership because it will
  // outlive this class.
  content::WebContents* web_contents_;

  DISALLOW_COPY_AND_ASSIGN(ColorChooserAura);
};

#endif  // CHROME_BROWSER_UI_VIEWS_COLOR_CHOOSER_AURA_H_
