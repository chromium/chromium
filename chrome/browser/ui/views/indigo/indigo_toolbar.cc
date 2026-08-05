// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/indigo/indigo_toolbar.h"

#include <memory>
#include <utility>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/time/time.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/glic/browser_ui/glic_vector_icon_manager.h"
#include "chrome/browser/indigo/resources/grit/indigo_strings.h"
#include "chrome/browser/ui/views/controls/hover_button.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/ui_base_features.h"
#include "ui/color/color_id.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/background.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/separator.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/layout/animating_layout_manager.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/layout/layout_manager_base.h"
#include "ui/views/metadata/view_factory.h"
#include "ui/views/vector_icons.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_targeter.h"
#include "ui/views/view_targeter_delegate.h"
#include "ui/views/view_utils.h"

namespace indigo {

DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(gfx::Rect, kIndigoTrackedElementRectKey)
DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(gfx::Vector2d, kIndigoToolbarCornerOffsetKey)

namespace {

// In the layout specifications below, "Padding" constants define the empty
// border (internal padding) inside the views. This controls the clickable
// target bounds and the shape of the gray hover highlight background. "Margin"
// constants define the external margins used by FlexLayoutView to position
// views relative to their siblings or parent borders.

// Dimensions (in dip).
constexpr int kToolbarInitialOffset = 20;

// General layout constants.
constexpr int kIconSize = 16;

// Collapsed chip layout specifications.
constexpr int kExpandButtonVerticalPadding = 4;
constexpr int kExpandButtonLeftPadding = 6;
constexpr int kExpandButtonRightPadding = 4;

constexpr int kTopRowButtonVerticalMargin = 4;
constexpr int kExpandButtonLeftMargin = 6;
constexpr int kExpandButtonRightMargin = 4;
constexpr int kExpandButtonIconLabelSpacing = 8;

constexpr int kCloseButtonSize = 24;
constexpr int kCloseButtonLeftMargin = 4;
constexpr int kCloseButtonRightMargin = 4;

// Expanded menu items layout specifications.
constexpr int kMenuItemVerticalPadding = 4;
constexpr int kMenuItemHorizontalPadding = 6;
constexpr int kMenuItemVerticalMargin = 4;
constexpr int kMenuItemHorizontalMargin = 6;

// Timing specifications.

// Radius constants.
constexpr int kToolbarCornerRadius = 10;
constexpr int kExpandButtonHoverRadius = 8;
constexpr int kCloseButtonHoverRadius = kCloseButtonSize / 2;
constexpr int kMenuItemHoverCornerRadius = 4;

class IndigoOverlayTargeterDelegate : public views::ViewTargeterDelegate {
 public:
  IndigoOverlayTargeterDelegate() = default;
  IndigoOverlayTargeterDelegate(const IndigoOverlayTargeterDelegate&) = delete;
  IndigoOverlayTargeterDelegate& operator=(
      const IndigoOverlayTargeterDelegate&) = delete;
  ~IndigoOverlayTargeterDelegate() override = default;

  bool DoesIntersectRect(const views::View* target,
                         const gfx::Rect& rect) const override {
    for (const views::View* child : target->children()) {
      gfx::RectF child_rect(rect);
      views::View::ConvertRectToTarget(target, child, &child_rect);
      if (child->HitTestRect(gfx::ToEnclosingRect(child_rect))) {
        return true;
      }
    }
    return false;
  }
};

class IndigoOverlayLayoutManager : public views::LayoutManagerBase {
 public:
  IndigoOverlayLayoutManager() = default;
  IndigoOverlayLayoutManager(const IndigoOverlayLayoutManager&) = delete;
  IndigoOverlayLayoutManager& operator=(const IndigoOverlayLayoutManager&) =
      delete;
  ~IndigoOverlayLayoutManager() override = default;

 protected:
  views::ProposedLayout CalculateProposedLayout(
      const views::SizeBounds& size_bounds) const override {
    views::ProposedLayout layout;
    gfx::Rect bounds;

    for (views::View* child : host_view()->children()) {
      if (!IsChildIncludedInLayout(child)) {
        continue;
      }
      gfx::Size preferred_size = child->GetPreferredSize(views::SizeBounds());
      gfx::Point origin = GetChildOrigin(child);
      gfx::Rect child_bounds(origin, preferred_size);
      layout.child_layouts.push_back(
          {child, child->GetVisible(), child_bounds, size_bounds});
      bounds.Union(child_bounds);
    }
    layout.host_size = bounds.size();
    return layout;
  }

