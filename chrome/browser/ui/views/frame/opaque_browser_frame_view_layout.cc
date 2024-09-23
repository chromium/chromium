// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/opaque_browser_frame_view_layout.h"

#include <algorithm>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/containers/adapters.h"
#include "base/i18n/rtl.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/frame/caption_button_placeholder_container.h"
#include "ui/base/ui_base_features.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/font.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/view_utils.h"
#include "ui/views/window/caption_button_layout_constants.h"
#include "ui/views/window/frame_caption_button.h"

namespace {

constexpr int kCaptionButtonHeight = 18;

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// OpaqueBrowserFrameViewLayout, public:

// statics

// The content edge images have a shadow built into them.
const int OpaqueBrowserFrameViewLayout::kContentEdgeShadowThickness = 2;

// The frame has a 2 px 3D edge along the top.  This is overridable by
// subclasses, so RestoredFrameEdgeInsets() should be used instead of using this
// constant directly.
const int OpaqueBrowserFrameViewLayout::kTopFrameEdgeThickness = 2;

// The frame has a 1 px 3D edge along the side.  This is overridable by
// subclasses, so RestoredFrameEdgeInsets() should be used instead of using this
// constant directly.
const int OpaqueBrowserFrameViewLayout::kSideFrameEdgeThickness = 1;

// The icon is inset 1 px from the left frame border.
const int OpaqueBrowserFrameViewLayout::kIconLeftSpacing = 1;

// There is a 4 px gap between the icon and the title text.
const int OpaqueBrowserFrameViewLayout::kIconTitleSpacing = 4;

// The horizontal spacing to use in most cases when laying out things near the
// caption button area.
const int OpaqueBrowserFrameViewLayout::kCaptionSpacing = 5;

// The minimum vertical padding between the bottom of the caption buttons and
// the top of the content shadow.
const int OpaqueBrowserFrameViewLayout::kCaptionButtonBottomPadding = 3;

OpaqueBrowserFrameViewLayout::OpaqueBrowserFrameViewLayout() = default;

OpaqueBrowserFrameViewLayout::~OpaqueBrowserFrameViewLayout() = default;

void OpaqueBrowserFrameViewLayout::SetButtonOrdering(
    const std::vector<views::FrameButton>& leading_buttons,
    const std::vector<views::FrameButton>& trailing_buttons) {
  leading_buttons_ = leading_buttons;
  trailing_buttons_ = trailing_buttons;
}

gfx::Rect OpaqueBrowserFrameViewLayout::GetBoundsForTabStripRegion(
    const gfx::Size& tabstrip_minimum_size,
    int total_width) const {
  const int x = available_space_leading_x_;
  const int available_width = available_space_trailing_x_ - x;
  return gfx::Rect(x, NonClientTopHeight(false), std::max(0, available_width),
                   tabstrip_minimum_size.height());
}

gfx::Rect OpaqueBrowserFrameViewLayout::GetBoundsForWebAppFrameToolbar(
    const gfx::Size& toolbar_preferred_size) const {
  if (delegate_->IsFullscreen()) {
    return gfx::Rect();
  }

  // Adding 2px of vertical padding puts at least 1 px of space on the top and
  // bottom of the element.
  constexpr int kVerticalPadding = 2;

  const int x = available_space_leading_x_;
  const int available_width = available_space_trailing_x_ - x;
  return gfx::Rect(x, FrameEdgeInsets(false).top(),
                   std::max(0, available_width),
                   toolbar_preferred_size.height() + kVerticalPadding +
                       kContentEdgeShadowThickness);
}

void OpaqueBrowserFrameViewLayout::LayoutWebAppWindowTitle(
    const gfx::Rect& available_space,
    views::Label& window_title_label) const {
  gfx::Rect bounds = available_space;
  bounds.Inset(gfx::Insets::TLBR(0, kIconTitleSpacing, 0, kCaptionSpacing));
  window_title_label.SetSubpixelRenderingEnabled(false);
  window_title_label.SetHorizontalAlignment(gfx::ALIGN_LEFT);
  window_title_label.SetBoundsRect(bounds);
}

gfx::Size OpaqueBrowserFrameViewLayout::GetMinimumSize(
    const views::View* host) const {
  // Ensure that we can fit the main browser view.
  gfx::Size min_size = delegate_->GetBrowserViewMinimumSize();
  if (delegate_->GetBorderlessModeEnabled()) {
    // In borderless mode the window doesn't have the window controls or tab
    // strip.
    return min_size;
  }

  // Ensure that we can, at minimum, hold our window controls and a tab strip.
  int top_width = minimum_size_for_buttons_;
  if (delegate_->IsTabStripVisible())
    top_width += delegate_->GetTabstripMinimumSize().width();
  min_size.set_width(std::max(min_size.width(), top_width));

  // Account for the frame.
  const auto border_insets = FrameBorderInsets(false);
  min_size.Enlarge(border_insets.width(),
                   NonClientTopHeight(false) + border_insets.bottom());

  return min_size;
}

gfx::Rect OpaqueBrowserFrameViewLayout::GetWindowBoundsForClientBounds(
    const gfx::Rect& client_bounds) const {
  int top_height = NonClientTopHeight(false);
  auto border_insets = FrameBorderInsets(false);
  return gfx::Rect(
      std::max(0, client_bounds.x() - border_insets.left()),
      std::max(0, client_bounds.y() - top_height),
      client_bounds.width() + border_insets.width(),
      client_bounds.height() + top_height + border_insets.bottom());
}

gfx::Insets OpaqueBrowserFrameViewLayout::FrameBorderInsets(
    bool restored) const {
  return !restored && delegate_->IsFrameCondensed()
             ? gfx::Insets()
             : RestoredFrameBorderInsets();
}

int OpaqueBrowserFrameViewLayout::FrameTopBorderThickness(bool restored) const {
  int thickness = FrameBorderInsets(restored).top();
  if ((restored || !delegate_->IsFrameCondensed()) && thickness > 0)
    thickness += NonClientExtraTopThickness();
  return thickness;
}

int OpaqueBrowserFrameViewLayout::NonClientTopHeight(bool restored) const {
  if (!delegate_->ShouldShowWindowTitle())
    return FrameTopBorderThickness(restored);

  // Adding 2px of vertical padding puts at least 1 px of space on the top and
  // bottom of the element.
  constexpr int kVerticalPadding = 2;
  const int icon_height = FrameEdgeInsets(restored).top() +
                          delegate_->GetIconSize() + kVerticalPadding;
  const int caption_button_height = DefaultCaptionButtonY(restored) +
                                    kCaptionButtonHeight +
                                    kCaptionButtonBottomPadding;

  int web_app_button_height = delegate_->WebAppButtonHeight();
  if (web_app_button_height > 0) {
    web_app_button_height += FrameEdgeInsets(restored).top() + kVerticalPadding;
  }
  return std::max(std::max(icon_height, caption_button_height),
                  web_app_button_height) +
         kContentEdgeShadowThickness;
}

gfx::Insets OpaqueBrowserFrameViewLayout::FrameEdgeInsets(bool restored) const {
  return IsFrameEdgeVisible(restored) ? RestoredFrameEdgeInsets()
                                      : gfx::Insets();
}

int OpaqueBrowserFrameViewLayout::DefaultCaptionButtonY(bool restored) const {
  // Maximized buttons start at window top, since the window has no border. This
  // offset is for the image (the actual clickable bounds extend all the way to
  // the top to take Fitts' Law into account).
  return views::NonClientFrameView::kFrameShadowThickness;
}

int OpaqueBrowserFrameViewLayout::CaptionButtonY(views::FrameButton button_id,
                                                 bool restored) const {
  return DefaultCaptionButtonY(restored);
}

gfx::Rect OpaqueBrowserFrameViewLayout::IconBounds() const {
  return window_icon_bounds_;
}

gfx::Rect OpaqueBrowserFrameViewLayout::CalculateClientAreaBounds(
    int width,
    int height) const {
  auto border_thickness = FrameBorderInsets(false);
  int top_height =
      (is_window_controls_overlay_enabled_ || is_borderless_mode_enabled_ ||
       delegate_->WebAppButtonHeight() > 0)
          ? border_thickness.top()
          : NonClientTopHeight(false);
  return gfx::Rect(
      border_thickness.left(), top_height,
      std::max(0, width - border_thickness.width()),
      std::max(0, height - top_height - border_thickness.bottom()));
}

int OpaqueBrowserFrameViewLayout::GetWindowCaptionSpacing(
    views::FrameButton button_id,
    bool leading_spacing,
    bool is_leading_button) const {
  if (leading_spacing) {
    if (is_leading_button) {
      // If we're the first button and maximized, add width to the right
      // hand side of the screen.
      return delegate_->IsFrameCondensed() && is_leading_button
                 ? kFrameBorderThickness -
                       views::NonClientFrameView::kFrameShadowThickness
                 : 0;
    }
    if (forced_window_caption_spacing_ >= 0)
      return forced_window_caption_spacing_;
  }
  return 0;
}

void OpaqueBrowserFrameViewLayout::SetWindowControlsOverlayEnabled(
    bool enabled,
    views::View* host) {
  if (enabled == is_window_controls_overlay_enabled_)
    return;

  is_window_controls_overlay_enabled_ = enabled;

  for (const raw_ptr<views::Button>& button :
       {minimize_button_, maximize_button_, restore_button_, close_button_}) {
    if (!button)
      continue;

    if (is_window_controls_overlay_enabled_) {
      // Move button to top of hierarchy to ensure that it receives events
      // before the placeholder container.
      host->AddChildView(button);
      button->SetPaintToLayer();
      button->layer()->SetFillsBoundsOpaquely(false);
    } else {
      button->DestroyLayer();
    }
  }
}

void OpaqueBrowserFrameViewLayout::SetBorderlessModeEnabled(bool enabled,
                                                            views::View* host) {
  is_borderless_mode_enabled_ = enabled;
}

///////////////////////////////////////////////////////////////////////////////
// OpaqueBrowserFrameViewLayout, protected:

OpaqueBrowserFrameViewLayout::TopAreaPadding
OpaqueBrowserFrameViewLayout::GetTopAreaPadding(
    bool has_leading_buttons,
    bool has_trailing_buttons) const {
  const auto padding = FrameBorderInsets(false);
  return TopAreaPadding{padding.left(), padding.right()};
}

gfx::Insets OpaqueBrowserFrameViewLayout::RestoredFrameBorderInsets() const {
  return gfx::Insets(kFrameBorderThickness);
}

gfx::Insets OpaqueBrowserFrameViewLayout::RestoredFrameEdgeInsets() const {
  return gfx::Insets::TLBR(kTopFrameEdgeThickness, kSideFrameEdgeThickness,
                           kSideFrameEdgeThickness, kSideFrameEdgeThickness);
}

int OpaqueBrowserFrameViewLayout::NonClientExtraTopThickness() const {
  return kNonClientExtraTopThickness;
}

bool OpaqueBrowserFrameViewLayout::IsFrameEdgeVisible(bool restored) const {
  return delegate_->UseCustomFrame() &&
         (restored || !delegate_->IsFrameCondensed());
}

///////////////////////////////////////////////////////////////////////////////
// OpaqueBrowserFrameViewLayout, private:

void OpaqueBrowserFrameViewLayout::LayoutWindowControls() {
  // Keep a list of all buttons that we don't show.
  std::vector<views::FrameButton> buttons_not_shown;
  buttons_not_shown.push_back(views::FrameButton::kMaximize);
  buttons_not_shown.push_back(views::FrameButton::kMinimize);
  buttons_not_shown.push_back(views::FrameButton::kClose);

  if (delegate_->ShouldShowCaptionButtons()) {
    for (const auto& button : leading_buttons_) {
      ConfigureButton(button, ALIGN_LEADING);
      std::erase(buttons_not_shown, button);
    }

    for (const auto& button : base::Reversed(trailing_buttons_)) {
      ConfigureButton(button, ALIGN_TRAILING);
      std::erase(buttons_not_shown, button);
    }
  }

  for (const auto& button_id : buttons_not_shown)
    HideButton(button_id);
}

void OpaqueBrowserFrameViewLayout::LayoutTitleBar() {
  bool use_hidden_icon_location = true;

  int size = delegate_->GetIconSize();
  bool should_show_icon = delegate_->ShouldShowWindowIcon() && window_icon_;
  bool should_show_title = delegate_->ShouldShowWindowTitle() && window_title_;
  bool should_show_toolbar = delegate_->WebAppButtonHeight() > 0;
  std::optional<int> icon_spacing;

  if (should_show_icon || should_show_title || should_show_toolbar) {
    use_hidden_icon_location = false;

    // Our frame border has a different "3D look" than Windows'.  Theirs has
    // a more complex gradient on the top that they push their icon/title
    // below; then the maximized window cuts this off and the icon/title are
    // centered in the remaining space.  Because the apparent shape of our
    // border is simpler, using the same positioning makes things look
    // slightly uncentered with restored windows, so when the window is
    // restored, instead of calculating the remaining space from below the
    // frame border, we calculate from below the 3D edge.
    const int unavailable_dip_at_top = FrameEdgeInsets(false).top();
    // When the icon is shorter than the minimum space we reserve for the
    // caption button, we vertically center it.  We want to bias rounding to
    // put extra space below the icon, since we'll use the same Y coordinate for
    // the title, and the majority of the font weight is below the centerline.
    const int available_height = NonClientTopHeight(false);
    const int icon_height =
        unavailable_dip_at_top + size + kContentEdgeShadowThickness;
    const int y = unavailable_dip_at_top + (available_height - icon_height) / 2;

    // Want same spacing adjacent to the icon as above when the icon is the
    // first element in the frame. We'll use this spacing again to ensure
    // appropriate spacing between icon and title.
    icon_spacing = y;
    if (should_show_toolbar && leading_buttons_.empty()) {
      const auto insets = FrameEdgeInsets(false);
      available_space_leading_x_ = insets.left() + *icon_spacing;
    } else {
      available_space_leading_x_ += kIconLeftSpacing;
    }

    window_icon_bounds_ = gfx::Rect(available_space_leading_x_, y, size, size);
    available_space_leading_x_ += size;
    minimum_size_for_buttons_ += size;
  }

  if (window_icon_) {
    SetViewVisibility(window_icon_, should_show_icon);
    if (should_show_icon)
      window_icon_->SetBoundsRect(window_icon_bounds_);
  }

  if (window_title_) {
    SetViewVisibility(window_title_, should_show_title);
    if (should_show_title) {
      window_title_->SetText(delegate_->GetWindowTitle());

      // If possible, make space between icon and title symmetrical with space
      // between icon and frame.
      const int icon_title_spacing = kIconTitleSpacing;
      const int text_width =
          std::max(0, available_space_trailing_x_ - kCaptionSpacing -
                          available_space_leading_x_ - icon_title_spacing);
      window_title_->SetBounds(available_space_leading_x_ + icon_title_spacing,
                               window_icon_bounds_.y(), text_width,
                               window_icon_bounds_.height());
      available_space_leading_x_ += text_width + icon_title_spacing;
    }
  }

  if (use_hidden_icon_location) {
    if (placed_leading_button_) {
      // There are window button icons on the left. Don't size the hidden window
      // icon that people can double click on to close the window.
      window_icon_bounds_ = gfx::Rect();
    } else {
      // We set the icon bounds to a small rectangle in the top leading corner
      // if there are no icons on the leading side.
      const auto frame_insets = FrameBorderInsets(false);
      window_icon_bounds_ = gfx::Rect(frame_insets.left() + kIconLeftSpacing,
                                      frame_insets.top(), size, size);
    }
  }
}

void OpaqueBrowserFrameViewLayout::ConfigureButton(views::FrameButton button_id,
                                                   ButtonAlignment alignment) {
  switch (button_id) {
    case views::FrameButton::kMinimize: {
      if (delegate_->CanMinimize()) {
        SetViewVisibility(minimize_button_, true);
        SetBoundsForButton(button_id, minimize_button_, alignment);
      } else {
        HideButton(button_id);
      }
      break;
    }
    case views::FrameButton::kMaximize: {
      if (delegate_->CanMaximize()) {
        // When the window is restored, we show a maximized button; otherwise,
        // we show a restore button.
        bool is_restored =
            !delegate_->IsMaximized() && !delegate_->IsMinimized();
        views::Button* invisible_button =
            is_restored ? restore_button_ : maximize_button_;
        SetViewVisibility(invisible_button, false);

        views::Button* visible_button =
            is_restored ? maximize_button_ : restore_button_;
        SetViewVisibility(visible_button, true);
        SetBoundsForButton(button_id, visible_button, alignment);
      } else {
        HideButton(button_id);
      }
      break;
    }
    case views::FrameButton::kClose: {
      SetViewVisibility(close_button_, true);
      SetBoundsForButton(button_id, close_button_, alignment);
      break;
    }
  }
}

void OpaqueBrowserFrameViewLayout::HideButton(views::FrameButton button_id) {
  switch (button_id) {
    case views::FrameButton::kMinimize:
      SetViewVisibility(minimize_button_, false);
      break;
    case views::FrameButton::kMaximize:
      SetViewVisibility(restore_button_, false);
      SetViewVisibility(maximize_button_, false);
      break;
    case views::FrameButton::kClose:
      SetViewVisibility(close_button_, false);
      break;
  }
}

void OpaqueBrowserFrameViewLayout::SetBoundsForButton(
    views::FrameButton button_id,
    views::Button* button,
    ButtonAlignment alignment) {
  const int caption_y = CaptionButtonY(button_id, false);

  // There should always be the same number of non-shadow pixels visible to the
  // side of the caption buttons.  In maximized mode we extend buttons to the
  // screen top and the rightmost button to the screen right (or leftmost button
  // to the screen left, for left-aligned buttons) to obey Fitts' Law.
  const bool is_frame_condensed = delegate_->IsFrameCondensed();

  const int button_width = views::GetCaptionButtonWidth();

  gfx::Size button_size = button->GetPreferredSize();
  if (delegate_->GetFrameButtonStyle() ==
      OpaqueBrowserFrameViewLayoutDelegate::FrameButtonStyle::kMdButton) {
    DCHECK_EQ(std::string(views::FrameCaptionButton::kViewClassName),
              button->GetClassName());
    const int caption_button_center_size =
        button_width - 2 * views::kCaptionButtonInkDropDefaultCornerRadius;
    const int height =
        delegate_->GetTopAreaHeight() - FrameEdgeInsets(false).top();
    const int corner_radius =
        std::clamp((height - caption_button_center_size) / 2, 0,
                   views::kCaptionButtonInkDropDefaultCornerRadius);
    button_size = gfx::Size(button_width, height);
    button->SetPreferredSize(button_size);
    static_cast<views::FrameCaptionButton*>(button)->SetInkDropCornerRadius(
        corner_radius);
  } else if (delegate_->GetFrameButtonStyle() ==
             OpaqueBrowserFrameViewLayoutDelegate::FrameButtonStyle::
                 kImageButton) {
    DCHECK_EQ(std::string(views::ImageButton::kViewClassName),
              button->GetClassName());
    auto* const image_button = static_cast<views::ImageButton*>(button);
    image_button->SetImageHorizontalAlignment(
        (alignment == ALIGN_LEADING) ? views::ImageButton::ALIGN_RIGHT
                                     : views::ImageButton::ALIGN_LEFT);
    image_button->SetImageVerticalAlignment(views::ImageButton::ALIGN_BOTTOM);
  }

  TopAreaPadding top_area_padding = GetTopAreaPadding();

  switch (alignment) {
    case ALIGN_LEADING: {
      int extra_width = top_area_padding.leading;
      int button_start_spacing =
          GetWindowCaptionSpacing(button_id, true, !placed_leading_button_);

      available_space_leading_x_ += button_start_spacing;
      minimum_size_for_buttons_ += button_start_spacing;

      bool top_spacing_clickable = is_frame_condensed;
      bool start_spacing_clickable =
          is_frame_condensed && !placed_leading_button_;
      button->SetBounds(
          available_space_leading_x_ - (start_spacing_clickable
                                            ? button_start_spacing + extra_width
                                            : 0),
          top_spacing_clickable ? 0 : caption_y,
          button_size.width() + (start_spacing_clickable
                                     ? button_start_spacing + extra_width
                                     : 0),
          button_size.height() + (top_spacing_clickable ? caption_y : 0));

      int button_end_spacing =
          GetWindowCaptionSpacing(button_id, false, !placed_leading_button_);
      available_space_leading_x_ += button_size.width() + button_end_spacing;
      minimum_size_for_buttons_ += button_size.width() + button_end_spacing;
      placed_leading_button_ = true;
      break;
    }
    case ALIGN_TRAILING: {
      int extra_width = top_area_padding.trailing;
      int button_start_spacing =
          GetWindowCaptionSpacing(button_id, true, !placed_trailing_button_);

      available_space_trailing_x_ -= button_start_spacing;
      minimum_size_for_buttons_ += button_start_spacing;

      bool top_spacing_clickable = is_frame_condensed;
      bool start_spacing_clickable =
          is_frame_condensed && !placed_trailing_button_;
      button->SetBounds(
          available_space_trailing_x_ - button_size.width(),
          top_spacing_clickable ? 0 : caption_y,
          button_size.width() + (start_spacing_clickable
                                     ? button_start_spacing + extra_width
                                     : 0),
          button_size.height() + (top_spacing_clickable ? caption_y : 0));

      int button_end_spacing =
          GetWindowCaptionSpacing(button_id, false, !placed_trailing_button_);
      available_space_trailing_x_ -= button_size.width() + button_end_spacing;
      minimum_size_for_buttons_ += button_size.width() + button_end_spacing;
      placed_trailing_button_ = true;
      break;
    }
  }
}

void OpaqueBrowserFrameViewLayout::SetView(int id, views::View* view) {
  // Why do things this way instead of having an Init() method, where we're
  // passed the views we'll handle? Because OpaqueBrowserFrameView doesn't own
  // all the views which are part of it.
  switch (id) {
    case VIEW_ID_MINIMIZE_BUTTON:
      minimize_button_ = static_cast<views::Button*>(view);
      break;
    case VIEW_ID_MAXIMIZE_BUTTON:
      maximize_button_ = static_cast<views::Button*>(view);
      break;
    case VIEW_ID_RESTORE_BUTTON:
      restore_button_ = static_cast<views::Button*>(view);
      break;
    case VIEW_ID_CLOSE_BUTTON:
      close_button_ = static_cast<views::Button*>(view);
      break;
    case VIEW_ID_WINDOW_ICON:
      window_icon_ = view;
      break;
    case VIEW_ID_WINDOW_TITLE:
      if (view) {
        DCHECK_EQ(std::string(views::Label::kViewClassName),
                  view->GetClassName());
      }
      window_title_ = static_cast<views::Label*>(view);
      break;
  }

  if (view && views::IsViewClass<CaptionButtonPlaceholderContainer>(view)) {
    caption_button_placeholder_container_ =
        static_cast<CaptionButtonPlaceholderContainer*>(view);
  }

  if (is_window_controls_overlay_enabled_ &&
      (id == VIEW_ID_MINIMIZE_BUTTON || id == VIEW_ID_MAXIMIZE_BUTTON ||
       id == VIEW_ID_RESTORE_BUTTON || id == VIEW_ID_CLOSE_BUTTON)) {
    view->SetPaintToLayer();
    view->layer()->SetFillsBoundsOpaquely(false);
  }
}

OpaqueBrowserFrameViewLayout::TopAreaPadding
OpaqueBrowserFrameViewLayout::GetTopAreaPadding() const {
  return GetTopAreaPadding(!leading_buttons_.empty(),
                           !trailing_buttons_.empty());
}

void OpaqueBrowserFrameViewLayout::LayoutTitleBarForWindowControlsOverlay(
    const views::View* host) {
  int height = NonClientTopHeight(false);
  int container_x = placed_trailing_button_ ? available_space_trailing_x_ : 0;
  auto insets = FrameBorderInsets(/*restored=*/false);
  caption_button_placeholder_container_->SetBounds(
      container_x, insets.top(), minimum_size_for_buttons_ - insets.width(),
      height - insets.top());
}

///////////////////////////////////////////////////////////////////////////////
// OpaqueBrowserFrameViewLayout, views::LayoutManager:

void OpaqueBrowserFrameViewLayout::Layout(views::View* host) {
  TRACE_EVENT0("views.frame", "OpaqueBrowserFrameViewLayout::Layout");
  // Reset all our data so that everything is invisible.
  TopAreaPadding top_area_padding = GetTopAreaPadding();
  available_space_leading_x_ = top_area_padding.leading;
  available_space_trailing_x_ = host->width() - top_area_padding.trailing;
  minimum_size_for_buttons_ =
      available_space_leading_x_ + host->width() - available_space_trailing_x_;
  placed_leading_button_ = false;
  placed_trailing_button_ = false;

  LayoutWindowControls();
  if (is_window_controls_overlay_enabled_)
    LayoutTitleBarForWindowControlsOverlay(host);
  else
    LayoutTitleBar();

  // Any buttons/icon/title were laid out based on the frame border thickness,
  // but the tabstrip bounds need to be based on the non-client border thickness
  // on any side where there aren't other buttons forcing a larger inset.
  const int old_button_size =
      available_space_leading_x_ + host->width() - available_space_trailing_x_;
  auto insets = FrameBorderInsets(false);
  available_space_leading_x_ =
      std::max(available_space_leading_x_, insets.left());
  // The trailing corner is a mirror of the leading one.
  available_space_trailing_x_ =
      std::min(available_space_trailing_x_, host->width() - insets.right());
  if (base::i18n::IsRTL()) {
    auto offset = insets.right() - insets.left();
    available_space_leading_x_ += offset;
    available_space_trailing_x_ += offset;
  }
  minimum_size_for_buttons_ += (available_space_leading_x_ + host->width() -
                                available_space_trailing_x_ - old_button_size);

  client_view_bounds_ =
      CalculateClientAreaBounds(host->width(), host->height());
}

gfx::Size OpaqueBrowserFrameViewLayout::GetPreferredSize(
    const views::View* host) const {
  // This is never used; NonClientView::CalculatePreferredSize() will be called
  // instead.
  NOTREACHED();
}

gfx::Size OpaqueBrowserFrameViewLayout::GetPreferredSize(
    const views::View* host,
    const views::SizeBounds& available_size) const {
  // This is never used; NonClientView::CalculatePreferredSize() will be called
  // instead.
  NOTREACHED();
}

void OpaqueBrowserFrameViewLayout::ViewAdded(views::View* host,
                                             views::View* view) {
  if (views::IsViewClass<views::ClientView>(view)) {
    client_view_ = static_cast<views::ClientView*>(view);
    return;
  }

  SetView(view->GetID(), view);
}

void OpaqueBrowserFrameViewLayout::ViewRemoved(views::View* host,
                                               views::View* view) {
  if (views::IsViewClass<views::ClientView>(view)) {
    client_view_ = nullptr;
    return;
  }

  SetView(view->GetID(), nullptr);
}
