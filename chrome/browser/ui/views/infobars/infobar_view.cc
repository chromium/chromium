// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/infobars/infobar_view.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/infobars/core/infobar_delegate.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "third_party/skia/include/effects/SkGradientShader.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/class_property.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/theme_provider.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/native_theme/common_theme.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/button/label_button_border.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/metadata/metadata_impl_macros.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/non_client_view.h"

// Helpers --------------------------------------------------------------------

// Used to mark children that are Labels, so we can update their background
// colors on a theme change.
enum class LabelType {
  kNone,
  kLabel,
  kLink,
};
DEFINE_UI_CLASS_PROPERTY_TYPE(LabelType)

namespace {

DEFINE_UI_CLASS_PROPERTY_KEY(LabelType, kLabelType, LabelType::kNone)

// IDs of the colors to use for infobar elements.
constexpr int kInfoBarLabelBackgroundColor = ThemeProperties::COLOR_INFOBAR;
constexpr int kInfoBarLabelTextColor = ThemeProperties::COLOR_BOOKMARK_TEXT;

bool SortViewsByDecreasingWidth(views::View* view_1, views::View* view_2) {
  return view_1->GetPreferredSize().width() >
         view_2->GetPreferredSize().width();
}

int GetElementSpacing() {
  return ChromeLayoutProvider::Get()->GetDistanceMetric(
      DISTANCE_UNRELATED_CONTROL_HORIZONTAL);
}

gfx::Insets GetCloseButtonSpacing() {
  auto* provider = ChromeLayoutProvider::Get();
  const gfx::Insets vector_button_insets =
      provider->GetInsetsMetric(views::INSETS_VECTOR_IMAGE_BUTTON);
  return gfx::Insets(
             provider->GetDistanceMetric(DISTANCE_TOAST_CONTROL_VERTICAL),
             GetElementSpacing()) -
         vector_button_insets;
}

}  // namespace


// InfoBarView ----------------------------------------------------------------

InfoBarView::InfoBarView(std::unique_ptr<infobars::InfoBarDelegate> delegate)
    : infobars::InfoBar(std::move(delegate)),
      views::ExternalFocusTracker(this, nullptr) {
  // Make Infobar animation aligned to the Compositor.
  SetNotifier(std::make_unique<
              gfx::AnimationDelegateNotifier<views::AnimationDelegateViews>>(
      this, this));

  set_owned_by_client();  // InfoBar deletes itself at the appropriate time.

  // Clip child layers; without this, buttons won't look correct during
  // animation.
  SetPaintToLayer();
  layer()->SetMasksToBounds(true);

  gfx::Image image = this->delegate()->GetIcon();
  if (!image.IsEmpty()) {
    icon_ = new views::ImageView;
    icon_->SetImage(image.ToImageSkia());
    icon_->SizeToPreferredSize();
    icon_->SetProperty(
        views::kMarginsKey,
        gfx::Insets(ChromeLayoutProvider::Get()->GetDistanceMetric(
                        DISTANCE_TOAST_LABEL_VERTICAL),
                    0));
    AddChildView(icon_);
  }

  if (this->delegate()->IsCloseable()) {
    auto close_button = views::CreateVectorImageButton(base::BindRepeating(
        &InfoBarView::CloseButtonPressed, base::Unretained(this)));
    // This is the wrong color, but allows the button's size to be computed
    // correctly.  We'll reset this with the correct color in OnThemeChanged().
    views::SetImageFromVectorIcon(close_button.get(),
                                  vector_icons::kCloseRoundedIcon,
                                  gfx::kPlaceholderColor);
    close_button->SetAccessibleName(
        l10n_util::GetStringUTF16(IDS_ACCNAME_CLOSE));
    gfx::Insets close_button_spacing = GetCloseButtonSpacing();
    close_button->SetProperty(views::kMarginsKey,
                              gfx::Insets(close_button_spacing.top(), 0,
                                          close_button_spacing.bottom(), 0));
    close_button_ = AddChildView(std::move(close_button));
  }
}

InfoBarView::~InfoBarView() {
  // We should have closed any open menus in PlatformSpecificHide(), then
  // subclasses' RunMenu() functions should have prevented opening any new ones
  // once we became unowned.
  DCHECK(!menu_runner_.get());
}

void InfoBarView::RecalculateHeight() {
  // Ensure the infobar is tall enough to display its contents.
  int height = 0;
  for (View* child : children()) {
    const gfx::Insets* const margins = child->GetProperty(views::kMarginsKey);
    const int margin_height = margins ? margins->height() : 0;
    height = std::max(height, child->height() + margin_height);
  }
  SetTargetHeight(height + GetSeparatorHeight());
}

