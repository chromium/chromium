// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/indigo/indigo_toolbar.h"

#include <memory>
#include <utility>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/functional/function_ref.h"
#include "base/logging.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/indigo/resources/grit/indigo_strings.h"
#include "chrome/browser/ui/views/controls/hover_button.h"
#include "chrome/browser/ui/views/frame/contents_container_view.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
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
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/layout/layout_manager_base.h"
#include "ui/views/metadata/view_factory.h"
#include "ui/views/vector_icons.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_targeter.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"

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

// Radius constants.
constexpr int kToolbarCornerRadius = 12;
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
      gfx::Point top_right = tracked_rect->top_right();
      gfx::Point toolbar_top_right = top_right + *corner_offset;
      return gfx::Point(
          toolbar_top_right.x() - preferred_size.width() + insets.right(),
          toolbar_top_right.y() - insets.top());
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

class IndigoExpandButton : public HoverButton {
  METADATA_HEADER(IndigoExpandButton, HoverButton)

 public:
  explicit IndigoExpandButton(PressedCallback callback, bool is_expanded)
      : HoverButton(std::move(callback),
                    CreateExpandButtonParams(is_expanded)) {
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

    chevron_ = views::AsViewClass<views::ImageView>(secondary_view());
    if (chevron_) {
      chevron_->SetProperty(
          views::kMarginsKey,
          gfx::Insets::TLBR(0, kExpandButtonIconLabelSpacing, 0, 0));
    }
    is_expanded_ = is_expanded;
    UpdateState();
  }

  IndigoExpandButton(const IndigoExpandButton&) = delete;
  IndigoExpandButton& operator=(const IndigoExpandButton&) = delete;
  ~IndigoExpandButton() override = default;

  void SetExpanded(bool expanded) {
    CHECK_NE(expanded, is_expanded_);
    is_expanded_ = expanded;
    UpdateState();
  }

 private:
  static HoverButton::Params CreateExpandButtonParams(bool is_expanded) {
    HoverButton::Params params;
    params.title = l10n_util::GetStringUTF16(IDS_INDIGO_TOOLBAR_CAPTION);
    params.secondary_view = CreateChevronView(is_expanded);
    params.add_vertical_label_spacing = false;
    return params;
  }

  static std::unique_ptr<views::ImageView> CreateChevronView(bool is_expanded) {
    auto chevron = std::make_unique<views::ImageView>();
    chevron->SetProperty(views::kElementIdentifierKey,
                         IndigoToolbar::kChevronElementId);
    chevron->SetImage(GetChevronImageModel(is_expanded));
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

    if (chevron_) {
      chevron_->SetImage(GetChevronImageModel(is_expanded_));
    }
  }

  bool is_expanded_ = false;
  raw_ptr<views::ImageView> chevron_ = nullptr;
};

BEGIN_METADATA(IndigoExpandButton)
END_METADATA

}  // namespace

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(IndigoToolbar, kToolbarElementId);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(IndigoToolbar, kCloseButtonElementId);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(IndigoToolbar, kExpandButtonElementId);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(IndigoToolbar, kChevronElementId);
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

  auto view =
      views::Builder<views::FlexLayoutView>()
          .SetProperty(views::kElementIdentifierKey, kToolbarElementId)
          .SetPaintToLayer()
          .CustomConfigure(base::BindOnce([](views::View* view) {
            view->layer()->SetFillsBoundsOpaquely(false);
            view->layer()->SetName("IndigoToolbar");
            view->GetViewAccessibility().SetRole(ax::mojom::Role::kToolbar);
            view->GetViewAccessibility().SetName(
                l10n_util::GetStringUTF16(IDS_INDIGO_TOOLBAR_CAPTION));
          }))
          .SetBackground(
              std::make_unique<views::BubbleBackground>(bubble_border.get()))
          .SetBorder(std::move(bubble_border))
          .SetOrientation(views::LayoutOrientation::kVertical)
          .SetMainAxisAlignment(views::LayoutAlignment::kStart)
          .SetCrossAxisAlignment(views::LayoutAlignment::kStretch)
          .SetDefault(views::kMarginsKey, gfx::Insets())
          .SetCollapseMargins(true)
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
                              is_expanded_))
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
                          .CustomConfigure(
                              base::BindOnce([](views::ImageButton* button) {
                                // CreateVectorImageButton doesn't set the
                                // ink drop base color automatically.
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
  return button;
}

void IndigoToolbar::Show(views::View* parent_view) {
  views::View* view = view_tracker_.view();
  if (!view) {
    owned_view_ = CreateToolbarView();
    view = owned_view_.get();
    view_tracker_.SetView(view);
  }

  // Default properties: initially unanchored with default corner offset.
  view->SetProperty(kIndigoTrackedElementRectKey, gfx::Rect());
  view->SetProperty(
      kIndigoToolbarCornerOffsetKey,
      gfx::Vector2d(-kToolbarInitialOffset, kToolbarInitialOffset));

  view->SetVisible(false);

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
}

void IndigoToolbar::TabDidBecomeVisible(views::View* parent_view) {
  if (owned_view_) {
    DCHECK(parent_view->children().empty());
    parent_view->AddChildView(std::move(owned_view_));
    parent_view->InvalidateLayout();
  }
}

void IndigoToolbar::OnCloseButtonClicked() {
  Hide();
  if (delegate_) {
    delegate_->OnClose(this);
  }
}

void IndigoToolbar::OnExpandButtonClicked() {
  views::View* view = view_tracker_.view();
  if (!view) {
    return;
  }

  is_expanded_ = !is_expanded_;

  auto* expand_button = views::AsViewClass<IndigoExpandButton>(
      view->GetViewByElementId(kExpandButtonElementId));
  if (expand_button) {
    expand_button->SetExpanded(is_expanded_);
  }

  auto* expanded_container =
      view->GetViewByElementId(kExpandedContainerElementId);
  if (expanded_container) {
    expanded_container->SetVisible(is_expanded_);
  }
}

void IndigoToolbar::OnRegenerateButtonClicked() {
  delegate_->OnRegenerate(this);
}

void IndigoToolbar::OnReplacePhotoClicked() {
  delegate_->OnReplaceOriginalPhoto(this);
}

void IndigoToolbar::OnDeletePhotoClicked() {
  delegate_->OnDeleteOriginalPhoto(this);
}

void IndigoToolbar::UpdateTrackedPosition(const gfx::Rect& rect) {
  views::View* view = view_tracker_.view();
  if (view) {
    view->SetVisible(!rect.IsEmpty());
    view->SetProperty(kIndigoTrackedElementRectKey, rect);
    if (view->parent()) {
      view->parent()->InvalidateLayout();
    }
  }
}

}  // namespace indigo