 private:
  gfx::Point GetChildOrigin(const views::View* child) const {
    const gfx::Rect* tracked_rect =
        child->GetProperty(kIndigoTrackedElementRectKey);
    const gfx::Vector2d* corner_offset =
        child->GetProperty(kIndigoToolbarCornerOffsetKey);
    gfx::Size preferred_size = child->GetPreferredSize();
    gfx::Insets insets = child->GetInsets();

    if (tracked_rect && !tracked_rect->IsEmpty() && corner_offset) {
      gfx::Point origin = tracked_rect->top_right() + *corner_offset;
      origin.Offset(insets.right() - preferred_size.width(), -insets.top());
      return origin;
    }
    return gfx::Point(kToolbarInitialOffset, kToolbarInitialOffset);
  }
};

ui::ImageModel GetChevronImageModel(bool is_expanded) {
  return ui::ImageModel::FromVectorIcon(
      is_expanded ? (features::IsRoundedIconsEnabled()
                         ? vector_icons::kKeyboardArrowUpIcon
                         : vector_icons::kCaretUpOldIcon)
                  : (features::IsRoundedIconsEnabled()
                         ? vector_icons::kKeyboardArrowDownIcon
                         : vector_icons::kCaretDownOldIcon),
      ui::kColorSysOnSurface, kIconSize);
}

std::unique_ptr<views::ImageView> CreateCompactSparkIcon() {
  auto spark_icon = std::make_unique<views::ImageView>();
  spark_icon->SetCanProcessEventsWithinSubtree(false);
  spark_icon->SetProperty(views::kElementIdentifierKey,
                          IndigoToolbar::kSparkIconElementId);
  spark_icon->SetImage(ui::ImageModel::FromVectorIcon(
      glic::GlicVectorIconManager::GetVectorIcon(IDR_GLIC_BUTTON_VECTOR_ICON),
      ui::kColorSysOnSurface, kIconSize));
  return spark_icon;
}

// A custom view that tracks mouse and focus events to notify when the toolbar's
// interaction state starts or ends.
class IndigoToolbarView : public views::View,
                          public views::FocusChangeListener {
  METADATA_HEADER(IndigoToolbarView, views::View)

 public:
  explicit IndigoToolbarView(
      base::RepeatingCallback<void(bool)> interaction_changed_callback)
      : interaction_changed_callback_(std::move(interaction_changed_callback)) {
    SetLayoutManager(std::make_unique<views::FillLayout>());
    SetNotifyEnterExitOnChild(true);
  }

  IndigoToolbarView(const IndigoToolbarView&) = delete;
  IndigoToolbarView& operator=(const IndigoToolbarView&) = delete;
  ~IndigoToolbarView() override = default;

  void ChildPreferredSizeChanged(views::View* child) override {
    views::View::ChildPreferredSizeChanged(child);
    PreferredSizeChanged();
  }

  void OnMouseEntered(const ui::MouseEvent& event) override {
    views::View::OnMouseEntered(event);
    is_mouse_over_ = true;
    MaybeNotifyInteractionState();
  }

  void OnMouseExited(const ui::MouseEvent& event) override {
    views::View::OnMouseExited(event);
    is_mouse_over_ = false;
    MaybeNotifyInteractionState();
  }

  void AddedToWidget() override {
    views::View::AddedToWidget();
    if (auto* focus_manager = GetFocusManager()) {
      focus_manager->AddFocusChangeListener(this);
    }
  }

  void RemovedFromWidget() override {
    if (auto* focus_manager = GetFocusManager()) {
      focus_manager->RemoveFocusChangeListener(this);
    }
    views::View::RemovedFromWidget();
  }

  void OnDidChangeFocus(views::View* focused_before,
                        views::View* focused_now) override {
    const bool contained_focus_before =
        focused_before && Contains(focused_before);
    const bool contains_focus_now = focused_now && Contains(focused_now);
    if (contained_focus_before != contains_focus_now) {
      MaybeNotifyInteractionState();
    }
  }

 private:
  void MaybeNotifyInteractionState() {
    auto* focus_manager = GetFocusManager();
    const bool is_focused =
        focus_manager && Contains(focus_manager->GetFocusedView());
    const bool is_interacting = is_focused || is_mouse_over_;
    if (is_interacting != is_interacting_) {
      is_interacting_ = is_interacting;
      interaction_changed_callback_.Run(is_interacting);
    }
  }

  base::RepeatingCallback<void(bool)> interaction_changed_callback_;
  bool is_interacting_ = false;
  bool is_mouse_over_ = false;
};

BEGIN_METADATA(IndigoToolbarView)
END_METADATA

class IndigoExpandButton : public HoverButton {
  METADATA_HEADER(IndigoExpandButton, HoverButton)