void InfoBarView::Layout() {
  const int spacing = GetElementSpacing();
  int start_x = 0;
  if (icon_) {
    icon_->SetPosition(gfx::Point(spacing, OffsetY(icon_)));
    start_x = icon_->bounds().right();
  }

  const int content_minimum_width = GetContentMinimumWidth();
  if (content_minimum_width > 0)
    start_x += spacing + content_minimum_width;

  if (close_button_) {
    const gfx::Insets close_button_spacing = GetCloseButtonSpacing();
    close_button_->SizeToPreferredSize();
    close_button_->SetPosition(gfx::Point(
        std::max(
            start_x + close_button_spacing.left(),
            width() - close_button_spacing.right() - close_button_->width()),
        OffsetY(close_button_)));

    // For accessibility reasons, the close button should come last.
    DCHECK_EQ(close_button_, close_button_->parent()->children().back());
  }
}

void InfoBarView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  node_data->SetName(l10n_util::GetStringUTF8(IDS_ACCNAME_INFOBAR));
  node_data->role = ax::mojom::Role::kAlertDialog;
  node_data->AddStringAttribute(ax::mojom::StringAttribute::kKeyShortcuts,
                                "Alt+Shift+A");
}

gfx::Size InfoBarView::CalculatePreferredSize() const {
  int width = 0;

  const int spacing = GetElementSpacing();
  if (icon_)
    width += spacing + icon_->width();

  const int content_width = GetContentMinimumWidth();
  if (content_width)
    width += spacing + content_width;

  const int trailing_space =
      close_button_ ? GetCloseButtonSpacing().width() + close_button_->width()
                    : GetElementSpacing();
  return gfx::Size(width + trailing_space, computed_height());
}

void InfoBarView::ViewHierarchyChanged(
    const views::ViewHierarchyChangedDetails& details) {
  View::ViewHierarchyChanged(details);

  // Anything that needs to happen once after all subclasses add their children.
  if (details.is_add && (details.child == this)) {
    if (close_button_)
      ReorderChildView(close_button_, -1);
    RecalculateHeight();
  }
}

void InfoBarView::OnPaint(gfx::Canvas* canvas) {
  views::View::OnPaint(canvas);

  if (GetDrawSeparator()) {
    const SkColor color =
        GetColor(ThemeProperties::COLOR_TOOLBAR_CONTENT_AREA_SEPARATOR);
    const gfx::Rect local_bounds = GetLocalBounds();
    canvas->DrawSharpLine({local_bounds.x(), local_bounds.y()},
                          {local_bounds.right(), local_bounds.y()}, color);
  }
}

void InfoBarView::OnThemeChanged() {
  views::View::OnThemeChanged();
  const SkColor background_color = GetColor(kInfoBarLabelBackgroundColor);
  SetBackground(views::CreateSolidBackground(background_color));

  const SkColor text_color = GetColor(kInfoBarLabelTextColor);
  if (close_button_) {
    views::SetImageFromVectorIcon(close_button_,
                                  vector_icons::kCloseRoundedIcon, text_color);
  }

  for (views::View* child : children()) {
    LabelType label_type = child->GetProperty(kLabelType);
    if (label_type != LabelType::kNone) {
      auto* label = static_cast<views::Label*>(child);
      label->SetBackgroundColor(background_color);
      if (label_type == LabelType::kLabel)
        label->SetEnabledColor(text_color);
    }
  }

  // Native theme changes can affect font sizes.
  RecalculateHeight();
}

void InfoBarView::OnWillChangeFocus(View* focused_before, View* focused_now) {
  views::ExternalFocusTracker::OnWillChangeFocus(focused_before, focused_now);

  // This will trigger some screen readers to read the entire contents of this
  // infobar.
  if (focused_before && focused_now && !Contains(focused_before) &&
      Contains(focused_now)) {
    NotifyAccessibilityEvent(ax::mojom::Event::kAlert, true);
  }
}

views::Label* InfoBarView::CreateLabel(const std::u16string& text) const {
  views::Label* label =
      new views::Label(text, views::style::CONTEXT_DIALOG_BODY_TEXT);
  SetLabelDetails(label);
  label->SetEnabledColor(GetColor(kInfoBarLabelTextColor));
  label->SetProperty(kLabelType, LabelType::kLabel);
  return label;
}

views::Link* InfoBarView::CreateLink(const std::u16string& text) {
  views::Link* link =
      new views::Link(text, views::style::CONTEXT_DIALOG_BODY_TEXT);
  SetLabelDetails(link);
  link->SetCallback(
      base::BindRepeating(&InfoBarView::LinkClicked, base::Unretained(this)));
  link->SetProperty(kLabelType, LabelType::kLink);
  return link;
}

// static
void InfoBarView::AssignWidths(Views* views, int available_width) {
  std::sort(views->begin(), views->end(), SortViewsByDecreasingWidth);
  AssignWidthsSorted(views, available_width);
}

