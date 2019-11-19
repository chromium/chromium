// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_TOOLBAR_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_TOOLBAR_BUTTON_H_

#include <memory>

#include "base/optional.h"
#include "ui/base/theme_provider.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/gfx/geometry/point.h"
#include "ui/views/context_menu_controller.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/label_button.h"

class TabStripModel;

namespace test {
class ToolbarButtonTestApi;
}

namespace ui {
class MenuModel;
}

namespace views {
class InstallableInkDrop;
class MenuModelAdapter;
class MenuRunner;
}

// This class provides basic drawing and mouse-over behavior for buttons
// appearing in the toolbar.
class ToolbarButton : public views::LabelButton,
                      public views::ContextMenuController {
 public:
  // More convenient form of the ctor below, when |model| and |tab_strip_model|
  // are both nullptr.
  explicit ToolbarButton(views::ButtonListener* listener);

  // |listener| and |tab_strip_model| must outlive this class.
  // |model| can be null if no menu is to be shown.
  // |tab_strip_model| is only needed if showing the menu with |model| requires
  // an active tab. There may be no active tab in |tab_strip_model| during
  // shutdown.
  ToolbarButton(views::ButtonListener* listener,
                std::unique_ptr<ui::MenuModel> model,
                TabStripModel* tab_strip_model,
                bool trigger_menu_on_long_press = true);
  ToolbarButton(const ToolbarButton&) = delete;
  ToolbarButton& operator=(const ToolbarButton&) = delete;
  ~ToolbarButton() override;

  // Set up basic mouseover border behavior.
  // Should be called before first paint.
  void Init();

  // Highlights the button by setting the label to given |highlight_text|, using
  // tinting it using the |hightlight_color| if set. The highlight is displayed
  // using an animation. If some highlight is already set, it shows the new
  // highlight directly without any animation. To clear the previous highlight
  // (also using an animation), call this function with both parameters empty.
  void SetHighlight(const base::string16& highlight_text,
                    base::Optional<SkColor> highlight_color);

  // Sets the leading margin when the browser is maximized and updates layout to
  // make the focus rectangle centered.
  void SetLeadingMargin(int margin);

  // Sets the trailing margin when the browser is maximized and updates layout
  // to make the focus rectangle centered.
  void SetTrailingMargin(int margin);

  // Methods for handling ButtonDropDown-style menus.
  void ClearPendingMenu();
  bool IsMenuShowing() const;

  // Sets |layout_insets_|, see comment there.
  void SetLayoutInsets(const gfx::Insets& insets);

  // views::LabelButton:
  void SetText(const base::string16& text) override;
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;
  void OnThemeChanged() override;
  gfx::Rect GetAnchorBoundsInScreen() const override;
  bool OnMousePressed(const ui::MouseEvent& event) override;
  bool OnMouseDragged(const ui::MouseEvent& event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;
  // Showing the drop down results in a MouseCaptureLost, we need to ignore it.
  void OnMouseCaptureLost() override;
  void OnMouseExited(const ui::MouseEvent& event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  std::unique_ptr<views::InkDrop> CreateInkDrop() override;
  std::unique_ptr<views::InkDropHighlight> CreateInkDropHighlight()
      const override;
  views::InkDrop* GetInkDrop() override;
  SkColor GetInkDropBaseColor() const override;

  // views::ContextMenuController:
  void ShowContextMenuForViewImpl(View* source,
                                  const gfx::Point& point,
                                  ui::MenuSourceType source_type) override;

  ui::MenuModel* menu_model_for_test() { return model_.get(); }

  // Chooses from |desired_dark_color| and |desired_light_color| based on
  // whether the toolbar background is dark or light.
  //
  // If the resulting color will achieve sufficient contrast,
  // returns it. Otherwise, blends it towards |dark_extreme| if it's light, or
  // |dark_extreme| if it's dark until minimum contrast is achieved, and returns
  // the result.
  static SkColor AdjustHighlightColorForContrast(
      const ui::ThemeProvider* theme_provider,
      SkColor desired_dark_color,
      SkColor desired_light_color,
      SkColor dark_extreme,
      SkColor light_extreme);

  // Returns the default border color used for toolbar buttons (when having a
  // highlight text, see SetHighlight()).
  static SkColor GetDefaultBorderColor(views::View* host_view);

 protected:
  // Returns if menu should be shown. Override this to change default behavior.
  virtual bool ShouldShowMenu();

  // Function to show the dropdown menu.
  virtual void ShowDropDownMenu(ui::MenuSourceType source_type);

  // Sets |layout_inset_delta_|, see comment there.
  void SetLayoutInsetDelta(const gfx::Insets& insets);

  void UpdateColorsAndInsets();

  static constexpr int kDefaultIconSize = 16;
  static constexpr int kDefaultTouchableIconSize = 24;

 private:
  friend test::ToolbarButtonTestApi;

  class HighlightColorAnimation : gfx::AnimationDelegate {
   public:
    explicit HighlightColorAnimation(ToolbarButton* parent);
    HighlightColorAnimation(const HighlightColorAnimation&) = delete;
    HighlightColorAnimation& operator=(const HighlightColorAnimation&) = delete;
    ~HighlightColorAnimation() override;

    // Starts a fade-in animation using the provided |highlight color| or using
    // a default color if not set.
    void Show(base::Optional<SkColor> highlight_color);

    // Starts a fade-out animation. A no-op if the fade-out animation is
    // currently in progress or not shown.
    void Hide();

    // Returns current text / border / background / ink-drop base color based on
    // current |highlight_color_| and on the current animation state (which
    // influences the alpha channel). Returns no value if there is no such color
    // and we should use the default text color / paint no border / paint no
    // background / use the default ink-drop base color.
    base::Optional<SkColor> GetTextColor() const;
    base::Optional<SkColor> GetBorderColor() const;
    base::Optional<SkColor> GetBackgroundColor() const;
    base::Optional<SkColor> GetInkDropBaseColor() const;

    void AnimationEnded(const gfx::Animation* animation) override;
    void AnimationProgressed(const gfx::Animation* animation) override;

   private:
    // Returns whether the animation is currently shown. Note that this returns
    // true even after calling Hide() until the fade-out animation finishes.
    bool IsShown() const;

    void ClearHighlightColor();

    ToolbarButton* const parent_;

    // A highlight color is used to signal special states. When set this color
    // is used as a base for background, text, border and ink drops. When not
    // set, uses the default ToolbarButton ink drop.
    base::Optional<SkColor> highlight_color_;

    // Animation for showing the highlight color (in border, text, and
    // background) when it becomes non-empty and hiding it when it becomes empty
    // again.
    gfx::SlideAnimation highlight_color_animation_;
  };

  // Clears the current highlight, i.e. it sets the label to an empty string and
  // clears the highlight color. If there was a non-empty highlight, previously,
  // it hides the current highlight using an animation. Otherwise, it is a
  // no-op.
  void ClearHighlight();

  // Sets the spacing on the outer side of the label (not the side where the
  // image is). The spacing is applied only when the label is non-empty.
  void SetLabelSideSpacing(int spacing);

  // Callback for MenuModelAdapter.
  void OnMenuClosed();

  // views::ImageButton:
  const char* GetClassName() const override;

  // The model that populates the attached menu.
  std::unique_ptr<ui::MenuModel> model_;

  TabStripModel* const tab_strip_model_;

  // Indicates if menu is currently showing.
  bool menu_showing_ = false;

  // Whether the menu should be shown when there is a long mouse press or a drag
  // event.
  const bool trigger_menu_on_long_press_;

  // Y position of mouse when left mouse button is pressed.
  int y_position_on_lbuttondown_ = 0;

  // The model adapter for the drop down menu.
  std::unique_ptr<views::MenuModelAdapter> menu_model_adapter_;

  // Menu runner to display drop down menu.
  std::unique_ptr<views::MenuRunner> menu_runner_;

  // Layout insets to use. This is used when the ToolbarButton is not actually
  // hosted inside the toolbar. If not supplied,
  // |GetLayoutInsets(TOOLBAR_BUTTON)| is used instead which is not appropriate
  // outside the toolbar.
  base::Optional<gfx::Insets> layout_insets_;

  // Delta from regular toolbar-button insets. This is necessary for buttons
  // that use smaller or larger icons than regular ToolbarButton instances.
  // AvatarToolbarButton for instance uses smaller insets to accommodate for a
  // larger-than-16dp avatar avatar icon outside of touchable mode.
  gfx::Insets layout_inset_delta_;

  // Used instead of the standard InkDrop implementation when
  // |views::kInstallableInkDropFeature| is enabled.
  std::unique_ptr<views::InstallableInkDrop> installable_ink_drop_;

  // Class responsible for animating highlight color (calling a callback on
  // |this| to refresh UI).
  HighlightColorAnimation highlight_color_animation_;

  base::Optional<SkColor> last_border_color_;

  // A factory for tasks that show the dropdown context menu for the button.
  base::WeakPtrFactory<ToolbarButton> show_menu_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_TOOLBAR_BUTTON_H_
