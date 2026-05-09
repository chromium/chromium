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
#include "chrome/browser/ui/views/controls/hover_button.h"
#include "chrome/browser/ui/views/frame/contents_container_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
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
#include "ui/views/metadata/view_factory.h"
#include "ui/views/vector_icons.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_targeter.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"

DEFINE_UI_CLASS_PROPERTY_TYPE(gfx::Point*)

namespace indigo {

DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(gfx::Point, kIndigoToolbarOffsetKey)

namespace {

// Dimensions and distances (in dip).
constexpr int kToolbarPadding = 4;
constexpr int kToolbarInitialOffset = 20;
constexpr int kSeparatorHorizontalPadding = 8;
constexpr int kFeatureIconSize = 12;
constexpr int kControlIconSize = 16;
constexpr int kActionIconSize = 20;

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

class IndigoOverlayLayoutManager : public views::LayoutManager {
 public:
  IndigoOverlayLayoutManager() = default;
  IndigoOverlayLayoutManager(const IndigoOverlayLayoutManager&) = delete;
  IndigoOverlayLayoutManager& operator=(const IndigoOverlayLayoutManager&) =
      delete;
  ~IndigoOverlayLayoutManager() override = default;

  void Layout(views::View* host) override {
    for (views::View* child : host->children()) {
      if (!child->GetVisible()) {
        continue;
      }
      const gfx::Point* offset = child->GetProperty(kIndigoToolbarOffsetKey);
      gfx::Size preferred_size = child->GetPreferredSize();
      if (offset) {
        child->SetBoundsRect(gfx::Rect(*offset, preferred_size));
      } else {
        child->SetBoundsRect(gfx::Rect(gfx::Point(), preferred_size));
      }
    }
  }

  gfx::Size GetPreferredSize(const views::View* host) const override {
    gfx::Rect bounds;
    for (const views::View* child : host->children()) {
      if (!child->GetVisible()) {
        continue;
      }
      const gfx::Point* offset = child->GetProperty(kIndigoToolbarOffsetKey);
      gfx::Point origin = offset ? *offset : gfx::Point();
      bounds.Union(gfx::Rect(origin, child->GetPreferredSize()));
    }
    return bounds.size();
  }

  gfx::Size GetPreferredSize(
      const views::View* host,
      const views::SizeBounds& available_size) const override {
    return GetPreferredSize(host);
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
                      views::Builder<views::ImageView>().SetImage(
                          ui::ImageModel::FromVectorIcon(
                              vector_icons::kChatSparkIcon,
                              ui::kColorSysOnSurfaceVariant, kFeatureIconSize)),
                      views::Builder<views::Label>()
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
                          .SetImageModel(views::Button::STATE_NORMAL,
                                         ui::ImageModel::FromVectorIcon(
                                             vector_icons::kCaretDownIcon,
                                             ui::kColorSysOnSurfaceSubtle,
                                             kControlIconSize))
                          .SetToggledImageModel(
                              views::Button::STATE_NORMAL,
                              ui::ImageModel::FromVectorIcon(
                                  vector_icons::kCaretUpIcon,
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
                                             vector_icons::kCloseIcon,
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
                              vector_icons::kReloadIcon,
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
                              vector_icons::kPhotoIcon,
                              base::BindRepeating(
                                  &IndigoToolbar::OnReplacePhotoClicked,
                                  base::Unretained(this))))
                          .SetProperty(views::kElementIdentifierKey,
                                       kReplacePhotoButtonElementId),
                      views::Builder<views::Button>(
                          CreateExpandedButton(
                              l10n_util::GetStringUTF16(
                                  IDS_INDIGO_TOOLBAR_DELETE_ORIGINAL_PHOTO),
                              kDeleteIcon,
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

void IndigoToolbar::ShowAt(
    views::View* parent_view,
    base::FunctionRef<gfx::Point(const gfx::Size&)> toolbar_origin_func) {
  views::View* view = view_tracker_.view();
  if (!view) {
    owned_view_ = CreateToolbarView();
    view = owned_view_.get();
    view_tracker_.SetView(view);
  }

  gfx::Insets insets = view->GetInsets();
  gfx::Size preferred_size = view->GetPreferredSize();
  gfx::Size content_size = preferred_size;
  content_size.Enlarge(-insets.width(), -insets.height());
  gfx::Point point = toolbar_origin_func(content_size);

  gfx::Point preferred_offset = point;
  preferred_offset.Offset(-insets.left(), -insets.top());
  view->SetProperty(kIndigoToolbarOffsetKey, preferred_offset);

  if (!parent_view) {
    // View is created and offset is stored in owned_view_, but cannot be shown
    // yet.
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

void IndigoToolbar::Show(views::View* parent_view) {
  ShowAt(parent_view, [](const gfx::Size& size) {
    return gfx::Point(kToolbarInitialOffset, kToolbarInitialOffset);
  });
}

void IndigoToolbar::ShowInside(views::View* parent_view,
                               const gfx::Rect& rect) {
  ShowAt(parent_view, [rect](const gfx::Size& size) {
    return gfx::Point(rect.right() - size.width() - kToolbarInitialOffset,
                      rect.y() + kToolbarInitialOffset);
  });
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

}  // namespace indigo
