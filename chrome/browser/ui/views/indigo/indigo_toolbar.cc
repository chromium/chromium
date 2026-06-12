// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/indigo/indigo_toolbar.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/function_ref.h"
#include "base/logging.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/indigo/resources/grit/indigo_strings.h"
#include "chrome/browser/ui/views/controls/hover_button.h"
#include "chrome/browser/ui/views/frame/contents_container_view.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/ui_base_features.h"
#include "ui/color/color_id.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/background.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
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

DEFINE_UI_CLASS_PROPERTY_TYPE(gfx::Point*)

namespace indigo {

DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(gfx::Rect, kIndigoTrackedElementRectKey)
DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(gfx::Vector2d, kIndigoToolbarCornerOffsetKey)

namespace {

// Dimensions and distances (in dip).
constexpr int kToolbarPadding = 4;
constexpr int kToolbarInitialOffset = 20;
constexpr int kSeparatorHorizontalPadding = 8;
constexpr int kControlIconSize = 16;
constexpr int kActionIconSize = 20;
constexpr int kLabelLeftMargin = 12;

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

}  // namespace

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(IndigoToolbar, kToolbarElementId);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(IndigoToolbar, kCloseButtonElementId);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(IndigoToolbar, kExpandButtonElementId);
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
      gfx::RoundedCornersF(views::LayoutProvider::Get()->GetCornerRadiusMetric(
          views::Emphasis::kHigh)));

  auto view =
      views::Builder<views::FlexLayoutView>()
          .SetProperty(views::kElementIdentifierKey, kToolbarElementId)
          .SetPaintToLayer()
          .CustomConfigure(base::BindOnce([](views::View* view) {
            view->layer()->SetFillsBoundsOpaquely(false);
            view->layer()->SetName("IndigoToolbar");
          }))
          .SetBackground(
              std::make_unique<views::BubbleBackground>(bubble_border.get()))
          .SetBorder(std::move(bubble_border))
          .SetOrientation(views::LayoutOrientation::kVertical)
          .SetMainAxisAlignment(views::LayoutAlignment::kStart)
          .SetCrossAxisAlignment(views::LayoutAlignment::kStretch)
          .SetInteriorMargin(gfx::Insets(kToolbarPadding))
          .SetDefault(views::kMarginsKey, gfx::Insets(kToolbarPadding))
          .SetCollapseMargins(true)
          .AddChildren(
              // Top row: Always visible
              views::Builder<views::FlexLayoutView>()
                  .SetOrientation(views::LayoutOrientation::kHorizontal)
                  .SetMainAxisAlignment(views::LayoutAlignment::kEnd)
                  .SetCrossAxisAlignment(views::LayoutAlignment::kStretch)
                  .SetInteriorMargin(gfx::Insets(kToolbarPadding))
                  .SetDefault(views::kMarginsKey, gfx::Insets(kToolbarPadding))
                  .SetCollapseMargins(true)
                  .AddChildren(
                      views::Builder<views::Label>()
                          .SetProperty(
                              views::kMarginsKey,
                              gfx::Insets::TLBR(0, kLabelLeftMargin, 0, 0))
                          .SetText(l10n_util::GetStringUTF16(
                              IDS_INDIGO_TOOLBAR_CAPTION))
                          .SetTextContext(views::style::CONTEXT_LABEL)
                          .SetTextStyle(views::style::STYLE_SECONDARY)
                          .SetEnabledColor(ui::kColorSysOnSurfaceSubtle)
                          .SetHorizontalAlignment(gfx::ALIGN_LEFT)
                          .SetProperty(
                              views::kFlexBehaviorKey,
                              views::FlexSpecification(
                                  views::MinimumFlexSizeRule::kPreferred,
                                  views::MaximumFlexSizeRule::kUnbounded)),
                      views::Builder<views::ToggleImageButton>(
                          views::CreateVectorToggleImageButton(
                              base::BindRepeating(
                                  &IndigoToolbar::OnExpandButtonClicked,
                                  base::Unretained(this))))
                          .SetProperty(views::kElementIdentifierKey,
                                       kExpandButtonElementId)
                          .SetImageModel(
                              views::Button::STATE_NORMAL,
                              ui::ImageModel::FromVectorIcon(
                                  features::IsRoundedIconsEnabled()
                                      ? vector_icons::kKeyboardArrowDownIcon
                                      : vector_icons::kCaretDownOldIcon,
                                  ui::kColorSysOnSurfaceSubtle,
                                  kControlIconSize))
                          .SetToggledImageModel(
                              views::Button::STATE_NORMAL,
                              ui::ImageModel::FromVectorIcon(
                                  features::IsRoundedIconsEnabled()
                                      ? vector_icons::kKeyboardArrowUpIcon
                                      : vector_icons::kCaretUpOldIcon,
                                  ui::kColorSysOnSurfaceSubtle,
                                  kControlIconSize))
                          .SetTooltipText(l10n_util::GetStringUTF16(
                              IDS_INDIGO_TOOLBAR_EXPAND))
                          .SetToggledTooltipText(l10n_util::GetStringUTF16(
                              IDS_INDIGO_TOOLBAR_COLLAPSE))
                          .CustomConfigure(base::BindOnce(
                              [](views::ToggleImageButton* button) {
                                // CreateVectorToggleImageButton doesn't set the
                                // ink drop base color automatically.
                                views::InkDrop::Get(button)->SetBaseColor(
                                    ui::kColorSysOnSurfaceSubtle);
                              })),
                      views::Builder<views::Separator>()
                          .SetOrientation(
                              views::Separator::Orientation::kVertical)
                          .SetColorId(ui::kColorSysSurfaceVariant)
                          .SetProperty(
                              views::kMarginsKey,
                              gfx::Insets::VH(kToolbarPadding,
                                              kSeparatorHorizontalPadding)),
                      views::Builder<views::ImageButton>(
                          views::CreateVectorImageButton(base::BindRepeating(
                              &IndigoToolbar::OnCloseButtonClicked,
                              base::Unretained(this))))
                          .SetProperty(views::kElementIdentifierKey,
                                       kCloseButtonElementId)
                          .SetImageModel(views::Button::STATE_NORMAL,
                                         ui::ImageModel::FromVectorIcon(
                                             features::IsRoundedIconsEnabled()
                                                 ? vector_icons::kCloseIcon
                                                 : vector_icons::kCloseOldIcon,
                                             ui::kColorSysOnSurfaceSubtle,
                                             kControlIconSize))
                          .SetTooltipText(l10n_util::GetStringUTF16(IDS_CLOSE))
                          .CustomConfigure(
                              base::BindOnce([](views::ImageButton* button) {
                                // CreateVectorToggleImageButton doesn't set the
                                // ink drop base color automatically.
                                views::InkDrop::Get(button)->SetBaseColor(
                                    ui::kColorSysOnSurfaceSubtle);
                              }))),
              // Expanded Row: Toggled visibility
              views::Builder<views::FlexLayoutView>()
                  .SetProperty(views::kElementIdentifierKey,
                               kExpandedContainerElementId)
                  .SetVisible(false)
                  .SetOrientation(views::LayoutOrientation::kVertical)
                  .SetCrossAxisAlignment(views::LayoutAlignment::kStretch)
                  .AddChildren(
                      views::Builder<views::Separator>()
                          .SetOrientation(
                              views::Separator::Orientation::kHorizontal)
                          .SetColorId(ui::kColorSysDivider)
                          .SetProperty(views::kMarginsKey,
                                       gfx::Insets::VH(kToolbarPadding, 0)),
                      views::Builder<views::Button>(
                          CreateExpandedButton(
                              l10n_util::GetStringUTF16(
                                  IDS_INDIGO_TOOLBAR_REGENERATE),
                              vector_icons::kReloadCustomIcon,
                              base::BindRepeating(
                                  &IndigoToolbar::OnRegenerateButtonClicked,
                                  base::Unretained(this))))
                          .SetProperty(views::kElementIdentifierKey,
                                       kRegenerateButtonElementId),
                      views::Builder<views::Separator>()
                          .SetOrientation(
                              views::Separator::Orientation::kHorizontal)
                          .SetColorId(ui::kColorSysDivider)
                          .SetProperty(views::kMarginsKey,
                                       gfx::Insets::VH(kToolbarPadding, 0)),
                      views::Builder<views::Button>(
                          CreateExpandedButton(
                              l10n_util::GetStringUTF16(
                                  IDS_INDIGO_TOOLBAR_REPLACE_ORIGINAL_PHOTO),
                              features::IsRoundedIconsEnabled()
                                  ? vector_icons::kPhotoFilledIcon
                                  : vector_icons::kPhotoOldIcon,
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
  return views::Builder<HoverButton>(
             std::make_unique<HoverButton>(
                 std::move(callback),
                 ui::ImageModel::FromVectorIcon(
                     icon, ui::kColorSysOnSurfaceVariant, kActionIconSize),
                 label))
      .SetTextColor(views::Button::STATE_NORMAL, ui::kColorSysOnSurfaceSubtle)
      .SetTextColor(views::Button::STATE_HOVERED, ui::kColorSysOnSurfaceSubtle)
      .SetTextColor(views::Button::STATE_PRESSED, ui::kColorSysOnSurfaceSubtle)
      .Build();
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

  auto* expand_button = views::AsViewClass<views::ToggleImageButton>(
      view->GetViewByElementId(kExpandButtonElementId));
  if (expand_button) {
    expand_button->SetToggled(is_expanded_);
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