 public:
  explicit IndigoExpandButton(
      PressedCallback callback,
      base::RepeatingCallback<void(bool)> interaction_changed_callback)
      : HoverButton(std::move(callback), CreateExpandButtonParams()),
        interaction_changed_callback_(std::move(interaction_changed_callback)) {
    SetProperty(views::kElementIdentifierKey,
                IndigoToolbar::kExpandButtonElementId);
    SetProperty(
        views::kFlexBehaviorKey,
        views::FlexSpecification(views::MinimumFlexSizeRule::kPreferred,
                                 views::MaximumFlexSizeRule::kUnbounded));
    SetBorder(views::CreateEmptyBorder(gfx::Insets::TLBR(
        kExpandButtonVerticalPadding, kExpandButtonLeftPadding,
        kExpandButtonVerticalPadding, kExpandButtonRightPadding)));
    title()->SetTextContext(views::style::CONTEXT_LABEL);
    title()->SetTextStyle(views::style::STYLE_BODY_5);
    title()->SetEnabledColor(ui::kColorSysOnSurface);
    views::InstallRoundRectHighlightPathGenerator(this, gfx::Insets(),
                                                  kExpandButtonHoverRadius);
    views::InkDrop::UseInkDropForFloodFillRipple(views::InkDrop::Get(this),
                                                 /*highlight_on_hover=*/true,
                                                 /*highlight_on_focus=*/true);
    SetFocusBehavior(FocusBehavior::ACCESSIBLE_ONLY);

    CHECK(icon_view());
    icon_view()->parent()->SetProperty(views::kMarginsKey, gfx::Insets());
    icon_view()->parent()->SetVisible(false);

    chevron_ = views::AsViewClass<views::ImageView>(secondary_view());
    CHECK(chevron_);
    chevron_->SetProperty(
        views::kMarginsKey,
        gfx::Insets::TLBR(0, kExpandButtonIconLabelSpacing, 0, 0));
    UpdateState();
  }

  IndigoExpandButton(const IndigoExpandButton&) = delete;
  IndigoExpandButton& operator=(const IndigoExpandButton&) = delete;
  ~IndigoExpandButton() override = default;

  void SetExpanded(bool expanded) {
    if (expanded == is_expanded_) {
      return;
    }
    is_expanded_ = expanded;
    UpdateState();
  }

  void SetCompact(bool compact) {
    if (compact == is_compact_) {
      return;
    }
    is_compact_ = compact;
    icon_view()->parent()->SetVisible(compact);
    title()->parent()->SetVisible(!compact);
    chevron_->SetVisible(!compact);

    SetBorder(views::CreateEmptyBorder(gfx::Insets::TLBR(
        kExpandButtonVerticalPadding,
        compact ? kCloseButtonLeftMargin : kExpandButtonLeftPadding,
        kExpandButtonVerticalPadding, kExpandButtonRightPadding)));

    SetProperty(
        views::kMarginsKey,
        gfx::Insets::TLBR(
            kTopRowButtonVerticalMargin,
            compact ? kCloseButtonLeftMargin : kExpandButtonLeftMargin,
            kTopRowButtonVerticalMargin,
            compact ? kCloseButtonRightMargin : kExpandButtonRightMargin));

    SetProperty(
        views::kFlexBehaviorKey,
        compact
            ? views::FlexSpecification(views::MinimumFlexSizeRule::kPreferred,
                                       views::MaximumFlexSizeRule::kPreferred)
            : views::FlexSpecification(views::MinimumFlexSizeRule::kPreferred,
                                       views::MaximumFlexSizeRule::kUnbounded));
  }

