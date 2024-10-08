// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_TOOLBAR_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_TOOLBAR_BUTTON_H_

#include <memory>
#include <optional>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "chrome/browser/ui/views/chrome_views_export.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/pointer/touch_ui_controller.h"
#include "ui/base/theme_provider.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/context_menu_controller.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/metadata/view_factory.h"

class TabStripModel;

namespace test {
class ToolbarButtonTestApi;
}

namespace ui {
class MenuModel;
}

namespace views {
class MenuModelAdapter;
class MenuRunner;
}

// This class provides basic drawing and mouse-over behavior for buttons
// appearing in the toolbar.
class ToolbarButton : public views::LabelButton,
                      public views::ContextMenuController {
  METADATA_HEADER(ToolbarButton, views::LabelButton)

 public:
  enum class Edge {
    kLeft = 0,
    kRight,
  };

  // More convenient form of the ctor below, when |model| and |tab_strip_model|
  // are both nullptr.
  explicit ToolbarButton(PressedCallback callback = PressedCallback());

  // |tab_strip_model| must outlive this class.
  // |model| can be null if no menu is to be shown.
  // |tab_strip_model| is only needed if showing the menu with |model| requires
  // an active tab. There may be no active tab in |tab_strip_model| during
  // shutdown.
  ToolbarButton(PressedCallback callback,
                std::unique_ptr<ui::MenuModel> model,
                TabStripModel* tab_strip_model,
                bool trigger_menu_on_long_press = true);
  ToolbarButton(const ToolbarButton&) = delete;
  ToolbarButton& operator=(const ToolbarButton&) = delete;
  ~ToolbarButton() override;

  // Highlights the button by setting the label to given |highlight_text|, using
  // tinting it using the |hightlight_color| if set. The highlight is displayed
  // using an animation. If some highlight is already set, it shows the new
  // highlight directly without any animation. To clear the previous highlight
  // (also using an animation), call this function with both parameters empty.
  void SetHighlight(const std::u16string& highlight_text,
                    std::optional<SkColor> highlight_color);

  // Sets the leading margin when the browser is maximized and updates layout to
  // make the focus rectangle centered.
  void SetLeadingMargin(int margin);

  // Sets the trailing margin when the browser is maximized and updates layout
  // to make the focus rectangle centered.
  void SetTrailingMargin(int margin);

  // Methods for handling ButtonDropDown-style menus.
  void ClearPendingMenu();
  bool IsMenuShowing() const;

  // Sets the button's vector icon. This icon uses the default colors and are
  // automatically updated on theme changes.
  void SetVectorIcon(const gfx::VectorIcon& icon);

  // Sets an icon and touch-mode icon for this button.
  // TODO(pbos): Investigate if touch-mode icons are just different-size ones
  // that can be folded into the same .icon file as different CANVAS_DIMENSIONS.
  void SetVectorIcons(const gfx::VectorIcon& icon,
                      const gfx::VectorIcon& touch_icon);

  // Updates the images using the given icon and the default colors returned by
  // GetForegroundColor().
  void UpdateIconsWithStandardColors(const gfx::VectorIcon& icon);

  // Updates the icon images and colors as necessary. Should be called any time
  // icon state changes, e.g. in response to theme or touch mode changes.
  virtual void UpdateIcon();

  // Returns the button's corner radius for `edge`.
  virtual float GetCornerRadiusFor(ToolbarButton::Edge edge) const;

  // Gets/Sets |layout_insets_|, see comment there.
  std::optional<gfx::Insets> GetLayoutInsets() const;
  void SetLayoutInsets(const std::optional<gfx::Insets>& insets);

  // views::LabelButton:
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
  std::u16string GetTooltipText(const gfx::Point& p) const override;
  std::unique_ptr<views::ActionViewInterface> GetActionViewInterface() override;

  // views::ContextMenuController:
  void ShowContextMenuForViewImpl(View* source,
                                  const gfx::Point& point,
                                  ui::MenuSourceType source_type) override;

  // ui::PropertyHandler:
  void AfterPropertyChange(const void* key, int64_t old_value) override;

  ui::MenuModel* menu_model() { return model_.get(); }

  void set_menu_identifier(ui::ElementIdentifier menu_identifier) {
    menu_identifier_ = menu_identifier;
  }
  ui::ElementIdentifier menu_identifier() const { return menu_identifier_; }

  bool GetVectorIconsHasValueForTesting() { return vector_icons_.has_value(); }

 protected:
  struct VectorIcons {
    // RAW_PTR_EXCLUSION: Never allocated by PartitionAlloc (always points to a
    // global), so there is no benefit to using a raw_ptr, only cost.
    RAW_PTR_EXCLUSION const gfx::VectorIcon& icon;
    RAW_PTR_EXCLUSION const gfx::VectorIcon& touch_icon;
  };

  // Returns if menu should be shown. Override this to change default behavior.
  virtual bool ShouldShowMenu();

  // Returns if the button inkdrop should persist after the user interacts with
  // IPH for the button. Override this to change default behavior.
  // TODO(crbug.com/40258442): Investigate if this is still needed and if so how
  // it can be applied to all Buttons rather than just ToolbarButtons.
  virtual bool ShouldShowInkdropAfterIphInteraction();

  // Function to show the dropdown menu.
  virtual void ShowDropDownMenu(ui::MenuSourceType source_type);

  // Sets |layout_inset_delta_|, see comment there.
  void SetLayoutInsetDelta(const gfx::Insets& insets);

  // Updates the button's background and border.
  virtual void UpdateColorsAndInsets();

  // Returns the standard toolbar button foreground color for the given state.
  // This color is typically used for the icon and text of toolbar buttons.
  virtual SkColor GetForegroundColor(ButtonState state) const;

  // Returns the icon size of the toolbar button
  virtual int GetIconSize() const;

  // Retuns true if a non-empty border should be painted.
  virtual bool ShouldPaintBorder() const;

  // Retuns true if the background highlight color should be blended
  // with the toolbar color.
  virtual bool ShouldBlendHighlightColor() const;

  // Returns whether to directly use the highlight as background instead
  // of blending it with the toolbar colors.
  // TODO(shibalik): remove this method after fixing for profile button.
  virtual bool ShouldDirectlyUseHighlightAsBackground() const;

  // Virtual method to explicitly set the highlighted text color instead of the
  // default behavior of the HighlightColorAnimation.
  virtual std::optional<SkColor> GetHighlightTextColor() const;

  // Virtual method to explicitly set the highlighted border color instead of
  // the default behavior of the HighlightColorAnimation.
  virtual std::optional<SkColor> GetHighlightBorderColor() const;

  const std::optional<VectorIcons>& GetVectorIcons() const {
    return vector_icons_;
  }

  // Sets the spacing on the outer side of the label (not the side where the
  // image is). The spacing is applied only when the label is non-empty.
  void SetLabelSideSpacing(int spacing);

  // Returns the target insets according to the button's layout.
  const gfx::Insets GetTargetInsets() const;

  // Returns the target size according to the current button's size and insets.
  const gfx::Size GetTargetSize() const;

  // Returns the button's rounded corner radius based on its size.
  int GetRoundedCornerRadius() const;

  // Updates the images using the given icons and specific colors.
  void UpdateIconsWithColors(const gfx::VectorIcon& icon,
                             SkColor normal_color,
                             SkColor hovered_color,
                             SkColor pressed_color,
                             SkColor disabled_color);

  static constexpr int kDefaultIconSize = 16;
  static constexpr int kDefaultIconSizeChromeRefresh = 20;
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
    void Show(std::optional<SkColor> highlight_color);

    // Starts a fade-out animation. A no-op if the fade-out animation is
    // currently in progress or not shown.
    void Hide();

    // Returns current text / border / background / ink-drop base color based on
    // current |highlight_color_| and on the current animation state (which
    // influences the alpha channel). Returns no value if there is no such color
    // and we should use the default text color / paint no border / paint no
    // background / use the default ink-drop base color.
    std::optional<SkColor> GetTextColor() const;
    std::optional<SkColor> GetBorderColor() const;
    std::optional<SkColor> GetBackgroundColor() const;
    std::optional<SkColor> GetInkDropBaseColor() const;

    void AnimationEnded(const gfx::Animation* animation) override;
    void AnimationProgressed(const gfx::Animation* animation) override;

   private:
    friend test::ToolbarButtonTestApi;

    // Returns whether the animation is currently shown. Note that this returns
    // true even after calling Hide() until the fade-out animation finishes.
    bool IsShown() const;

    void ClearHighlightColor();

    const raw_ptr<ToolbarButton> parent_;

    // A highlight color is used to signal special states. When set this color
    // is used as a base for background, text, border and ink drops. When not
    // set, uses the default ToolbarButton ink drop.
    std::optional<SkColor> highlight_color_;

    // Animation for showing the highlight color (in border, text, and
    // background) when it becomes non-empty and hiding it when it becomes empty
    // again.
    gfx::SlideAnimation highlight_color_animation_;
  };

  void TouchUiChanged();

  // Clears the current highlight, i.e. it sets the label to an empty string and
  // clears the highlight color. If there was a non-empty highlight, previously,
  // it hides the current highlight using an animation. Otherwise, it is a
  // no-op.
  void ClearHighlight();

  // Callback for MenuModelAdapter.
  void OnMenuClosed();

  // views::LabelButton:
  // This is private to avoid a foot-shooter. Callers should use SetHighlight()
  // instead which sets an optional color as well.
  void SetText(const std::u16string& text) override;

  // Sets the in product help promo. Called after the kHasInProductHelpPromoKey
  // property changes. When this button has an in product help promo, the button
  // gets a blue highlight that pulses.
  void SetHasInProductHelpPromo(bool has_in_product_help_promo);

  // The model that populates the attached menu.
  std::unique_ptr<ui::MenuModel> model_;

  const raw_ptr<TabStripModel> tab_strip_model_;

  // Indicates if menu is currently showing.
  bool menu_showing_ = false;

  // Whether the menu should be shown when there is a long mouse press or a drag
  // event.
  const bool trigger_menu_on_long_press_;

  // Determines whether to highlight the button for in-product help.
  // TODO(crbug.com/40258442): Remove this member after issue is addressed.
  bool has_in_product_help_promo_ = false;

  // Y position of mouse when left mouse button is pressed.
  int y_position_on_lbuttondown_ = 0;

  // The model adapter for the drop down menu.
  std::unique_ptr<views::MenuModelAdapter> menu_model_adapter_;

  // Menu runner to display drop down menu.
  std::unique_ptr<views::MenuRunner> menu_runner_;

  // Optional identifier for the menu when it runs.
  ui::ElementIdentifier menu_identifier_;

  // Layout insets to use. This is used when the ToolbarButton is not actually
  // hosted inside the toolbar. If not supplied,
  // |GetLayoutInsets(TOOLBAR_BUTTON)| is used instead which is not appropriate
  // outside the toolbar.
  std::optional<gfx::Insets> layout_insets_;

  // Delta from regular toolbar-button insets. This is necessary for buttons
  // that use smaller or larger icons than regular ToolbarButton instances.
  // CastToolbarButton for instance uses larger insets for touchable mode to
  // match the expected touchable UI.
  gfx::Insets layout_inset_delta_;

  // Used to ensure the button remains highlighted while the menu is active.
  std::optional<Button::ScopedAnchorHighlight> menu_anchor_higlight_;

  // Vector icons for the ToolbarButton. The icon is chosen based on touch-ui.
  // Reacts to theme changes using default colors.
  std::optional<VectorIcons> vector_icons_;

  // Class responsible for animating highlight color (calling a callback on
  // |this| to refresh UI).
  HighlightColorAnimation highlight_color_animation_;

  // If either |last_border_color_| or |last_paint_insets_| have changed since
  // the last update to |border_| it must be recalculated  to match current
  // values.
  std::optional<SkColor> last_border_color_;
  gfx::Insets last_paint_insets_;

  base::CallbackListSubscription subscription_ =
      ui::TouchUiController::Get()->RegisterCallback(
          base::BindRepeating(&ToolbarButton::TouchUiChanged,
                              base::Unretained(this)));

  // A factory for tasks that show the dropdown context menu for the button.
  base::WeakPtrFactory<ToolbarButton> show_menu_factory_{this};
};

class ToolbarButtonActionViewInterface
    : public views::LabelButtonActionViewInterface {
 public:
  explicit ToolbarButtonActionViewInterface(ToolbarButton* action_view);
  ~ToolbarButtonActionViewInterface() override = default;

  // LabelButtonActionViewInterface:
  void ActionItemChangedImpl(actions::ActionItem* action_item) override;

 private:
  raw_ptr<ToolbarButton> action_view_;
};

BEGIN_VIEW_BUILDER(CHROME_VIEWS_EXPORT, ToolbarButton, views::LabelButton)
VIEW_BUILDER_PROPERTY(std::optional<gfx::Insets>, LayoutInsets)
END_VIEW_BUILDER

DEFINE_VIEW_BUILDER(CHROME_VIEWS_EXPORT, ToolbarButton)

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_TOOLBAR_BUTTON_H_