int InfoBarView::GetContentMinimumWidth() const {
  return 0;
}

int InfoBarView::GetStartX() const {
  // Ensure we don't return a value greater than GetEndX(), so children can
  // safely set something's width to "GetEndX() - GetStartX()" without risking
  // that being negative.
  return std::min((icon_ ? icon_->bounds().right() : 0) + GetElementSpacing(),
                  GetEndX());
}

int InfoBarView::GetEndX() const {
  return close_button_ ? close_button_->x() - GetCloseButtonSpacing().left()
                       : width() - GetElementSpacing();
}

int InfoBarView::OffsetY(views::View* view) const {
  return GetSeparatorHeight() +
         std::max((target_height() - view->height()) / 2, 0) -
         (target_height() - height());
}

void InfoBarView::PlatformSpecificShow(bool animate) {
  // If we gain focus, we want to restore it to the previously-focused element
  // when we're hidden. So when we're in a Widget, create a focus tracker so
  // that if we gain focus we'll know what the previously-focused element was.
  SetFocusManager(GetFocusManager());

  NotifyAccessibilityEvent(ax::mojom::Event::kAlert, true);
}

void InfoBarView::PlatformSpecificHide(bool animate) {
  // Cancel any menus we may have open.  It doesn't make sense to leave them
  // open while we're hidden, and if we're going to become unowned, we can't
  // allow the user to choose any options and potentially call functions that
  // try to access the owner.
  menu_runner_.reset();

  // It's possible to be called twice (once with |animate| true and once with it
  // false); in this case the second SetFocusManager() call will silently no-op.
  SetFocusManager(NULL);

  if (!animate)
    return;

  // Do not restore focus (and active state with it) if some other top-level
  // window became active.
  views::Widget* widget = GetWidget();
  if (!widget || widget->IsActive())
    FocusLastFocusedExternalView();
}

void InfoBarView::PlatformSpecificOnHeightRecalculated() {
  // Ensure that notifying our container of our size change will result in a
  // re-layout.
  InvalidateLayout();
}

// static
void InfoBarView::AssignWidthsSorted(Views* views, int available_width) {
  if (views->empty())
    return;
  gfx::Size back_view_size(views->back()->GetPreferredSize());
  back_view_size.set_width(
      std::min(back_view_size.width(),
               available_width / static_cast<int>(views->size())));
  views->back()->SetSize(back_view_size);
  views->pop_back();
  AssignWidthsSorted(views, available_width - back_view_size.width());
}

bool InfoBarView::GetDrawSeparator() const {
  // There will be no parent when this infobar is not in a container, e.g. if
  // it's in a background tab.  It's still possible to reach here in that case,
  // e.g. if ElevationIconSetter triggers a Layout().
  return parent() && parent()->children().front() != this;
}

int InfoBarView::GetSeparatorHeight() const {
  // We only need a separator for infobars after the first; the topmost infobar
  // uses the toolbar as its top separator.
  //
  // This only works because all infobars have padding at the top; if we
  // actually draw all the way to the top, we'd risk drawing a separator atop
  // some infobar content.
  return GetDrawSeparator() ? 1 : 0;
}

SkColor InfoBarView::GetColor(int id) const {
  const auto* theme_provider = GetThemeProvider();
  // When there's no theme provider, this color will never be used; it will be
  // reset due to the OnThemeChanged() override.
  return theme_provider ? theme_provider->GetColor(id) : gfx::kPlaceholderColor;
}

void InfoBarView::SetLabelDetails(views::Label* label) const {
  label->SizeToPreferredSize();
  label->SetBackgroundColor(GetColor(kInfoBarLabelBackgroundColor));
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label->SetProperty(views::kMarginsKey,
                     gfx::Insets(ChromeLayoutProvider::Get()->GetDistanceMetric(
                                     DISTANCE_TOAST_LABEL_VERTICAL),
                                 0));
}

void InfoBarView::LinkClicked(const ui::Event& event) {
  if (!owner())
    return;  // We're closing; don't call anything, it might access the owner.
  if (delegate()->LinkClicked(ui::DispositionFromEventFlags(event.flags())))
    RemoveSelf();
}

void InfoBarView::CloseButtonPressed() {
  if (!owner())
    return;  // We're closing; don't call anything, it might access the owner.
  delegate()->InfoBarDismissed();
  RemoveSelf();
}

BEGIN_METADATA(InfoBarView, views::View)
ADD_READONLY_PROPERTY_METADATA(int, ContentMinimumWidth)
ADD_READONLY_PROPERTY_METADATA(int, StartX)
ADD_READONLY_PROPERTY_METADATA(int, EndX)
ADD_READONLY_PROPERTY_METADATA(bool, DrawSeparator)
ADD_READONLY_PROPERTY_METADATA(int, SeparatorHeight)
END_METADATA
