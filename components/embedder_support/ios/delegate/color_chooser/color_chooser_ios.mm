// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/embedder_support/ios/delegate/color_chooser/color_chooser_ios.h"

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#include "components/embedder_support/ios/delegate/color_chooser/color_chooser_coordinator_ios.h"
#include "components/embedder_support/ios/delegate/color_chooser/color_chooser_mediator_ios.h"
#include "content/public/browser/web_contents.h"
#include "skia/ext/skia_utils_ios.h"
#include "ui/gfx/native_widget_types.h"

namespace web_contents_delegate_ios {

ColorChooserIOS::ColorChooserIOS(
    content::WebContents* web_contents,
    SkColor initial_color,
    const std::vector<blink::mojom::ColorSuggestionPtr>& suggestions)
    : web_contents_(web_contents) {
  gfx::NativeWindow native_window = web_contents_->GetTopLevelNativeWindow();
  coordinator_ = [[ColorChooserCoordinatorIOS alloc]
      initWithBaseViewController:native_window.Get().rootViewController
                    colorChooser:this
                           color:skia::UIColorFromSkColor(initial_color)];
}

ColorChooserIOS::~ColorChooserIOS() = default;

void ColorChooserIOS::End() {
  if (!coordinator_) {
    return;
  }
  [coordinator_ closeColorChooser];
  coordinator_ = nullptr;
}

void ColorChooserIOS::SetSelectedColor(SkColor color) {
  if (!coordinator_) {
    return;
  }
  [coordinator_ setColor:skia::UIColorFromSkColor(color)];
}

void ColorChooserIOS::OnColorChosen(SkColor color) {
  // Clean up |coordinator_| since this is called after the UI chooser is
  // closed.
  coordinator_ = nullptr;
  web_contents_->DidChooseColorInColorChooser(color);
  web_contents_->DidEndColorChooser();
}

base::WeakPtr<ColorChooserIOS> ColorChooserIOS::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace web_contents_delegate_ios
