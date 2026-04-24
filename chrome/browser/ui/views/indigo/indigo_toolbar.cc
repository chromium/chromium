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
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
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
#include "ui/views/widget/widget.h"

namespace indigo {

namespace {

// Dimensions and distances (in dip).
constexpr int kToolbarPadding = 4;
constexpr int kToolbarInitialOffset = 20;
constexpr int kSeparatorHorizontalPadding = 8;
constexpr int kFeatureIconSize = 12;
constexpr int kControlIconSize = 16;
constexpr int kActionIconSize = 20;

}  // namespace

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(IndigoToolbar, kToolbarElementId);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(IndigoToolbar, kCloseButtonElementId);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(IndigoToolbar, kExpandButtonElementId);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(IndigoToolbar,
                                      kRegenerateButtonElementId);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(IndigoToolbar,
                                      kReplacePhotoButtonElementId);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(IndigoToolbar,
                                      kDeletePhotoButtonElementId);

IndigoToolbar::IndigoToolbar(Delegate* delegate) : delegate_(delegate) {}

IndigoToolbar::~IndigoToolbar() {
  if (widget_) {
    // Since we own the widget, we should close it if we're being destroyed.
    // This will trigger OnWidgetClosed, where the widget is destroyed, and
    // also notify the delegate (which will need to be prepared for this).
    widget_->Close();
  }
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
                          .CopyAddressTo(&expand_button_)
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
                  .CopyAddressTo(&expanded_container_)
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
    gfx::NativeView parent_view,
    base::FunctionRef<gfx::Point(const gfx::Size&)> toolbar_origin_func) {
  if (!widget_) {
    widget_ = std::make_unique<views::Widget>();
    views::Widget::InitParams params(
        views::Widget::InitParams::CLIENT_OWNS_WIDGET,
        views::Widget::InitParams::TYPE_CONTROL);
    params.name = "IndigoToolbar";
    params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
    params.shadow_type = views::Widget::InitParams::ShadowType::kNone;
    params.parent = parent_view;
    params.activatable = views::Widget::InitParams::Activatable::kNo;
    params.accept_events = true;

    widget_->Init(std::move(params));
    widget_->MakeCloseSynchronous(
        base::BindOnce(&IndigoToolbar::OnWidgetClosed, base::Unretained(this)));
    widget_->SetContentsView(CreateToolbarView());
  }

  gfx::Insets insets = widget_->GetContentsView()->GetInsets();
  gfx::Size preferred_size = widget_->GetContentsView()->GetPreferredSize();
  gfx::Size content_size = preferred_size;
  content_size.Enlarge(-insets.width(), -insets.height());
  gfx::Point point = toolbar_origin_func(content_size);

  gfx::Rect widget_bounds(point, preferred_size);

  // Shift the widget origin so the content area (inside the shadow) starts at
  // the target point.
  widget_bounds.Offset(-insets.left(), -insets.top());

  widget_->SetBounds(widget_bounds);
  widget_->Show();
}

void IndigoToolbar::Show(gfx::NativeView parent_view) {
  ShowAt(parent_view, [](const gfx::Size& size) {
    return gfx::Point(kToolbarInitialOffset, kToolbarInitialOffset);
  });
}

void IndigoToolbar::ShowInside(gfx::NativeView parent_view,
                               const gfx::Rect& rect) {
  ShowAt(parent_view, [rect](const gfx::Size& size) {
    return gfx::Point(rect.right() - size.width() - kToolbarInitialOffset,
                      rect.y() + kToolbarInitialOffset);
  });
}

void IndigoToolbar::Hide() {
  if (widget_) {
    widget_->Close();
  }
}

void IndigoToolbar::OnCloseButtonClicked() {
  Hide();
}

void IndigoToolbar::OnExpandButtonClicked() {
  is_expanded_ = !is_expanded_;
  expand_button_->SetToggled(is_expanded_);
  expanded_container_->SetVisible(is_expanded_);

  gfx::Size preferred_size = widget_->GetContentsView()->GetPreferredSize();
  widget_->SetSize(preferred_size);
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

void IndigoToolbar::OnWidgetClosed(views::Widget::ClosedReason reason) {
  expand_button_ = nullptr;
  expanded_container_ = nullptr;

  // As recommended in the comment on `views::Widget::MakeCloseSynchronous`,
  // destroy the widget here.
  widget_.reset();

  delegate_->OnClose(this);
}

}  // namespace indigo
