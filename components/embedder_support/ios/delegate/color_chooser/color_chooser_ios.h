// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EMBEDDER_SUPPORT_IOS_DELEGATE_COLOR_CHOOSER_COLOR_CHOOSER_IOS_H_
#define COMPONENTS_EMBEDDER_SUPPORT_IOS_DELEGATE_COLOR_CHOOSER_COLOR_CHOOSER_IOS_H_

#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/raw_ptr.h"
#include "content/public/browser/color_chooser.h"
#include "third_party/blink/public/mojom/choosers/color_chooser.mojom.h"

namespace content {
class WebContents;
}

@class ColorChooserCoordinatorIOS;

namespace web_contents_delegate_ios {

class ColorChooserIOS : public content::ColorChooser {
 public:
  ColorChooserIOS(
      content::WebContents* web_contents,
      SkColor initial_color,
      const std::vector<blink::mojom::ColorSuggestionPtr>& suggestions);

  ColorChooserIOS(const ColorChooserIOS&) = delete;
  ColorChooserIOS& operator=(const ColorChooserIOS&) = delete;

  ~ColorChooserIOS() override;

  void OnColorChosen(SkColor color);

  // content::ColorChooser interface
  void End() override;
  void SetSelectedColor(SkColor color) override;

  // Return a weak pointer to the current object.
  base::WeakPtr<ColorChooserIOS> AsWeakPtr();

 private:
  // The web contents invoking the color chooser.  No ownership because it will
  // outlive this class.
  raw_ptr<content::WebContents> web_contents_;
  ColorChooserCoordinatorIOS* __strong coordinator_;

  base::WeakPtrFactory<ColorChooserIOS> weak_ptr_factory_{this};
};

}  // namespace web_contents_delegate_ios

#endif  // COMPONENTS_EMBEDDER_SUPPORT_IOS_DELEGATE_COLOR_CHOOSER_COLOR_CHOOSER_IOS_H_
