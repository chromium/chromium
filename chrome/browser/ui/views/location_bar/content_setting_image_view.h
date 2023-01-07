// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_CONTENT_SETTING_IMAGE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_CONTENT_SETTING_IMAGE_VIEW_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/content_settings/content_setting_image_model.h"
#include "chrome/browser/ui/views/location_bar/icon_label_bubble_view.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/user_education/common/help_bubble.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/views/painter.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

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
  METADATA_HEADER(ContentSettingImageView);

  class Delegate {
   public:
    // Delegate should return true if the content setting icon should be hidden.
    virtual bool ShouldHideContentSettingImage() = 0;

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
                          IconLabelBubbleView::Delegate* parent_delegate,
                          Delegate* delegate,
                          const gfx::FontList& font_list);
  ContentSettingImageView(const ContentSettingImageView&) = delete;
  ContentSettingImageView& operator=(const ContentSettingImageView&) = delete;
  ~ContentSettingImageView() override;

  // Updates the decoration from the shown WebContents.
  void Update();

  // Set the color of the button icon. Based on the text color by default.
  void SetIconColor(absl::optional<SkColor> color);
  absl::optional<SkColor> GetIconColor() const;

  void disable_animation() { can_animate_ = false; }

  bool ShowBubbleImpl();

  // IconLabelBubbleView:
  bool OnMousePressed(const ui::MouseEvent& event) override;
  bool OnKeyPressed(const ui::KeyEvent& event) override;
  void OnThemeChanged() override;
  bool ShouldShowSeparator() const override;
  bool ShowBubble(const ui::Event& event) override;
  bool IsBubbleShowing() const override;
  void AnimationEnded(const gfx::Animation* animation) override;

  ContentSettingImageModel::ImageType GetTypeForTesting() const;
  views::Widget* GetBubbleWidgetForTesting() const;

  void reset_animation_for_testing() {
    IconLabelBubbleView::ResetSlideAnimation(true);
  }
  user_education::HelpBubble* critical_promo_bubble_for_testing() {
    return critical_promo_bubble_.get();
  }

 private:
  // views::WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override;

  // Updates the image and tooltip to match the current model state.
  void UpdateImage();

  raw_ptr<Delegate> delegate_ = nullptr;  // Weak.
  std::unique_ptr<ContentSettingImageModel> content_setting_image_model_;
  raw_ptr<views::BubbleDialogDelegateView> bubble_view_ = nullptr;
  absl::optional<SkColor> icon_color_;

  // Observes destruction of bubble's Widgets spawned by this ImageView.
  base::ScopedObservation<views::Widget, views::WidgetObserver> observation_{
      this};
  bool can_animate_ = true;

  // Has a value that is not is_zero() if a promo is showing, or has an
  // is_zero() value if the promo was considered but it was decided not to show
  // it.
  std::unique_ptr<user_education::HelpBubble> critical_promo_bubble_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_CONTENT_SETTING_IMAGE_VIEW_H_
