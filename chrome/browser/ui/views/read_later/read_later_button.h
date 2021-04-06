// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_READ_LATER_READ_LATER_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_READ_LATER_READ_LATER_BUTTON_H_

#include "base/scoped_observation.h"
#include "chrome/browser/ui/views/bubble/webui_bubble_manager.h"
#include "chrome/browser/ui/webui/read_later/read_later_ui.h"
#include "components/reading_list/core/reading_list_model.h"
#include "components/reading_list/core/reading_list_model_observer.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/animation/multi_animation.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/metadata/metadata_header_macros.h"
#include "ui/views/widget/widget_observer.h"
#include "ui/views/widget/widget_utils.h"

class Browser;

namespace views {
class DotIndicator;
}

// Button in the bookmarks bar that provides access to the corresponding
// read later menu.
// TODO(corising): Handle the the async presentation of the UI bubble.
class ReadLaterButton : public views::LabelButton,
                        public views::WidgetObserver,
                        public ReadingListModelObserver {
 public:
  METADATA_HEADER(ReadLaterButton);
  explicit ReadLaterButton(Browser* browser);
  ReadLaterButton(const ReadLaterButton&) = delete;
  ReadLaterButton& operator=(const ReadLaterButton&) = delete;
  ~ReadLaterButton() override;

  void CloseBubble();

  views::DotIndicator* dot_indicator_for_testing() { return dot_indicator_; }

 private:
  class HighlightColorAnimation : gfx::AnimationDelegate {
   public:
    explicit HighlightColorAnimation(ReadLaterButton* parent);
    HighlightColorAnimation(const HighlightColorAnimation&) = delete;
    HighlightColorAnimation& operator=(const HighlightColorAnimation&) = delete;
    ~HighlightColorAnimation() override;

    void Show();
    void Hide();
    void SetColor(SkColor color);

    // Returns current text / background / icon color based on
    // |highlight_color_| and on the current animation state (which
    // influences the alpha channel).
    SkColor GetTextColor() const;
    base::Optional<SkColor> GetBackgroundColor() const;
    SkColor GetIconColor() const;

    void AnimationEnded(const gfx::Animation* animation) override;
    void AnimationProgressed(const gfx::Animation* animation) override;

   private:
    SkColor FadeWithAnimation(SkColor target_color,
                              SkColor original_color) const;
    void ClearHighlightColor();

    ReadLaterButton* const parent_;

    SkColor highlight_color_ = SK_ColorTRANSPARENT;

    // Animation for showing the highlight color (in icon, text, and
    // background).
    gfx::MultiAnimation highlight_color_animation_;
  };

  // LabelButton:
  std::unique_ptr<views::InkDrop> CreateInkDrop() override;
  std::unique_ptr<views::InkDropHighlight> CreateInkDropHighlight()
      const override;
  SkColor GetInkDropBaseColor() const override;
  void OnThemeChanged() override;
  void Layout() override;

  // views::WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override;

  // ReadingListModelObserver:
  void ReadingListModelLoaded(const ReadingListModel* model) override;
  void ReadingListModelBeingDeleted(const ReadingListModel* model) override;
  void ReadingListDidAddEntry(const ReadingListModel* model,
                              const GURL& url,
                              reading_list::EntrySource source) override;

  void ButtonPressed();

  void UpdateColors();

  Browser* const browser_;

  views::DotIndicator* dot_indicator_ = nullptr;

  ReadingListModel* reading_list_model_ = nullptr;
  base::ScopedObservation<ReadingListModel, ReadingListModelObserver>
      reading_list_model_scoped_observation_{this};

  std::unique_ptr<WebUIBubbleManagerT<ReadLaterUI>> webui_bubble_manager_;

  views::WidgetOpenTimer widget_open_timer_;

  base::ScopedObservation<views::Widget, views::WidgetObserver>
      bubble_widget_observation_{this};

  std::unique_ptr<HighlightColorAnimation> highlight_color_animation_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_READ_LATER_READ_LATER_BUTTON_H_
