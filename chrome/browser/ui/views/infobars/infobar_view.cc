// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/infobars/infobar_view.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
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
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/theme_provider.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/window_open_disposition_utils.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/native_theme/common_theme.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/button/label_button_border.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/non_client_view.h"

// Helpers --------------------------------------------------------------------

namespace {

int GetElementSpacing() {
  return ChromeLayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_UNRELATED_CONTROL_HORIZONTAL);
}

gfx::Insets GetCloseButtonSpacing() {
  auto* provider = ChromeLayoutProvider::Get();
  const gfx::Insets vector_button_insets =
      provider->GetInsetsMetric(views::INSETS_VECTOR_IMAGE_BUTTON);
  return gfx::Insets::VH(
             provider->GetDistanceMetric(DISTANCE_TOAST_CONTROL_VERTICAL),
             GetElementSpacing()) -
         vector_button_insets;
}

}  // namespace


// InfoBarView ----------------------------------------------------------------

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(InfoBarView, kInfoBarElementId);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(InfoBarView, kDismissButtonElementId);

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

  const ui::ImageModel& image = this->delegate()->GetIcon();
  if (!image.IsEmpty()) {
    icon_ = new views::ImageView;
    icon_->SetImage(image);
    icon_->SizeToPreferredSize();
    icon_->SetProperty(
        views::kMarginsKey,
        gfx::Insets::VH(ChromeLayoutProvider::Get()->GetDistanceMetric(
                            DISTANCE_TOAST_LABEL_VERTICAL),
                        0));
    AddChildView(icon_.get());
  }

  if (this->delegate()->IsCloseable()) {
    auto close_button = views::CreateVectorImageButton(base::BindRepeating(
        &InfoBarView::CloseButtonPressed, base::Unretained(this)));
    // This is the wrong color, but allows the button's size to be computed
    // correctly.  We'll reset this with the correct color in OnThemeChanged().
    views::SetImageFromVectorIconWithColor(
        close_button.get(), vector_icons::kCloseChromeRefreshIcon,
        gfx::kPlaceholderColor, gfx::kPlaceholderColor);
    close_button->SetTooltipText(l10n_util::GetStringUTF16(IDS_ACCNAME_CLOSE));
    gfx::Insets close_button_spacing = GetCloseButtonSpacing();
    close_button->SetProperty(
        views::kMarginsKey,
        gfx::Insets::TLBR(close_button_spacing.top(), 0,
                          close_button_spacing.bottom(), 0));
    close_button_ = AddChildView(std::move(close_button));
    close_button_->SetProperty(views::kElementIdentifierKey,
                               kDismissButtonElementId);

    InstallCircleHighlightPathGenerator(close_button_);
  }

  SetTargetHeight(
      ChromeLayoutProvider::Get()->GetDistanceMetric(DISTANCE_INFOBAR_HEIGHT));

  GetViewAccessibility().SetRole(ax::mojom::Role::kAlertDialog);
  GetViewAccessibility().SetName(l10n_util::GetStringUTF8(IDS_ACCNAME_INFOBAR));
  GetViewAccessibility().SetKeyShortcuts("Alt+Shift+A");
}

InfoBarView::~InfoBarView() {
  // We should have closed any open menus in PlatformSpecificHide(), then
  // subclasses' RunMenu() functions should have prevented opening any new ones
  // once we became unowned.
  DCHECK(!menu_runner_.get());
}

void InfoBarView::Layout(PassKey) {
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

gfx::Size InfoBarView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
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
  // TODO(330923783): Create a container for info bar subclasses to add children
  // to, so that we don't have to move the close button to the end every time a
  // child is added.
  if (details.is_add && (details.child == this) && close_button_) {
    ReorderChildView(close_button_, children().size());
  }
}

void InfoBarView::OnThemeChanged() {
  views::View::OnThemeChanged();
  const auto* cp = GetColorProvider();
  const SkColor background_color = cp->GetColor(kColorInfoBarBackground);
  SetBackground(views::CreateSolidBackground(background_color));

  const SkColor text_color = cp->GetColor(kColorInfoBarForeground);
  const SkColor icon_color = cp->GetColor(kColorInfoBarButtonIcon);
  const SkColor icon_disabled_color =
      cp->GetColor(kColorInfoBarButtonIconDisabled);
  if (close_button_) {
    views::SetImageFromVectorIconWithColor(
        close_button_, vector_icons::kCloseChromeRefreshIcon, icon_color,
        icon_disabled_color);
  }

  for (views::View* child : children()) {
    auto* label = views::AsViewClass<views::Label>(child);
    if (label) {
      label->SetBackgroundColor(background_color);
      if (!views::IsViewClass<views::Link>(child)) {
        label->SetEnabledColor(text_color);
        label->SetAutoColorReadabilityEnabled(false);
      }
    }
  }

  // Set dark mode status so that it can be used to set a different icon image
  // that is more suitable for a dark background.
  delegate()->set_dark_mode(
      color_utils::IsDark(cp->GetColor(kColorInfoBarBackground)));
  if (icon_) {
    icon_->SetImage(delegate()->GetIcon());
  }
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

std::unique_ptr<views::Label> InfoBarView::CreateLabel(
    const std::u16string& text) const {
  auto label = std::make_unique<views::Label>(
      text, views::style::CONTEXT_DIALOG_BODY_TEXT);
  SetLabelDetails(label.get());
  return label;
}

std::unique_ptr<views::Link> InfoBarView::CreateLink(
    const std::u16string& text) {
  auto link = std::make_unique<views::Link>(
      text, views::style::CONTEXT_DIALOG_BODY_TEXT);
  SetLabelDetails(link.get());
  link->SetCallback(
      base::BindRepeating(&InfoBarView::LinkClicked, base::Unretained(this)));
  return link;
}

// static
void InfoBarView::AssignWidths(Views* views, int available_width) {
  // Sort by width decreasing.
  std::sort(views->begin(), views->end(),
            [](views::View* view_1, views::View* view_2) {
              return view_1->GetPreferredSize().width() >
                     view_2->GetPreferredSize().width();
            });
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
  return std::max((target_height() - view->height()) / 2, 0) -
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
  SetFocusManager(nullptr);

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

void InfoBarView::SetLabelDetails(views::Label* label) const {
  label->SizeToPreferredSize();
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label->SetProperty(
      views::kMarginsKey,
      gfx::Insets::VH(ChromeLayoutProvider::Get()->GetDistanceMetric(
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

BEGIN_METADATA(InfoBarView)
ADD_READONLY_PROPERTY_METADATA(int, ContentMinimumWidth)
ADD_READONLY_PROPERTY_METADATA(int, StartX)
ADD_READONLY_PROPERTY_METADATA(int, EndX)
END_METADATA