  void StateChanged(ButtonState old_state) override {
    HoverButton::StateChanged(old_state);
    MaybeNotifyInteractionState();
  }

  void OnFocus() override {
    HoverButton::OnFocus();
    MaybeNotifyInteractionState();
  }

  void OnBlur() override {
    HoverButton::OnBlur();
    MaybeNotifyInteractionState();
  }

 private:
  void MaybeNotifyInteractionState() {
    const bool is_interacting = HasFocus() || GetState() == STATE_HOVERED ||
                                GetState() == STATE_PRESSED;
    if (is_interacting != is_interacting_) {
      is_interacting_ = is_interacting;
      interaction_changed_callback_.Run(is_interacting);
    }
  }
  static HoverButton::Params CreateExpandButtonParams() {
    HoverButton::Params params;
    params.icon_view = CreateCompactSparkIcon();
    params.icon_label_spacing = 0;
    params.title = l10n_util::GetStringUTF16(IDS_INDIGO_TOOLBAR_CAPTION);
    params.secondary_view = CreateChevronView();
    params.add_vertical_label_spacing = false;
    return params;
  }

  static std::unique_ptr<views::ImageView> CreateChevronView() {
    auto chevron = std::make_unique<views::ImageView>();
    chevron->SetProperty(views::kElementIdentifierKey,
                         IndigoToolbar::kChevronElementId);
    chevron->SetImage(GetChevronImageModel(/*is_expanded=*/false));
    return chevron;
  }

  void UpdateState() {
    SetTooltipText(l10n_util::GetStringUTF16(is_expanded_
                                                 ? IDS_INDIGO_TOOLBAR_COLLAPSE
                                                 : IDS_INDIGO_TOOLBAR_EXPAND));

    if (is_expanded_) {
      GetViewAccessibility().SetIsExpanded();
    } else {
      GetViewAccessibility().SetIsCollapsed();
    }

    chevron_->SetImage(GetChevronImageModel(is_expanded_));
  }

