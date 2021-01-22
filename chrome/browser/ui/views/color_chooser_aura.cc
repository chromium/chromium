// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/color_chooser_aura.h"

#include "build/build_config.h"
#include "chrome/browser/ui/color_chooser.h"
#include "content/public/browser/web_contents.h"
#include "ui/views/color_chooser/color_chooser_view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

ColorChooserAura::ColorChooserAura(content::WebContents* web_contents,
                                   SkColor initial_color)
    : web_contents_(web_contents) {
  chooser_ = std::make_unique<views::ColorChooser>(this, initial_color);
  chooser_widget_ = base::WrapUnique(views::Widget::CreateWindowWithParent(
      chooser_->MakeWidgetDelegate(), web_contents->GetTopLevelNativeWindow()));
  chooser_widget_->Show();
}

ColorChooserAura::~ColorChooserAura() = default;

void ColorChooserAura::OnColorChosen(SkColor color) {
  if (web_contents_)
    web_contents_->DidChooseColorInColorChooser(color);
}

void ColorChooserAura::OnColorChooserDialogClosed() {
  DidEndColorChooser();
}

void ColorChooserAura::End() {
  if (chooser_widget_) {
    // DidEndColorChooser will invoke Browser::DidEndColorChooser, which deletes
    // this. Take care of the call order.
    DidEndColorChooser();
  }
}

void ColorChooserAura::DidEndColorChooser() {
  if (web_contents_)
    web_contents_->DidEndColorChooser();
}

void ColorChooserAura::SetSelectedColor(SkColor color) {
  chooser_->OnColorChanged(color);
}

// static
ColorChooserAura* ColorChooserAura::Open(
    content::WebContents* web_contents, SkColor initial_color) {
  return new ColorChooserAura(web_contents, initial_color);
}

namespace chrome {

content::ColorChooser* ShowColorChooser(content::WebContents* web_contents,
                                        SkColor initial_color) {
  return ColorChooserAura::Open(web_contents, initial_color);
}

}  // namespace chrome
