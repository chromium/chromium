// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_CONTENT_SETTING_IMAGE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_CONTENT_SETTING_IMAGE_VIEW_H_

#include <memory>

#include "base/macros.h"
#include "chrome/browser/ui/content_settings/content_setting_image_model.h"
#include "chrome/browser/ui/views/location_bar/icon_label_bubble_view.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/views/painter.h"
#include "ui/views/view.h"

class ContentSettingImageModel;

namespace content {
class WebContents;
}

namespace gfx {
class FontList;
}

namespace views {
class BubbleDialogDelegateView;
}

// The ContentSettingImageView displays an icon and optional text label for
// various content settings affordances in the location bar (i.e. plugin
// blocking, geolocation).
class ContentSettingImageView : public IconLabelBubbleView,
                                public views::WidgetObserver {
 public:
  class Delegate {
   public:
    // Gets the color to use for the ink highlight.
    virtual SkColor GetContentSettingInkDropColor() const = 0;

    // Gets the web contents the ContentSettingImageView is for.
    virtual content::WebContents* GetContentSettingWebContents() = 0;

    // Gets the ContentSettingBubbleModelDelegate for this
    // ContentSettingImageView.
    virtual ContentSettingBubbleModelDelegate*
    GetContentSettingBubbleModelDelegate() = 0;

    // Invoked when a bubble is shown.
    virtual void OnContentSettingImageBubbleShown(
        ContentSettingImageModel::ImageType type) const {}
  };

  ContentSettingImageView(std::unique_ptr<ContentSettingImageModel> image_model,
                          Delegate* delegate,
                          const gfx::FontList& font_list);
  ~ContentSettingImageView() override;

  // Updates the decoration from the shown WebContents.
  void Update();

  // Set the color of the button icon. Based on the text color by default.
  void SetIconColor(SkColor color);

  void disable_animation() { can_animate_ = false; }

  // IconLabelBubbleView:
  const char* GetClassName() const override;
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;
  bool GetTooltipText(const gfx::Point& p,
                      base::string16* tooltip) const override;
  bool OnMousePressed(const ui::MouseEvent& event) override;
  bool OnKeyPressed(const ui::KeyEvent& event) override;
  void OnNativeThemeChanged(const ui::NativeTheme* native_theme) override;
  SkColor GetTextColor() const override;
  bool ShouldShowSeparator() const override;
  bool ShowBubble(const ui::Event& event) override;
  bool IsBubbleShowing() const override;
  SkColor GetInkDropBaseColor() const override;

  ContentSettingImageModel::ImageType GetTypeForTesting() const;

 private:
  // views::WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override;

  // Updates the image and tooltip to match the current model state.
  void UpdateImage();

  Delegate* delegate_;  // Weak.
  std::unique_ptr<ContentSettingImageModel> content_setting_image_model_;
  views::BubbleDialogDelegateView* bubble_view_;
  base::Optional<SkColor> icon_color_;

  bool can_animate_ = true;

  DISALLOW_COPY_AND_ASSIGN(ContentSettingImageView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_CONTENT_SETTING_IMAGE_VIEW_H_