  bool is_expanded_ = false;
  bool is_compact_ = false;
  bool is_interacting_ = false;
  base::RepeatingCallback<void(bool)> interaction_changed_callback_;
  raw_ptr<views::ImageView> chevron_ = nullptr;
};

BEGIN_METADATA(IndigoExpandButton)
END_METADATA

}  // namespace

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(IndigoToolbar, kToolbarElementId);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(IndigoToolbar,
                                      kAnimatingContainerElementId);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(IndigoToolbar, kCloseButtonElementId);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(IndigoToolbar, kExpandButtonElementId);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(IndigoToolbar, kChevronElementId);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(IndigoToolbar, kSparkIconElementId);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(IndigoToolbar,
                                      kExpandedContainerElementId);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(IndigoToolbar,
                                      kRegenerateButtonElementId);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(IndigoToolbar,
                                      kReplacePhotoButtonElementId);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(IndigoToolbar,
                                      kDeletePhotoButtonElementId);

std::unique_ptr<views::View> CreateIndigoOverlayView() {
  auto view = std::make_unique<views::View>();
  view->SetEventTargeter(std::make_unique<views::ViewTargeter>(
      std::make_unique<IndigoOverlayTargeterDelegate>()));
  view->SetLayoutManager(std::make_unique<IndigoOverlayLayoutManager>());
  return view;
}

IndigoToolbar::IndigoToolbar(Delegate* delegate) : delegate_(delegate) {}

IndigoToolbar::~IndigoToolbar() {
  Hide();
}

std::unique_ptr<views::View> IndigoToolbar::CreateToolbarView() {
  auto bubble_border = std::make_unique<views::BubbleBorder>(
      views::BubbleBorder::NONE, views::BubbleBorder::STANDARD_SHADOW);
  bubble_border->SetColor(ui::kColorSysSurface);
  bubble_border->set_rounded_corners(
      gfx::RoundedCornersF(kToolbarCornerRadius));

  auto toolbar_view = std::make_unique<IndigoToolbarView>(base::BindRepeating(
      &IndigoToolbar::OnToolbarInteractionChanged, base::Unretained(this)));

  auto layout_manager = std::make_unique<views::AnimatingLayoutManager>();
  auto* animating_layout = layout_manager.get();
  animating_layout
      ->SetBoundsAnimationMode(
          views::AnimatingLayoutManager::BoundsAnimationMode::kAnimateBothAxes)
      .SetAnimationDuration(kToolbarAnimationDuration)
      .SetTweenType(gfx::Tween::FAST_OUT_SLOW_IN)
      .SetDefaultFadeMode(
          views::AnimatingLayoutManager::FadeInOutMode::kSlideFromLeadingEdge);

  auto animating_container =
      views::Builder<views::View>()
          .SetLayoutManager(std::move(layout_manager))
          .SetProperty(views::kElementIdentifierKey,
                       kAnimatingContainerElementId)
          .AddChildren(
              // Top row: Always visible
              views::Builder<views::FlexLayoutView>()
                  .SetOrientation(views::LayoutOrientation::kHorizontal)
                  .SetMainAxisAlignment(views::LayoutAlignment::kEnd)
                  .SetCrossAxisAlignment(views::LayoutAlignment::kStretch)
                  .SetCollapseMargins(true)
                  .AddChildren(
                      views::Builder<HoverButton>(
                          std::make_unique<IndigoExpandButton>(
                              base::BindRepeating(
                                  &IndigoToolbar::OnExpandButtonClicked,
                                  base::Unretained(this)),
                              base::BindRepeating(
                                  &IndigoToolbar::
                                      OnExpandButtonInteractionChanged,
                                  base::Unretained(this))))
                          .SetProperty(
                              views::kMarginsKey,
                              gfx::Insets::TLBR(kTopRowButtonVerticalMargin,
                                                kExpandButtonLeftMargin,
                                                kTopRowButtonVerticalMargin,
                                                kExpandButtonRightMargin)),
                      views::Builder<views::Separator>()
                          .SetOrientation(
                              views::Separator::Orientation::kVertical)
                          .SetColorId(ui::kColorSysDivider),
                      views::Builder<views::ImageButton>(
                          views::CreateVectorImageButton(base::BindRepeating(
                              &IndigoToolbar::OnCloseButtonClicked,
                              base::Unretained(this))))
                          .SetProperty(views::kElementIdentifierKey,
                                       kCloseButtonElementId)
                          .SetProperty(
                              views::kMarginsKey,
                              gfx::Insets::TLBR(kTopRowButtonVerticalMargin,
                                                kCloseButtonLeftMargin,
                                                kTopRowButtonVerticalMargin,
                                                kCloseButtonRightMargin))
                          .SetProperty(views::kCrossAxisAlignmentKey,
                                       views::LayoutAlignment::kCenter)
                          .SetPreferredSize(
                              gfx::Size(kCloseButtonSize, kCloseButtonSize))
                          .SetImageModel(views::Button::STATE_NORMAL,
                                         ui::ImageModel::FromVectorIcon(
                                             features::IsRoundedIconsEnabled()
                                                 ? vector_icons::kCloseIcon
                                                 : vector_icons::kCloseOldIcon,
                                             ui::kColorSysOnSurface, kIconSize))
                          .SetTooltipText(l10n_util::GetStringUTF16(IDS_CLOSE))
                          .SetFocusBehavior(
                              views::View::FocusBehavior::ACCESSIBLE_ONLY)
                          .CustomConfigure(
                              base::BindOnce([](views::ImageButton* button) {
                                // CreateVectorImageButton doesn't set the ink
                                // drop base color automatically.
                                views::InkDrop::Get(button)->SetBaseColor(
                                    ui::kColorSysOnSurfaceSubtle);
                                views::InstallRoundRectHighlightPathGenerator(
                                    button, gfx::Insets(),
                                    kCloseButtonHoverRadius);
                              }))),
              // Expanded Row: Toggled visibility
              views::Builder<views::FlexLayoutView>()
                  .SetProperty(views::kElementIdentifierKey,
                               kExpandedContainerElementId)
                  .SetProperty(views::kMarginsKey,
                               gfx::Insets::TLBR(0, 0, 4, 0))
                  .SetVisible(false)
                  .SetOrientation(views::LayoutOrientation::kVertical)
                  .SetCrossAxisAlignment(views::LayoutAlignment::kStretch)
                  .SetCollapseMargins(true)
                  .AddChildren(
                      views::Builder<views::Separator>()
                          .SetOrientation(
                              views::Separator::Orientation::kHorizontal)
                          .SetColorId(ui::kColorSysDivider),
                      views::Builder<views::Button>(
                          CreateExpandedButton(
                              l10n_util::GetStringUTF16(
                                  IDS_INDIGO_TOOLBAR_REGENERATE),
                              vector_icons::kRefreshIcon,
                              base::BindRepeating(
                                  &IndigoToolbar::OnRegenerateButtonClicked,
                                  base::Unretained(this))))
                          .SetProperty(views::kElementIdentifierKey,
                                       kRegenerateButtonElementId),
                      views::Builder<views::Separator>()
                          .SetOrientation(
                              views::Separator::Orientation::kHorizontal)
                          .SetColorId(ui::kColorSysDivider),
                      views::Builder<views::Button>(
                          CreateExpandedButton(
                              l10n_util::GetStringUTF16(
                                  IDS_INDIGO_TOOLBAR_REPLACE_ORIGINAL_PHOTO),
                              vector_icons::kPhotoSparkIcon,
                              base::BindRepeating(
                                  &IndigoToolbar::OnReplacePhotoClicked,
                                  base::Unretained(this))))
                          .SetProperty(views::kElementIdentifierKey,
                                       kReplacePhotoButtonElementId),
                      views::Builder<views::Button>(
                          CreateExpandedButton(
                              l10n_util::GetStringUTF16(
                                  IDS_INDIGO_TOOLBAR_DELETE_ORIGINAL_PHOTO),
                              features::IsRoundedIconsEnabled()
                                  ? kDeleteIcon
                                  : kDeleteOldIcon,
                              base::BindRepeating(
                                  &IndigoToolbar::OnDeletePhotoClicked,
                                  base::Unretained(this))))
                          .SetProperty(views::kElementIdentifierKey,
                                       kDeletePhotoButtonElementId)))
          .Build();

  // The animating layout requires a target layout manager to handle the actual
  // sizing and positioning of children during and after animations.
  auto flex_layout = std::make_unique<views::FlexLayout>();
  flex_layout->SetOrientation(views::LayoutOrientation::kVertical)
      .SetMainAxisAlignment(views::LayoutAlignment::kStart)
      .SetCrossAxisAlignment(views::LayoutAlignment::kStretch)
      .SetDefault(views::kMarginsKey, gfx::Insets())
      .SetCollapseMargins(true);

  animating_layout->SetTargetLayoutManager(std::move(flex_layout));

  auto view =
      views::Builder<views::View>(std::move(toolbar_view))
          .SetProperty(views::kElementIdentifierKey, kToolbarElementId)
          .SetPaintToLayer()
          .CustomConfigure(base::BindOnce([](views::View* view) {
            view->layer()->SetFillsBoundsOpaquely(false);
            view->layer()->SetMasksToBounds(true);
            view->layer()->SetName("IndigoToolbar");
            view->GetViewAccessibility().SetRole(ax::mojom::Role::kToolbar);
            view->GetViewAccessibility().SetName(
                l10n_util::GetStringUTF16(IDS_INDIGO_TOOLBAR_CAPTION));
          }))
          .SetBackground(
              std::make_unique<views::BubbleBackground>(bubble_border.get()))
          .SetBorder(std::move(bubble_border))
          .AddChild(views::Builder<views::View>(std::move(animating_container)))
          .Build();

  return view;
}

std::unique_ptr<views::Button> IndigoToolbar::CreateExpandedButton(
    const std::u16string& label,
    const gfx::VectorIcon& icon,
    views::Button::PressedCallback callback) {
  auto button = std::make_unique<HoverButton>(
      std::move(callback),
      ui::ImageModel::FromVectorIcon(icon, ui::kColorSysOnSurface, kIconSize),
      label);
  button->SetEnabledTextColors(ui::kColorSysOnSurface);
  button->SetLabelStyle(views::style::STYLE_BODY_5);
  button->SetBorder(views::CreateEmptyBorder(
      gfx::Insets::VH(kMenuItemVerticalPadding, kMenuItemHorizontalPadding)));
  button->SetProperty(
      views::kMarginsKey,
      gfx::Insets::VH(kMenuItemVerticalMargin, kMenuItemHorizontalMargin));
  views::InstallRoundRectHighlightPathGenerator(button.get(), gfx::Insets(),
                                                kMenuItemHoverCornerRadius);
  views::InkDrop::UseInkDropForFloodFillRipple(
      views::InkDrop::Get(button.get()),
      /*highlight_on_hover=*/true,
      /*highlight_on_focus=*/true);
  button->SetFocusBehavior(views::View::FocusBehavior::ACCESSIBLE_ONLY);
  return button;
}

void IndigoToolbar::Show(views::View* parent_view) {
  views::View* view = view_tracker_.view();
  const bool is_new_view = !view;
  if (is_new_view) {
    // If the view is newly constructed, we raw-assign the initial state.
    // CreateToolbarView() initializes the view tree in the collapsed
    // structure, so doing a forced SetPresentationState below is redundant.
    presentation_state_ = PresentationState::kCollapsed;
    owned_view_ = CreateToolbarView();
    view = owned_view_.get();
    view_tracker_.SetView(view);
  } else {
    // If we're recycling a pre-existing view, it might be in compact or
    // expanded mode. We must explicitly invoke SetPresentationState to reset
    // its layout and button states back to collapsed.
    SetPresentationState(PresentationState::kCollapsed);
    // ResetLayout immediately flushes the changes without waiting for
    // animation. This is needed to jump to the collapsed state immediately
    // since the view might be animating from a previous incarnation.
    views::View* animating_container =
        view->GetViewByElementId(kAnimatingContainerElementId);
    CHECK(animating_container);
    static_cast<views::AnimatingLayoutManager*>(
        animating_container->GetLayoutManager())
        ->ResetLayout();
  }

  // Default properties: initially unanchored with default corner offset.
  view->SetProperty(kIndigoTrackedElementRectKey, gfx::Rect());
  view->SetProperty(
      kIndigoToolbarCornerOffsetKey,
      gfx::Vector2d(-kToolbarInitialOffset, kToolbarInitialOffset));

  view->SetVisible(false);
  auto_compact_timer_.Stop();

  if (!parent_view) {
    // View is created and properties are stored in owned_view_, but cannot be
    // shown yet.
    return;
  }

  if (owned_view_) {
    DCHECK(parent_view->children().empty());
    parent_view->AddChildView(std::move(owned_view_));
    parent_view->InvalidateLayout();
  } else {
    view->InvalidateLayout();
  }
}

void IndigoToolbar::Hide() {
  auto_compact_timer_.Stop();
  views::View* view = view_tracker_.view();
  view_tracker_.SetView(nullptr);
  if (view && view->parent()) {
    views::View* parent = view->parent();
    parent->RemoveChildViewT(view);
    parent->InvalidateLayout();
  }
  owned_view_.reset();
}

void IndigoToolbar::TabWillBecomeHidden() {
  views::View* view = view_tracker_.view();
  CHECK(view);
  views::View* parent = view->parent();
  if (parent) {
    owned_view_ = parent->RemoveChildViewT(view);
    parent->InvalidateLayout();
  }
  auto_compact_timer_.Stop();
}

void IndigoToolbar::TabDidBecomeVisible(views::View* parent_view) {
  if (owned_view_) {
    DCHECK(parent_view->children().empty());
    parent_view->AddChildView(std::move(owned_view_));
    parent_view->InvalidateLayout();
    StartAutoCompactTimerIfNeeded(kInitialAutoCompactDelay);
  }
}

void IndigoToolbar::OnToolbarInteractionChanged(bool interacting) {
  is_interacting_ = interacting;
  if (interacting) {
    auto_compact_timer_.Stop();
  } else {
    StartAutoCompactTimerIfNeeded(kInteractionAutoCompactDelay);
  }
}

void IndigoToolbar::OnExpandButtonInteractionChanged(bool interacting) {
  if (interacting && presentation_state_ == PresentationState::kCompact) {
    SetPresentationState(PresentationState::kCollapsed);
  }
}

void IndigoToolbar::OnCloseButtonClicked() {
  base::RecordAction(base::UserMetricsAction("Indigo.Toolbar.Close"));
  Hide();
  if (delegate_) {
    delegate_->OnClose(this);
  }
}

void IndigoToolbar::OnExpandButtonClicked() {
  base::RecordAction(base::UserMetricsAction("Indigo.Toolbar.Expand"));
  views::View* view = view_tracker_.view();
  if (!view) {
    return;
  }

  if (presentation_state_ == PresentationState::kExpanded) {
    SetPresentationState(PresentationState::kCollapsed);
  } else {
    SetPresentationState(PresentationState::kExpanded);
  }
}

void IndigoToolbar::OnRegenerateButtonClicked() {
  SetPresentationState(PresentationState::kCollapsed, kInitialAutoCompactDelay);
  delegate_->OnRegenerate(this);
}

void IndigoToolbar::OnReplacePhotoClicked() {
  SetPresentationState(PresentationState::kCollapsed, kInitialAutoCompactDelay);
  delegate_->OnReplaceOriginalPhoto(this);
}

void IndigoToolbar::OnDeletePhotoClicked() {
  SetPresentationState(PresentationState::kCollapsed, kInitialAutoCompactDelay);
  delegate_->OnDeleteOriginalPhoto(this);
}

void IndigoToolbar::UpdateTrackedPosition(const gfx::Rect& rect) {
  views::View* view = view_tracker_.view();
  if (!view) {
    return;
  }

  const bool was_visible = view->GetVisible();
  const bool should_be_visible = !rect.IsEmpty();
  view->SetProperty(kIndigoTrackedElementRectKey, rect);
  view->SetVisible(should_be_visible);

  if (!should_be_visible) {
    auto_compact_timer_.Stop();
  } else if (!was_visible) {
    SetPresentationState(presentation_state_ == PresentationState::kExpanded
                             ? PresentationState::kExpanded
                             : PresentationState::kCollapsed,
                         kInitialAutoCompactDelay);
  }

  if (view->parent()) {
    view->parent()->InvalidateLayout();
  }
}

void IndigoToolbar::OnAutoCompactTimer() {
  if (IsToolbarReadyForAutoCompact()) {
    SetPresentationState(PresentationState::kCompact);
  }
}

void IndigoToolbar::StartAutoCompactTimerIfNeeded(base::TimeDelta delay) {
  if (!IsToolbarReadyForAutoCompact()) {
    return;
  }

  auto_compact_timer_.Start(FROM_HERE, delay, this,
                            &IndigoToolbar::OnAutoCompactTimer);
}

void IndigoToolbar::SetPresentationState(PresentationState state,
                                         base::TimeDelta auto_compact_delay) {
  views::View* view = view_tracker_.view();
  if (!view) {
    presentation_state_ = state;
    return;
  }

  const PresentationState previous_state = presentation_state_;
  presentation_state_ = state;

  const bool is_compact = state == PresentationState::kCompact;
  const bool is_expanded = state == PresentationState::kExpanded;

  auto* expand_button = views::AsViewClass<IndigoExpandButton>(
      view->GetViewByElementId(kExpandButtonElementId));
  CHECK(expand_button);
  expand_button->SetCompact(is_compact);
  expand_button->SetExpanded(is_expanded);

  views::View* expanded_container =
      view->GetViewByElementId(kExpandedContainerElementId);
  CHECK(expanded_container);

  if (is_expanded || is_compact || is_interacting_) {
    auto_compact_timer_.Stop();
  } else {
    StartAutoCompactTimerIfNeeded(auto_compact_delay);
  }

  views::View* animating_container =
      view->GetViewByElementId(kAnimatingContainerElementId);
  CHECK(animating_container);
  auto* animating_layout = static_cast<views::AnimatingLayoutManager*>(
      animating_container->GetLayoutManager());

  // FadeIn and FadeOut will automatically handle the visibility of the internal
  // child while animating the bounds of the outer container.
  const bool was_expanded = (previous_state == PresentationState::kExpanded);
  if (is_expanded != was_expanded) {
    if (is_expanded) {
      animating_layout->FadeIn(expanded_container);
    } else {
      animating_layout->FadeOut(expanded_container);
    }
  }
}

bool IndigoToolbar::IsToolbarReadyForAutoCompact() const {
  const views::View* view = view_tracker_.view();
  if (!view || !view->IsDrawn()) {
    return false;
  }

  return !is_interacting_ &&
         presentation_state_ == PresentationState::kCollapsed;
}

}  // namespace indigo
