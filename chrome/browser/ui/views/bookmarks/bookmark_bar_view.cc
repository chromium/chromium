// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bookmarks/bookmark_bar_view.h"

#include <stdint.h>

#include <algorithm>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/i18n/rtl.h"
#include "base/macros.h"
#include "base/metrics/user_metrics.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/bookmarks/managed_bookmark_service_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/favicon/favicon_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_metrics.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/bookmarks/bookmark_drag_drop.h"
#include "chrome/browser/ui/bookmarks/bookmark_tab_helper.h"
#include "chrome/browser/ui/bookmarks/bookmark_utils.h"
#include "chrome/browser/ui/bookmarks/bookmark_utils_desktop.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_bar_view_observer.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_context_menu.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_menu_controller_views.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/event_utils.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/top_container_background.h"
#include "chrome/browser/ui/views/toolbar/toolbar_ink_drop_util.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/extension_metrics.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/bookmarks/managed/managed_bookmark_service.h"
#include "components/metrics/metrics_service.h"
#include "components/prefs/pref_service.h"
#include "components/url_formatter/elide_url.h"
#include "components/url_formatter/url_formatter.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/theme_provider.h"
#include "ui/base/window_open_disposition.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/events/event_constants.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/favicon_size.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/gfx/text_constants.h"
#include "ui/gfx/text_elider.h"
#include "ui/resources/grit/ui_resources.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/flood_fill_ink_drop_ripple.h"
#include "ui/views/animation/ink_drop_highlight.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/animation/ink_drop_mask.h"
#include "ui/views/button_drag_utils.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/label_button_border.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/button/menu_button_controller.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/label.h"
#include "ui/views/drag_utils.h"
#include "ui/views/metrics.h"
#include "ui/views/style/platform_style.h"
#include "ui/views/view_constants.h"
#include "ui/views/widget/tooltip_manager.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/non_client_view.h"

using base::UserMetricsAction;
using bookmarks::BookmarkModel;
using bookmarks::BookmarkNode;
using bookmarks::BookmarkNodeData;
using content::OpenURLParams;
using content::PageNavigator;
using content::Referrer;
using ui::DropTargetEvent;
using views::Button;
using views::LabelButtonBorder;
using views::MenuButton;
using views::View;
using MD = ui::MaterialDesignController;

// Maximum size of buttons on the bookmark bar.
static const int kBookmarkBarMaxButtonWidth = 150;

// Margin around the content.
static const int kBookmarkBarHorizontalMargin = 8;

// Width of the drop indicator.
static const int kBookmarkBarDropIndicatorWidth = 2;

// Width of the separator between the recently bookmarked button and the
// overflow indicator.
static const int kBookmarkBarSeparatorWidth = 9;

// Tag for the 'Apps Shortcut' button.
static const int kBookmarkBarAppsShortcutButtonTag = 2;

namespace {

// Used to globally disable rich animations.
bool animations_enabled = true;

gfx::ImageSkia* GetImageSkiaNamed(int id) {
  return ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(id);
}

std::unique_ptr<views::InkDrop> CreateBookmarkButtonInkDrop(
    std::unique_ptr<views::InkDropImpl> ink_drop) {
  ink_drop->SetShowHighlightOnFocus(!views::PlatformStyle::kPreferFocusRings);
  return std::move(ink_drop);
}

std::unique_ptr<views::LabelButtonBorder> CreateBookmarkButtonBorder() {
  std::unique_ptr<LabelButtonBorder> border =
      std::make_unique<LabelButtonBorder>();
  border->set_insets(ChromeLayoutProvider::Get()->GetInsetsMetric(
      INSETS_BOOKMARKS_BAR_BUTTON));
  return border;
}

// BookmarkButtonBase -----------------------------------------------

// Base class for non-menu hosting buttons used on the bookmark bar.

class BookmarkButtonBase : public views::LabelButton {
 public:
  BookmarkButtonBase(BookmarkBarView::ButtonListener* listener,
                     const base::string16& title)
      : LabelButton(listener, title) {
    SetImageLabelSpacing(ChromeLayoutProvider::Get()->GetDistanceMetric(
        DISTANCE_RELATED_LABEL_HORIZONTAL_LIST));
    views::InstallPillHighlightPathGenerator(this);
    SetInkDropMode(InkDropMode::ON);
    set_has_ink_drop_action_on_click(true);
    set_ink_drop_visible_opacity(kToolbarInkDropVisibleOpacity);
    show_animation_ = std::make_unique<gfx::SlideAnimation>(this);
    if (!animations_enabled) {
      // For some reason during testing the events generated by animating
      // throw off the test. So, don't animate while testing.
      show_animation_->Reset(1);
    } else {
      show_animation_->Show();
    }
  }

  View* GetTooltipHandlerForPoint(const gfx::Point& point) override {
    return HitTestPoint(point) && CanProcessEventsWithinSubtree() ? this
                                                                  : nullptr;
  }

  bool IsTriggerableEvent(const ui::Event& e) override {
    return e.type() == ui::ET_GESTURE_TAP ||
           e.type() == ui::ET_GESTURE_TAP_DOWN ||
           event_utils::IsPossibleDispositionEvent(e);
  }

  // LabelButton:
  std::unique_ptr<views::InkDrop> CreateInkDrop() override {
    return CreateBookmarkButtonInkDrop(CreateDefaultFloodFillInkDropImpl());
  }

  std::unique_ptr<views::InkDropHighlight> CreateInkDropHighlight()
      const override {
    return CreateToolbarInkDropHighlight(this);
  }

  SkColor GetInkDropBaseColor() const override {
    return GetToolbarInkDropBaseColor(this);
  }

  std::unique_ptr<views::LabelButtonBorder> CreateDefaultBorder()
      const override {
    return CreateBookmarkButtonBorder();
  }

 private:
  std::unique_ptr<gfx::SlideAnimation> show_animation_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkButtonBase);
};

// BookmarkButton -------------------------------------------------------------

// Buttons used for the bookmarks on the bookmark bar.

class BookmarkButton : public BookmarkButtonBase {
 public:
  // The internal view class name.
  static const char kViewClassName[];

  BookmarkButton(BookmarkBarView::ButtonListener* listener,
                 const GURL& url,
                 const base::string16& title)
      : BookmarkButtonBase(listener, title), url_(url) {}

  // views::View:
  base::string16 GetTooltipText(const gfx::Point& p) const override {
    const views::TooltipManager* tooltip_manager =
        GetWidget()->GetTooltipManager();
    gfx::Point location(p);
    ConvertPointToScreen(this, &location);
    // Also update when the maximum width for tooltip has changed because the
    // it may be elided differently.
    int max_tooltip_width = tooltip_manager->GetMaxWidth(location);
    if (tooltip_text_.empty() || max_tooltip_width != max_tooltip_width_) {
      max_tooltip_width_ = max_tooltip_width;
      tooltip_text_ = BookmarkBarView::CreateToolTipForURLAndTitle(
          max_tooltip_width_, tooltip_manager->GetFontList(), url_, GetText());
    }
    return tooltip_text_;
  }

  void SetText(const base::string16& text) override {
    BookmarkButtonBase::SetText(text);
    tooltip_text_.clear();
  }

  const char* GetClassName() const override { return kViewClassName; }

 private:
  // A cached value of maximum width for tooltip to skip generating
  // new tooltip text.
  mutable int max_tooltip_width_ = 0;
  mutable base::string16 tooltip_text_;
  const GURL& url_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkButton);
};

// static
const char BookmarkButton::kViewClassName[] = "BookmarkButton";

// ShortcutButton -------------------------------------------------------------

// Buttons used for the shortcuts on the bookmark bar.

class ShortcutButton : public BookmarkButtonBase {
 public:
  // The internal view class name.
  static const char kViewClassName[];

  ShortcutButton(BookmarkBarView::ButtonListener* listener,
                 const base::string16& title)
      : BookmarkButtonBase(listener, title) {}

  const char* GetClassName() const override { return kViewClassName; }

 private:
  DISALLOW_COPY_AND_ASSIGN(ShortcutButton);
};

// static
const char ShortcutButton::kViewClassName[] = "ShortcutButton";

// BookmarkMenuButtonBase -----------------------------------------------------

// Base class for menu hosting buttons used on the bookmark bar.
class BookmarkMenuButtonBase : public views::MenuButton {
 public:
  BookmarkMenuButtonBase(const base::string16& title,
                         BookmarkBarView::MenuButtonListener* button_listener)
      : MenuButton(title, button_listener) {
    SetImageLabelSpacing(ChromeLayoutProvider::Get()->GetDistanceMetric(
        DISTANCE_RELATED_LABEL_HORIZONTAL_LIST));
    views::InstallPillHighlightPathGenerator(this);
    SetInkDropMode(InkDropMode::ON);
    set_ink_drop_visible_opacity(kToolbarInkDropVisibleOpacity);
  }

  // MenuButton:
  std::unique_ptr<views::InkDrop> CreateInkDrop() override {
    return CreateBookmarkButtonInkDrop(CreateDefaultFloodFillInkDropImpl());
  }

  std::unique_ptr<views::InkDropHighlight> CreateInkDropHighlight()
      const override {
    return CreateToolbarInkDropHighlight(this);
  }

  SkColor GetInkDropBaseColor() const override {
    return GetToolbarInkDropBaseColor(this);
  }

  std::unique_ptr<views::LabelButtonBorder> CreateDefaultBorder()
      const override {
    return CreateBookmarkButtonBorder();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(BookmarkMenuButtonBase);
};

// BookmarkFolderButton -------------------------------------------------------

// Buttons used for folders on the bookmark bar, including the 'other folders'
// button.
class BookmarkFolderButton : public BookmarkMenuButtonBase {
 public:
  BookmarkFolderButton(const base::string16& title,
                       BookmarkBarView::MenuButtonListener* button_listener)
      : BookmarkMenuButtonBase(title, button_listener) {
    show_animation_ = std::make_unique<gfx::SlideAnimation>(this);
    if (!animations_enabled) {
      // For some reason during testing the events generated by animating
      // throw off the test. So, don't animate while testing.
      show_animation_->Reset(1);
    } else {
      show_animation_->Show();
    }

    // ui::EF_MIDDLE_MOUSE_BUTTON opens all bookmarked links in separate tabs.
    set_triggerable_event_flags(ui::EF_LEFT_MOUSE_BUTTON |
                                ui::EF_MIDDLE_MOUSE_BUTTON);
  }

  base::string16 GetTooltipText(const gfx::Point& p) const override {
    return label()->GetPreferredSize().width() > label()->size().width()
               ? GetText()
               : base::string16();
  }

  bool OnMousePressed(const ui::MouseEvent& event) override {
    if (event.IsOnlyLeftMouseButton()) {
      // TODO(bruthig): The ACTION_PENDING triggering logic should be in
      // MenuButton::OnPressed() however there is a bug with the pressed state
      // logic in MenuButton. See http://crbug.com/567252.
      AnimateInkDrop(views::InkDropState::ACTION_PENDING, &event);
    }
    return MenuButton::OnMousePressed(event);
  }

 private:
  std::unique_ptr<gfx::SlideAnimation> show_animation_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkFolderButton);
};

// OverflowButton (chevron) --------------------------------------------------

class OverflowButton : public BookmarkMenuButtonBase {
 public:
  OverflowButton(BookmarkBarView* owner,
                 BookmarkBarView::MenuButtonListener* button_listener)
      : BookmarkMenuButtonBase(base::string16(), button_listener),
        owner_(owner) {}

  bool OnMousePressed(const ui::MouseEvent& e) override {
    owner_->StopThrobbing(true);
    return BookmarkMenuButtonBase::OnMousePressed(e);
  }

 private:
  BookmarkBarView* owner_;

  DISALLOW_COPY_AND_ASSIGN(OverflowButton);
};

void RecordAppLaunch(Profile* profile, const GURL& url) {
  const extensions::Extension* extension =
      extensions::ExtensionRegistry::Get(profile)
          ->enabled_extensions()
          .GetAppByURL(url);
  if (!extension)
    return;

  extensions::RecordAppLaunchType(extension_misc::APP_LAUNCH_BOOKMARK_BAR,
                                  extension->GetType());
}

}  // namespace

// DropLocation ---------------------------------------------------------------

struct BookmarkBarView::DropLocation {
  bool Equals(const DropLocation& other) {
    return ((other.index == index) && (other.on == on) &&
            (other.button_type == button_type));
  }

  // Index into the model the drop is over. This is relative to the root node.
  base::Optional<size_t> index;

  // Drop constants.
  int operation = ui::DragDropTypes::DRAG_NONE;

  // If true, the user is dropping on a folder.
  bool on = false;

  // Type of button.
  DropButtonType button_type = DROP_BOOKMARK;
};

// DropInfo -------------------------------------------------------------------

// Tracks drops on the BookmarkBarView.

struct BookmarkBarView::DropInfo {
  DropInfo() : valid(false), is_menu_showing(false), x(0), y(0) {}

  // Whether the data is valid.
  bool valid;

  // If true, the menu is being shown.
  bool is_menu_showing;

  // Coordinates of the drag (in terms of the BookmarkBarView).
  int x;
  int y;

  // DropData for the drop.
  BookmarkNodeData data;

  DropLocation location;
};

// ButtonSeparatorView  --------------------------------------------------------

class BookmarkBarView::ButtonSeparatorView : public views::View {
 public:
  ButtonSeparatorView() {}
  ~ButtonSeparatorView() override {}

  void OnPaint(gfx::Canvas* canvas) override {
    gfx::ScopedCanvas scoped_canvas(canvas);
    // 1px wide at all scale factors. If there is an uneven amount of padding
    // left over, place the extra pixel on the outside, i.e. away from the
    // "Other bookmarks" folder.
    const float scale = canvas->UndoDeviceScaleFactor();
    const gfx::RectF scaled_bounds =
        gfx::ScaleRect(gfx::RectF(bounds()), scale);

    const int kLineThickness = 1;
    const float fractional_x = (scaled_bounds.width() - kLineThickness) / 2.f;
    const int x = base::i18n::IsRTL() ? std::floor(fractional_x)
                                      : std::ceil(fractional_x);

    const int height = gfx::kFaviconSize * scale;
    const int top_y = (scaled_bounds.height() - height) / 2;
    canvas->DrawLine(gfx::Point(x, top_y), gfx::Point(x, top_y + height),
                     GetThemeProvider()->GetColor(
                         ThemeProperties::COLOR_TOOLBAR_VERTICAL_SEPARATOR));
  }

  gfx::Size CalculatePreferredSize() const override {
    // We get the full height of the bookmark bar, so that the height returned
    // here doesn't matter.
    return gfx::Size(kBookmarkBarSeparatorWidth, 1);
  }

  void GetAccessibleNodeData(ui::AXNodeData* node_data) override {
    node_data->SetName(l10n_util::GetStringUTF8(IDS_ACCNAME_SEPARATOR));
    node_data->role = ax::mojom::Role::kSplitter;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ButtonSeparatorView);
};

// BookmarkBarView ------------------------------------------------------------

BookmarkBarView::ButtonListener::ButtonListener(BookmarkBarView* parent)
    : parent_(parent) {}

void BookmarkBarView::ButtonListener::ButtonPressed(views::Button* source,
                                                    const ui::Event& event) {
  parent_->OnButtonPressed(source, event);
}

BookmarkBarView::MenuButtonListener::MenuButtonListener(BookmarkBarView* parent)
    : parent_(parent) {}

void BookmarkBarView::MenuButtonListener::ButtonPressed(
    views::Button* source,
    const ui::Event& event) {
  parent_->OnMenuButtonPressed(source, event);
}

// static
const char BookmarkBarView::kViewClassName[] = "BookmarkBarView";

BookmarkBarView::BookmarkBarView(Browser* browser, BrowserView* browser_view)
    : AnimationDelegateViews(this),
      button_listener_(this),
      menu_button_listener_(this),
      page_navigator_(nullptr),
      managed_(nullptr),
      bookmark_menu_(nullptr),
      bookmark_drop_menu_(nullptr),
      other_bookmarks_button_(nullptr),
      managed_bookmarks_button_(nullptr),
      apps_page_shortcut_(nullptr),
      overflow_button_(nullptr),
      bookmarks_separator_view_(nullptr),
      browser_(browser),
      browser_view_(browser_view),
      infobar_visible_(false),
      size_animation_(this),
      throbbing_view_(nullptr),
      bookmark_bar_state_(BookmarkBar::SHOW) {
  SetID(VIEW_ID_BOOKMARK_BAR);
  Init();

  // TODO(lgrey): This layer was introduced to support clipping the bookmark
  // bar to bounds to prevent it from drawing over the toolbar while animating.
  // This is no longer necessary, so the masking was removed; however removing
  // the layer now makes the animation jerky (or jerkier). The animation should
  // be fixed and, if the layer is no longer necessary, itshould be removed.
  // See https://crbug.com/844037.
  SetPaintToLayer();

  size_animation_.Reset(1);
  if (!gfx::Animation::ShouldRenderRichAnimation())
    animations_enabled = false;

  // May be null for tests.
  if (browser_view)
    SetBackground(std::make_unique<TopContainerBackground>(browser_view));
}

BookmarkBarView::~BookmarkBarView() {
  if (model_)
    model_->RemoveObserver(this);

  // It's possible for the menu to outlive us, reset the observer to make sure
  // it doesn't have a reference to us.
  if (bookmark_menu_) {
    bookmark_menu_->set_observer(nullptr);
    bookmark_menu_->SetPageNavigator(nullptr);
    bookmark_menu_->clear_bookmark_bar();
  }
  if (context_menu_.get())
    context_menu_->SetPageNavigator(nullptr);

  StopShowFolderDropMenuTimer();
}

// static
void BookmarkBarView::DisableAnimationsForTesting(bool disabled) {
  animations_enabled = !disabled;
}

void BookmarkBarView::AddObserver(BookmarkBarViewObserver* observer) {
  observers_.AddObserver(observer);
}

void BookmarkBarView::RemoveObserver(BookmarkBarViewObserver* observer) {
  observers_.RemoveObserver(observer);
}

void BookmarkBarView::SetPageNavigator(PageNavigator* navigator) {
  page_navigator_ = navigator;
  if (bookmark_menu_)
    bookmark_menu_->SetPageNavigator(navigator);
  if (context_menu_.get())
    context_menu_->SetPageNavigator(navigator);
}

void BookmarkBarView::SetInfoBarVisible(bool infobar_visible) {
  if (infobar_visible == infobar_visible_)
    return;
  InvalidateLayout();
  infobar_visible_ = infobar_visible;
}

void BookmarkBarView::SetBookmarkBarState(
    BookmarkBar::State state,
    BookmarkBar::AnimateChangeType animate_type) {
  if (animate_type == BookmarkBar::ANIMATE_STATE_CHANGE && animations_enabled) {
    if (state == BookmarkBar::SHOW)
      size_animation_.Show();
    else
      size_animation_.Hide();
  } else {
    size_animation_.Reset(state == BookmarkBar::SHOW ? 1 : 0);
    if (!animations_enabled)
      AnimationEnded(&size_animation_);
  }
  bookmark_bar_state_ = state;
}

const BookmarkNode* BookmarkBarView::GetNodeForButtonAtModelIndex(
    const gfx::Point& loc,
    size_t* model_start_index) {
  *model_start_index = 0;

  if (loc.x() < 0 || loc.x() >= width() || loc.y() < 0 || loc.y() >= height())
    return nullptr;

  gfx::Point adjusted_loc(GetMirroredXInView(loc.x()), loc.y());

  // Check the managed button first.
  if (managed_bookmarks_button_->GetVisible() &&
      managed_bookmarks_button_->bounds().Contains(adjusted_loc)) {
    return managed_->managed_node();
  }

  // Then check the bookmark buttons.
  for (size_t i = 0; i < bookmark_buttons_.size(); ++i) {
    views::View* child = bookmark_buttons_[i];
    if (!child->GetVisible())
      break;
    if (child->bounds().Contains(adjusted_loc))
      return model_->bookmark_bar_node()->children()[i].get();
  }

  // Then the overflow button.
  if (overflow_button_->GetVisible() &&
      overflow_button_->bounds().Contains(adjusted_loc)) {
    *model_start_index = GetFirstHiddenNodeIndex();
    return model_->bookmark_bar_node();
  }

  // And finally the other folder.
  if (other_bookmarks_button_->GetVisible() &&
      other_bookmarks_button_->bounds().Contains(adjusted_loc)) {
    return model_->other_node();
  }

  return nullptr;
}

views::MenuButton* BookmarkBarView::GetMenuButtonForNode(
    const BookmarkNode* node) {
  if (node == managed_->managed_node())
    return managed_bookmarks_button_;
  if (node == model_->other_node())
    return other_bookmarks_button_;
  if (node == model_->bookmark_bar_node())
    return overflow_button_;
  int index = model_->bookmark_bar_node()->GetIndexOf(node);
  if (index == -1 || !node->is_folder())
    return nullptr;
  return static_cast<views::MenuButton*>(bookmark_buttons_[size_t{index}]);
}

void BookmarkBarView::GetAnchorPositionForButton(
    views::MenuButton* button,
    views::MenuAnchorPosition* anchor) {
  using Position = views::MenuAnchorPosition;
  if (button == other_bookmarks_button_ || button == overflow_button_)
    *anchor = Position::kTopRight;
  else
    *anchor = Position::kTopLeft;
}

views::MenuItemView* BookmarkBarView::GetMenu() {
  return bookmark_menu_ ? bookmark_menu_->menu() : nullptr;
}

views::MenuItemView* BookmarkBarView::GetContextMenu() {
  return bookmark_menu_ ? bookmark_menu_->context_menu() : nullptr;
}

views::MenuItemView* BookmarkBarView::GetDropMenu() {
  return bookmark_drop_menu_ ? bookmark_drop_menu_->menu() : nullptr;
}

void BookmarkBarView::StopThrobbing(bool immediate) {
  if (!throbbing_view_)
    return;

  // If not immediate, cycle through 2 more complete cycles.
  throbbing_view_->StartThrobbing(immediate ? 0 : 4);
  throbbing_view_ = nullptr;
}

// static
base::string16 BookmarkBarView::CreateToolTipForURLAndTitle(
    int max_width,
    const gfx::FontList& tt_fonts,
    const GURL& url,
    const base::string16& title) {
  base::string16 result;

  // First the title.
  if (!title.empty()) {
    base::string16 localized_title = title;
    base::i18n::AdjustStringForLocaleDirection(&localized_title);
    result.append(
        gfx::ElideText(localized_title, tt_fonts, max_width, gfx::ELIDE_TAIL));
  }

  // Only show the URL if the url and title differ.
  if (title != base::UTF8ToUTF16(url.spec())) {
    if (!result.empty())
      result.push_back('\n');

    // We need to explicitly specify the directionality of the URL's text to
    // make sure it is treated as an LTR string when the context is RTL. For
    // example, the URL "http://www.yahoo.com/" appears as
    // "/http://www.yahoo.com" when rendered, as is, in an RTL context since
    // the Unicode BiDi algorithm puts certain characters on the left by
    // default.
    base::string16 elided_url(
        url_formatter::ElideUrl(url, tt_fonts, max_width));
    elided_url = base::i18n::GetDisplayStringInLTRDirectionality(elided_url);
    result.append(elided_url);
  }
  return result;
}

gfx::Size BookmarkBarView::CalculatePreferredSize() const {
  gfx::Size prefsize;
  int preferred_height = GetLayoutConstant(BOOKMARK_BAR_HEIGHT);
  prefsize.set_height(
      static_cast<int>(preferred_height * size_animation_.GetCurrentValue()));
  return prefsize;
}

gfx::Size BookmarkBarView::GetMinimumSize() const {
  // The minimum width of the bookmark bar should at least contain the overflow
  // button, by which one can access all the Bookmark Bar items, and the "Other
  // Bookmarks" folder, along with appropriate margins and button padding.
  // It should also contain the Managed Bookmarks folder, if it is visible.
  int width = kBookmarkBarHorizontalMargin;

  int height = GetLayoutConstant(BOOKMARK_BAR_HEIGHT);

  const int bookmark_bar_button_padding =
      GetLayoutConstant(TOOLBAR_ELEMENT_PADDING);

  if (managed_bookmarks_button_->GetVisible()) {
    gfx::Size size = managed_bookmarks_button_->GetPreferredSize();
    width += size.width() + bookmark_bar_button_padding;
  }
  if (other_bookmarks_button_->GetVisible()) {
    gfx::Size size = other_bookmarks_button_->GetPreferredSize();
    width += size.width() + bookmark_bar_button_padding;
  }
  if (overflow_button_->GetVisible()) {
    gfx::Size size = overflow_button_->GetPreferredSize();
    width += size.width() + bookmark_bar_button_padding;
  }
  if (bookmarks_separator_view_->GetVisible()) {
    gfx::Size size = bookmarks_separator_view_->GetPreferredSize();
    width += size.width();
  }
  if (apps_page_shortcut_->GetVisible()) {
    gfx::Size size = apps_page_shortcut_->GetPreferredSize();
    width += size.width() + bookmark_bar_button_padding;
  }

  return gfx::Size(width, height);
}

void BookmarkBarView::Layout() {
  // Skip layout during destruction, when no model exists.
  if (!model_)
    return;

  int x = kBookmarkBarHorizontalMargin;
  int width = View::width() - 2 * kBookmarkBarHorizontalMargin;

  const int button_height = GetLayoutConstant(BOOKMARK_BAR_BUTTON_HEIGHT);

  // Bookmark bar buttons should be centered between the bottom of the location
  // bar and the bottom of the bookmarks bar, which requires factoring in the
  // bottom margin of the toolbar into the button position.
  int toolbar_bottom_margin = 0;
  // Note: |browser_view_| may be null during tests.
  if (browser_view_ && !browser_view_->IsFullscreen()) {
    toolbar_bottom_margin =
        browser_view_->toolbar()->height() -
        browser_view_->GetLocationBarView()->bounds().bottom();
  }
  // Center the buttons in the total available space.
  const int total_height = GetContentsBounds().height() + toolbar_bottom_margin;
  const int top_margin = (total_height - button_height) / 2;
  // Calculate the top inset in the bookmarks bar itself (not counting the space
  // in the toolbar) but do not allow the buttons to leave the bookmarks bar.
  const int y = std::max(0, top_margin - toolbar_bottom_margin);

  gfx::Size other_bookmarks_pref =
      other_bookmarks_button_->GetVisible()
          ? other_bookmarks_button_->GetPreferredSize()
          : gfx::Size();
  gfx::Size overflow_pref = overflow_button_->GetPreferredSize();
  gfx::Size bookmarks_separator_pref =
      bookmarks_separator_view_->GetPreferredSize();
  gfx::Size apps_page_shortcut_pref =
      apps_page_shortcut_->GetVisible()
          ? apps_page_shortcut_->GetPreferredSize()
          : gfx::Size();

  const int bookmark_bar_button_padding =
      GetLayoutConstant(TOOLBAR_ELEMENT_PADDING);

  int max_x = kBookmarkBarHorizontalMargin + width - overflow_pref.width() -
              bookmarks_separator_pref.width();
  if (other_bookmarks_button_->GetVisible())
    max_x -= other_bookmarks_pref.width() + bookmark_bar_button_padding;

  // Start with the apps page shortcut button.
  if (apps_page_shortcut_->GetVisible()) {
    apps_page_shortcut_->SetBounds(x, y, apps_page_shortcut_pref.width(),
                                   button_height);
    x += apps_page_shortcut_pref.width() + bookmark_bar_button_padding;
  }

  // Then comes the managed bookmarks folder, if visible.
  if (managed_bookmarks_button_->GetVisible()) {
    gfx::Size managed_bookmarks_pref =
        managed_bookmarks_button_->GetPreferredSize();
    managed_bookmarks_button_->SetBounds(x, y, managed_bookmarks_pref.width(),
                                         button_height);
    x += managed_bookmarks_pref.width() + bookmark_bar_button_padding;
  }

  if (model_->loaded() && !model_->bookmark_bar_node()->children().empty()) {
    bool last_visible = x < max_x;
    size_t button_count = bookmark_buttons_.size();
    for (size_t i = 0; i <= button_count; ++i) {
      if (i == button_count) {
        // Add another button if there is room for it (and there is another
        // button to load).
        if (!last_visible || !model_->loaded() ||
            model_->bookmark_bar_node()->children().size() <= button_count)
          break;
        InsertBookmarkButtonAtIndex(
            CreateBookmarkButton(
                model_->bookmark_bar_node()->children()[i].get()),
            i);
        button_count = bookmark_buttons_.size();
      }
      views::View* child = bookmark_buttons_[i];
      gfx::Size pref = child->GetPreferredSize();
      int next_x = x + pref.width() + bookmark_bar_button_padding;
      last_visible = next_x < max_x;
      child->SetVisible(last_visible);
      // Only need to set bounds if the view is actually visible.
      if (last_visible)
        child->SetBounds(x, y, pref.width(), button_height);
      x = next_x;
    }
  }

  // Layout the right side buttons.
  x = max_x + bookmark_bar_button_padding;

  // The overflow button.
  overflow_button_->SetBounds(x, y, overflow_pref.width(), button_height);
  const bool show_overflow =
      model_->loaded() &&
      (model_->bookmark_bar_node()->children().size() >
           bookmark_buttons_.size() ||
       (!bookmark_buttons_.empty() && !bookmark_buttons_.back()->GetVisible()));
  overflow_button_->SetVisible(show_overflow);
  x += overflow_pref.width();

  // Separator.
  if (bookmarks_separator_view_->GetVisible()) {
    bookmarks_separator_view_->SetBounds(x, y, bookmarks_separator_pref.width(),
                                         button_height);

    x += bookmarks_separator_pref.width();
  }

  // The "Other Bookmarks" button.
  if (other_bookmarks_button_->GetVisible()) {
    other_bookmarks_button_->SetBounds(x, y, other_bookmarks_pref.width(),
                                       button_height);
    x += other_bookmarks_pref.width() + bookmark_bar_button_padding;
  }
}

void BookmarkBarView::ViewHierarchyChanged(
    const views::ViewHierarchyChangedDetails& details) {
  if (details.is_add && details.child == this) {
    // We may get inserted into a hierarchy with a profile - this typically
    // occurs when the bar's contents get populated fast enough that the
    // buttons are created before the bar is attached to a frame.
    UpdateAppearanceForTheme();

    if (height() > 0) {
      // We only layout while parented. When we become parented, if our bounds
      // haven't changed, OnBoundsChanged() won't get invoked and we won't
      // layout. Therefore we always force a layout when added.
      Layout();
    }
  }
}

void BookmarkBarView::PaintChildren(const views::PaintInfo& paint_info) {
  View::PaintChildren(paint_info);

  if (drop_info_.get() && drop_info_->valid &&
      drop_info_->location.operation != 0 &&
      drop_info_->location.index.has_value() &&
      drop_info_->location.button_type != DROP_OVERFLOW &&
      !drop_info_->location.on) {
    size_t index = drop_info_->location.index.value();
    DCHECK_LE(index, bookmark_buttons_.size());
    int x = 0;
    int y = 0;
    int h = height();
    if (index == bookmark_buttons_.size()) {
      if (index != 0)
        x = bookmark_buttons_[index - 1]->bounds().right();
      else if (managed_bookmarks_button_->GetVisible())
        x = managed_bookmarks_button_->bounds().right();
      else if (apps_page_shortcut_->GetVisible())
        x = apps_page_shortcut_->bounds().right();
      else
        x = kBookmarkBarHorizontalMargin;
    } else {
      x = bookmark_buttons_[index]->x();
    }
    if (!bookmark_buttons_.empty() && bookmark_buttons_.front()->GetVisible()) {
      y = bookmark_buttons_.front()->y();
      h = bookmark_buttons_.front()->height();
    }

    // Since the drop indicator is painted directly onto the canvas, we must
    // make sure it is painted in the right location if the locale is RTL.
    gfx::Rect indicator_bounds =
        GetMirroredRect(gfx::Rect(x - kBookmarkBarDropIndicatorWidth / 2, y,
                                  kBookmarkBarDropIndicatorWidth, h));

    ui::PaintRecorder recorder(paint_info.context(), size());
    // TODO(sky/glen): make me pretty!
    recorder.canvas()->FillRect(
        indicator_bounds,
        GetThemeProvider()->GetColor(ThemeProperties::COLOR_BOOKMARK_TEXT));
  }
}

bool BookmarkBarView::GetDropFormats(
    int* formats,
    std::set<ui::ClipboardFormatType>* format_types) {
  if (!model_ || !model_->loaded())
    return false;
  *formats = ui::OSExchangeData::URL;
  format_types->insert(BookmarkNodeData::GetBookmarkFormatType());
  return true;
}

bool BookmarkBarView::AreDropTypesRequired() {
  return true;
}

bool BookmarkBarView::CanDrop(const ui::OSExchangeData& data) {
  if (!model_ || !model_->loaded() ||
      !browser_->profile()->GetPrefs()->GetBoolean(
          bookmarks::prefs::kEditBookmarksEnabled))
    return false;

  if (!drop_info_.get())
    drop_info_ = std::make_unique<DropInfo>();

  // Only accept drops of 1 node, which is the case for all data dragged from
  // bookmark bar and menus.
  return drop_info_->data.Read(data) && drop_info_->data.size() == 1;
}

void BookmarkBarView::OnDragEntered(const DropTargetEvent& event) {}

int BookmarkBarView::OnDragUpdated(const DropTargetEvent& event) {
  if (!drop_info_.get())
    return 0;

  if (drop_info_->valid &&
      (drop_info_->x == event.x() && drop_info_->y == event.y())) {
    // The location of the mouse didn't change, return the last operation.
    return drop_info_->location.operation;
  }

  drop_info_->x = event.x();
  drop_info_->y = event.y();

  DropLocation location;
  CalculateDropLocation(event, drop_info_->data, &location);

  if (drop_info_->valid && drop_info_->location.Equals(location)) {
    // The position we're going to drop didn't change, return the last drag
    // operation we calculated. Copy of the operation in case it changed.
    drop_info_->location.operation = location.operation;
    return drop_info_->location.operation;
  }

  StopShowFolderDropMenuTimer();

  // TODO(sky): Optimize paint region.
  SchedulePaint();

  drop_info_->location = location;
  drop_info_->valid = true;

  if (drop_info_->is_menu_showing) {
    if (bookmark_drop_menu_)
      bookmark_drop_menu_->Cancel();
    drop_info_->is_menu_showing = false;
  }

  if (location.on || location.button_type == DROP_OVERFLOW ||
      location.button_type == DROP_OTHER_FOLDER) {
    const BookmarkNode* node;
    if (location.button_type == DROP_OTHER_FOLDER) {
      node = model_->other_node();
    } else if (location.button_type == DROP_OVERFLOW) {
      node = model_->bookmark_bar_node();
    } else {
      node =
          model_->bookmark_bar_node()->children()[location.index.value()].get();
    }
    StartShowFolderDropMenuTimer(node);
  }

  return drop_info_->location.operation;
}

void BookmarkBarView::OnDragExited() {
  StopShowFolderDropMenuTimer();

  // NOTE: we don't hide the menu on exit as it's possible the user moved the
  // mouse over the menu, which triggers an exit on us.

  drop_info_->valid = false;

  if (drop_info_->location.index.has_value()) {
    // TODO(sky): optimize the paint region.
    SchedulePaint();
  }
  drop_info_.reset();
}

int BookmarkBarView::OnPerformDrop(const DropTargetEvent& event) {
  StopShowFolderDropMenuTimer();

  if (bookmark_drop_menu_)
    bookmark_drop_menu_->Cancel();

  if (!drop_info_.get() || !drop_info_->location.operation)
    return ui::DragDropTypes::DRAG_NONE;

  const BookmarkNode* root =
      (drop_info_->location.button_type == DROP_OTHER_FOLDER)
          ? model_->other_node()
          : model_->bookmark_bar_node();

  if (drop_info_->location.index.has_value()) {
    // TODO(sky): optimize the SchedulePaint region.
    SchedulePaint();
  }

  const BookmarkNode* parent_node;
  size_t index;
  if (drop_info_->location.button_type == DROP_OTHER_FOLDER) {
    parent_node = root;
    index = parent_node->children().size();
  } else if (drop_info_->location.on) {
    parent_node = root->children()[drop_info_->location.index.value()].get();
    index = parent_node->children().size();
  } else {
    parent_node = root;
    index = drop_info_->location.index.value();
  }
  const BookmarkNodeData data = drop_info_->data;
  DCHECK(data.is_valid());
  bool copy = drop_info_->location.operation == ui::DragDropTypes::DRAG_COPY;
  drop_info_.reset();

  base::RecordAction(base::UserMetricsAction("BookmarkBar_DragEnd"));
  return chrome::DropBookmarks(browser_->profile(), data, parent_node, index,
                               copy);
}

void BookmarkBarView::OnThemeChanged() {
  UpdateAppearanceForTheme();
}

const char* BookmarkBarView::GetClassName() const {
  return kViewClassName;
}

void BookmarkBarView::VisibilityChanged(View* starting_from, bool is_visible) {
  AccessiblePaneView::VisibilityChanged(starting_from, is_visible);

  if (starting_from == this) {
    for (BookmarkBarViewObserver& observer : observers_)
      observer.OnBookmarkBarVisibilityChanged();
  }
}

void BookmarkBarView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  node_data->role = ax::mojom::Role::kToolbar;
  node_data->SetName(l10n_util::GetStringUTF8(IDS_ACCNAME_BOOKMARKS));
}

void BookmarkBarView::AnimationProgressed(const gfx::Animation* animation) {
  // |browser_view_| can be null during tests.
  if (browser_view_)
    browser_view_->ToolbarSizeChanged(true);
}

void BookmarkBarView::AnimationEnded(const gfx::Animation* animation) {
  // |browser_view_| can be null during tests.
  if (browser_view_) {
    browser_view_->ToolbarSizeChanged(false);
    SchedulePaint();
  }
}

void BookmarkBarView::BookmarkMenuControllerDeleted(
    BookmarkMenuController* controller) {
  if (controller == bookmark_menu_)
    bookmark_menu_ = nullptr;
  else if (controller == bookmark_drop_menu_)
    bookmark_drop_menu_ = nullptr;
}

void BookmarkBarView::OnBookmarkBubbleShown(const BookmarkNode* node) {
  StopThrobbing(true);
  if (!node)
    return;  // Generally shouldn't happen.
  StartThrobbing(node, false);
}

void BookmarkBarView::OnBookmarkBubbleHidden() {
  StopThrobbing(false);
}

void BookmarkBarView::BookmarkModelLoaded(BookmarkModel* model,
                                          bool ids_reassigned) {
  // There should be no buttons. If non-zero it means Load was invoked more than
  // once, or we didn't properly clear things. Either of which shouldn't happen.
  // The actual bookmark buttons are added from Layout().
  DCHECK(bookmark_buttons_.empty());
  DCHECK(model->other_node());
  other_bookmarks_button_->SetAccessibleName(model->other_node()->GetTitle());
  other_bookmarks_button_->SetText(model->other_node()->GetTitle());
  managed_bookmarks_button_->SetAccessibleName(
      managed_->managed_node()->GetTitle());
  managed_bookmarks_button_->SetText(managed_->managed_node()->GetTitle());
  UpdateAppearanceForTheme();
  UpdateOtherAndManagedButtonsVisibility();
  other_bookmarks_button_->SetEnabled(true);
  managed_bookmarks_button_->SetEnabled(true);
  LayoutAndPaint();
}

void BookmarkBarView::BookmarkModelBeingDeleted(BookmarkModel* model) {
  NOTREACHED();
  // Do minimal cleanup, presumably we'll be deleted shortly.
  model_->RemoveObserver(this);
  model_ = nullptr;
}

void BookmarkBarView::BookmarkNodeMoved(BookmarkModel* model,
                                        const BookmarkNode* old_parent,
                                        size_t old_index,
                                        const BookmarkNode* new_parent,
                                        size_t new_index) {
  bool was_throbbing =
      throbbing_view_ &&
      throbbing_view_ == DetermineViewToThrobFromRemove(old_parent, old_index);
  if (was_throbbing)
    throbbing_view_->StopThrobbing();
  bool needs_layout_and_paint =
      BookmarkNodeRemovedImpl(model, old_parent, old_index);
  if (BookmarkNodeAddedImpl(model, new_parent, new_index))
    needs_layout_and_paint = true;
  if (was_throbbing && new_index < bookmark_buttons_.size())
    StartThrobbing(new_parent->children()[new_index].get(), false);
  if (needs_layout_and_paint)
    LayoutAndPaint();
}

void BookmarkBarView::BookmarkNodeAdded(BookmarkModel* model,
                                        const BookmarkNode* parent,
                                        size_t index) {
  if (BookmarkNodeAddedImpl(model, parent, index))
    LayoutAndPaint();
}

void BookmarkBarView::BookmarkNodeRemoved(BookmarkModel* model,
                                          const BookmarkNode* parent,
                                          size_t old_index,
                                          const BookmarkNode* node,
                                          const std::set<GURL>& removed_urls) {
  // Close the menu if the menu is showing for the deleted node.
  if (bookmark_menu_ && bookmark_menu_->node() == node)
    bookmark_menu_->Cancel();
  if (BookmarkNodeRemovedImpl(model, parent, old_index))
    LayoutAndPaint();
}

void BookmarkBarView::BookmarkAllUserNodesRemoved(
    BookmarkModel* model,
    const std::set<GURL>& removed_urls) {
  UpdateOtherAndManagedButtonsVisibility();

  StopThrobbing(true);

  // Remove the existing buttons.
  for (views::LabelButton* button : bookmark_buttons_) {
    delete button;
  }
  bookmark_buttons_.clear();

  LayoutAndPaint();
}

void BookmarkBarView::BookmarkNodeChanged(BookmarkModel* model,
                                          const BookmarkNode* node) {
  BookmarkNodeChangedImpl(model, node);
}

void BookmarkBarView::BookmarkNodeChildrenReordered(BookmarkModel* model,
                                                    const BookmarkNode* node) {
  if (node != model->bookmark_bar_node())
    return;  // We only care about reordering of the bookmark bar node.

  // Remove the existing buttons.
  for (views::LabelButton* button : bookmark_buttons_) {
    delete button;
  }
  bookmark_buttons_.clear();

  // Create the new buttons.
  for (size_t i = 0; i < node->children().size(); ++i) {
    InsertBookmarkButtonAtIndex(CreateBookmarkButton(node->children()[i].get()),
                                i);
  }

  LayoutAndPaint();
}

void BookmarkBarView::BookmarkNodeFaviconChanged(BookmarkModel* model,
                                                 const BookmarkNode* node) {
  BookmarkNodeChangedImpl(model, node);
}

void BookmarkBarView::WriteDragDataForView(View* sender,
                                           const gfx::Point& press_pt,
                                           ui::OSExchangeData* data) {
  base::RecordAction(UserMetricsAction("BookmarkBar_DragButton"));

  const auto* node = GetNodeForSender(sender);
  gfx::ImageSkia icon;
  views::Widget* widget = sender->GetWidget();
  if (node->is_url()) {
    const gfx::Image& image = model_->GetFavicon(node);
    icon = image.IsEmpty() ? favicon::GetDefaultFavicon().AsImageSkia()
                           : image.AsImageSkia();
  } else {
    icon =
        chrome::GetBookmarkFolderIcon(widget->GetNativeTheme()->GetSystemColor(
            ui::NativeTheme::kColorId_LabelEnabledColor));
  }

  button_drag_utils::SetDragImage(node->url(), node->GetTitle(), icon,
                                  &press_pt, *widget, data);
  WriteBookmarkDragData(node, data);
}

int BookmarkBarView::GetDragOperationsForView(View* sender,
                                              const gfx::Point& p) {
  if (size_animation_.is_animating() ||
      size_animation_.GetCurrentValue() == 0) {
    // Don't let the user drag while animating open or we're closed. This
    // typically is only hit if the user does something to inadvertently trigger
    // DnD such as pressing the mouse and hitting control-b.
    return ui::DragDropTypes::DRAG_NONE;
  }

  return chrome::GetBookmarkDragOperation(browser_->profile(),
                                          GetNodeForSender(sender));
}

bool BookmarkBarView::CanStartDragForView(views::View* sender,
                                          const gfx::Point& press_pt,
                                          const gfx::Point& p) {
  // Check if we have not moved enough horizontally but we have moved downward
  // vertically - downward drag.
  gfx::Vector2d move_offset = p - press_pt;
  gfx::Vector2d horizontal_offset(move_offset.x(), 0);
  if (!View::ExceededDragThreshold(horizontal_offset) && move_offset.y() > 0) {
    // If the folder button was dragged, show the menu instead.
    const auto* node = GetNodeForSender(sender);
    if (node->is_folder()) {
      static_cast<views::MenuButton*>(sender)->Activate(nullptr);
      return false;
    }
  }
  return true;
}

void BookmarkBarView::OnMenuButtonPressed(views::Button* view,
                                          const ui::Event& event) {
  const BookmarkNode* node;

  size_t start_index = 0;
  if (view == other_bookmarks_button_) {
    node = model_->other_node();
  } else if (view == managed_bookmarks_button_) {
    node = managed_->managed_node();
  } else if (view == overflow_button_) {
    node = model_->bookmark_bar_node();
    start_index = GetFirstHiddenNodeIndex();
  } else {
    size_t button_index = GetIndexForButton(view);
    DCHECK_NE(size_t{-1}, button_index);
    node = model_->bookmark_bar_node()->children()[button_index].get();
  }

  // Clicking the middle mouse button or clicking with Control/Command key down
  // opens all bookmarks in the folder in new tabs.
  if ((event.flags() & ui::EF_MIDDLE_MOUSE_BUTTON) ||
      (event.flags() & ui::EF_PLATFORM_ACCELERATOR)) {
    WindowOpenDisposition disposition_from_event_flags =
        ui::DispositionFromEventFlags(event.flags());
    RecordBookmarkFolderLaunch(BOOKMARK_LAUNCH_LOCATION_ATTACHED_BAR);
    chrome::OpenAll(GetWidget()->GetNativeWindow(), page_navigator_, node,
                    disposition_from_event_flags, browser_->profile());
  } else {
    RecordBookmarkFolderOpen(BOOKMARK_LAUNCH_LOCATION_ATTACHED_BAR);
    bookmark_menu_ = new BookmarkMenuController(
        browser_, page_navigator_, GetWidget(), node, start_index, false);
    bookmark_menu_->set_observer(this);
    bookmark_menu_->RunMenuAt(this);
  }
}

void BookmarkBarView::OnButtonPressed(views::Button* sender,
                                      const ui::Event& event) {
  WindowOpenDisposition disposition_from_event_flags =
      ui::DispositionFromEventFlags(event.flags());

  if (sender->tag() == kBookmarkBarAppsShortcutButtonTag) {
    OpenURLParams params(GURL(chrome::kChromeUIAppsURL), Referrer(),
                         disposition_from_event_flags,
                         ui::PAGE_TRANSITION_AUTO_BOOKMARK, false);
    page_navigator_->OpenURL(params);
    RecordBookmarkAppsPageOpen(BOOKMARK_LAUNCH_LOCATION_ATTACHED_BAR);
    return;
  }

  size_t index = GetIndexForButton(sender);
  DCHECK_NE(size_t{-1}, index);
  const BookmarkNode* node =
      model_->bookmark_bar_node()->children()[index].get();
  DCHECK(page_navigator_);

  // Only URL nodes have regular buttons on the bookmarks bar; folder clicks
  // are directed to ::OnMenuButtonPressed().
  DCHECK(node->is_url());
  RecordAppLaunch(browser_->profile(), node->url());
  OpenURLParams params(node->url(), Referrer(), disposition_from_event_flags,
                       ui::PAGE_TRANSITION_AUTO_BOOKMARK, false);
  page_navigator_->OpenURL(params);
  RecordBookmarkLaunch(
      BOOKMARK_LAUNCH_LOCATION_ATTACHED_BAR,
      ProfileMetrics::GetBrowserProfileType(browser_->profile()));
}

void BookmarkBarView::ShowContextMenuForViewImpl(
    views::View* source,
    const gfx::Point& point,
    ui::MenuSourceType source_type) {
  if (!model_->loaded()) {
    // Don't do anything if the model isn't loaded.
    return;
  }

  const BookmarkNode* parent = nullptr;
  std::vector<const BookmarkNode*> nodes;
  if (source == other_bookmarks_button_) {
    parent = model_->other_node();
    // Do this so the user can open all bookmarks. BookmarkContextMenu makes
    // sure the user can't edit/delete the node in this case.
    nodes.push_back(parent);
  } else if (source == managed_bookmarks_button_) {
    parent = managed_->managed_node();
    nodes.push_back(parent);
  } else if (source != this && source != apps_page_shortcut_) {
    // User clicked on one of the bookmark buttons, find which one they
    // clicked on, except for the apps page shortcut, which must behave as if
    // the user clicked on the bookmark bar background.
    size_t bookmark_button_index = GetIndexForButton(source);
    DCHECK_NE(size_t{-1}, bookmark_button_index);
    DCHECK_LT(bookmark_button_index, bookmark_buttons_.size());
    const BookmarkNode* node =
        model_->bookmark_bar_node()->children()[bookmark_button_index].get();
    nodes.push_back(node);
    parent = node->parent();
  } else {
    parent = model_->bookmark_bar_node();
    nodes.push_back(parent);
  }
  // |close_on_remove| only matters for nested menus. We're not nested at this
  // point, so this value has no effect.
  const bool close_on_remove = true;

  context_menu_ = std::make_unique<BookmarkContextMenu>(
      GetWidget(), browser_, browser_->profile(),
      browser_->tab_strip_model()->GetActiveWebContents(),
      BOOKMARK_LAUNCH_LOCATION_ATTACHED_BAR, parent, nodes, close_on_remove);
  context_menu_->RunMenuAt(point, source_type);
}

void BookmarkBarView::Init() {
  // Note that at this point we're not in a hierarchy so GetThemeProvider() will
  // return nullptr.  When we're inserted into a hierarchy, we'll call
  // UpdateAppearanceForTheme(), which will set the appropriate colors for all
  // the objects added in this function.

  // Child views are traversed in the order they are added. Make sure the order
  // they are added matches the visual order.
  apps_page_shortcut_ = CreateAppsPageShortcutButton();
  AddChildView(apps_page_shortcut_);

  managed_bookmarks_button_ = CreateManagedBookmarksButton();
  // Also re-enabled when the model is loaded.
  managed_bookmarks_button_->SetEnabled(false);
  AddChildView(managed_bookmarks_button_);

  overflow_button_ = CreateOverflowButton();
  AddChildView(overflow_button_);

  other_bookmarks_button_ = CreateOtherBookmarksButton();
  // We'll re-enable when the model is loaded.
  other_bookmarks_button_->SetEnabled(false);
  AddChildView(other_bookmarks_button_);

  profile_pref_registrar_.Init(browser_->profile()->GetPrefs());
  profile_pref_registrar_.Add(
      bookmarks::prefs::kShowAppsShortcutInBookmarkBar,
      base::Bind(&BookmarkBarView::OnAppsPageShortcutVisibilityPrefChanged,
                 base::Unretained(this)));
  profile_pref_registrar_.Add(
      bookmarks::prefs::kShowManagedBookmarksInBookmarkBar,
      base::Bind(&BookmarkBarView::OnShowManagedBookmarksPrefChanged,
                 base::Unretained(this)));
  apps_page_shortcut_->SetVisible(
      chrome::ShouldShowAppsShortcutInBookmarkBar(browser_->profile()));

  bookmarks_separator_view_ = new ButtonSeparatorView();
  AddChildView(bookmarks_separator_view_);
  UpdateBookmarksSeparatorVisibility();

  set_context_menu_controller(this);

  model_ = BookmarkModelFactory::GetForBrowserContext(browser_->profile());
  managed_ = ManagedBookmarkServiceFactory::GetForProfile(browser_->profile());
  if (model_) {
    model_->AddObserver(this);
    if (model_->loaded())
      BookmarkModelLoaded(model_, false);
    // else case: we'll receive notification back from the BookmarkModel when
    // done loading, then we'll populate the bar.
  }
}

size_t BookmarkBarView::GetFirstHiddenNodeIndex() {
  const auto i =
      std::find_if(bookmark_buttons_.cbegin(), bookmark_buttons_.cend(),
                   [](const auto* button) { return !button->GetVisible(); });
  return i - bookmark_buttons_.cbegin();
}

MenuButton* BookmarkBarView::CreateOtherBookmarksButton() {
  // Title is set in Loaded.
  MenuButton* button =
      new BookmarkFolderButton(base::string16(), &menu_button_listener_);
  button->SetID(VIEW_ID_OTHER_BOOKMARKS);
  button->set_context_menu_controller(this);
  return button;
}

MenuButton* BookmarkBarView::CreateManagedBookmarksButton() {
  // Title is set in Loaded.
  MenuButton* button =
      new BookmarkFolderButton(base::string16(), &menu_button_listener_);
  button->SetID(VIEW_ID_MANAGED_BOOKMARKS);
  button->set_context_menu_controller(this);
  return button;
}

MenuButton* BookmarkBarView::CreateOverflowButton() {
  MenuButton* button = new OverflowButton(this, &menu_button_listener_);

  // The overflow button's image contains an arrow and therefore it is a
  // direction sensitive image and we need to flip it if the UI layout is
  // right-to-left.
  //
  // By default, menu buttons are not flipped because they generally contain
  // text and flipping the gfx::Canvas object will break text rendering. Since
  // the overflow button does not contain text, we can safely flip it.
  button->EnableCanvasFlippingForRTLUI(true);

  // Make visible as necessary.
  button->SetVisible(false);
  // Set accessibility name.
  button->SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_ACCNAME_BOOKMARKS_CHEVRON));
  return button;
}

views::View* BookmarkBarView::CreateBookmarkButton(const BookmarkNode* node) {
  int index = node->parent()->GetIndexOf(node);
  views::LabelButton* button = nullptr;
  if (node->is_url()) {
    button =
        new BookmarkButton(&button_listener_, node->url(), node->GetTitle());
    button->GetViewAccessibility().OverrideDescription(url_formatter::FormatUrl(
        node->url(), url_formatter::kFormatUrlOmitDefaults,
        net::UnescapeRule::SPACES, nullptr, nullptr, nullptr));
  } else {
    button = new BookmarkFolderButton(node->GetTitle(), &menu_button_listener_);
    button->GetViewAccessibility().OverrideDescription("");
  }
  ConfigureButton(node, button);
  bookmark_buttons_.insert(bookmark_buttons_.cbegin() + index, button);
  return button;
}

views::LabelButton* BookmarkBarView::CreateAppsPageShortcutButton() {
  views::LabelButton* button = new ShortcutButton(
      &button_listener_,
      l10n_util::GetStringUTF16(IDS_BOOKMARK_BAR_APPS_SHORTCUT_NAME));
  button->SetTooltipText(
      l10n_util::GetStringUTF16(IDS_BOOKMARK_BAR_APPS_SHORTCUT_TOOLTIP));
  button->SetID(VIEW_ID_BOOKMARK_BAR_ELEMENT);
  button->SetImage(views::Button::STATE_NORMAL,
                   *GetImageSkiaNamed(IDR_BOOKMARK_BAR_APPS_SHORTCUT));
  button->set_context_menu_controller(this);
  button->set_tag(kBookmarkBarAppsShortcutButtonTag);
  return button;
}

void BookmarkBarView::ConfigureButton(const BookmarkNode* node,
                                      views::LabelButton* button) {
  button->SetText(node->GetTitle());
  button->SetAccessibleName(node->GetTitle());
  button->SetID(VIEW_ID_BOOKMARK_BAR_ELEMENT);
  // We don't always have a theme provider (ui tests, for example).
  SkColor text_color = gfx::kPlaceholderColor;
  const ui::ThemeProvider* const tp = GetThemeProvider();
  if (tp) {
    text_color = tp->GetColor(ThemeProperties::COLOR_BOOKMARK_TEXT);
    button->SetEnabledTextColors(text_color);
    if (node->is_folder()) {
      button->SetImage(views::Button::STATE_NORMAL,
                       chrome::GetBookmarkFolderIcon(text_color));
    }
  }

  button->set_context_menu_controller(this);
  button->set_drag_controller(this);
  if (node->is_url()) {
    // Themify chrome:// favicons and the default one. This is similar to
    // code in the tabstrip.
    bool themify_icon = node->url().SchemeIs(content::kChromeUIScheme);
    gfx::ImageSkia favicon = model_->GetFavicon(node).AsImageSkia();
    if (favicon.isNull()) {
      if (MD::touch_ui() && GetThemeProvider()) {
        // This favicon currently does not match the default favicon icon used
        // elsewhere in the codebase.
        // See https://crbug/814447
        const gfx::ImageSkia icon =
            gfx::CreateVectorIcon(kDefaultTouchFaviconIcon, text_color);
        const gfx::ImageSkia mask =
            gfx::CreateVectorIcon(kDefaultTouchFaviconMaskIcon, SK_ColorBLACK);
        favicon = gfx::ImageSkiaOperations::CreateMaskedImage(icon, mask);
      } else {
        favicon = favicon::GetDefaultFavicon().AsImageSkia();
      }
      themify_icon = true;
    }

    if (themify_icon && tp &&
        tp->HasCustomColor(ThemeProperties::COLOR_TOOLBAR_BUTTON_ICON)) {
      favicon = gfx::ImageSkiaOperations::CreateColorMask(
          favicon, tp->GetColor(ThemeProperties::COLOR_TOOLBAR_BUTTON_ICON));
    }

    button->SetImage(views::Button::STATE_NORMAL, favicon);
  }
  button->SetMaxSize(gfx::Size(kBookmarkBarMaxButtonWidth, 0));
}

bool BookmarkBarView::BookmarkNodeAddedImpl(BookmarkModel* model,
                                            const BookmarkNode* parent,
                                            size_t index) {
  const bool needs_layout_and_paint = UpdateOtherAndManagedButtonsVisibility();
  if (parent != model->bookmark_bar_node())
    return needs_layout_and_paint;
  if (index < bookmark_buttons_.size()) {
    const BookmarkNode* node = parent->children()[index].get();
    InsertBookmarkButtonAtIndex(CreateBookmarkButton(node), index);
    return true;
  }
  // If the new node was added after the last button we've created we may be
  // able to fit it. Assume we can by returning true, which forces a Layout()
  // and creation of the button (if it fits).
  return index == bookmark_buttons_.size();
}

bool BookmarkBarView::BookmarkNodeRemovedImpl(BookmarkModel* model,
                                              const BookmarkNode* parent,
                                              size_t index) {
  const bool needs_layout = UpdateOtherAndManagedButtonsVisibility();

  StopThrobbing(true);
  // No need to start throbbing again as the bookmark bubble can't be up at
  // the same time as the user reorders.

  if (parent != model->bookmark_bar_node()) {
    // Only children of the bookmark_bar_node get buttons.
    return needs_layout;
  }
  if (index >= bookmark_buttons_.size())
    return needs_layout;

  views::LabelButton* button = bookmark_buttons_[index];
  bookmark_buttons_.erase(bookmark_buttons_.cbegin() + index);
  delete button;
  return true;
}

void BookmarkBarView::BookmarkNodeChangedImpl(BookmarkModel* model,
                                              const BookmarkNode* node) {
  if (node == managed_->managed_node()) {
    // The managed node may have its title updated.
    managed_bookmarks_button_->SetAccessibleName(
        managed_->managed_node()->GetTitle());
    managed_bookmarks_button_->SetText(managed_->managed_node()->GetTitle());
    return;
  }

  if (node->parent() != model->bookmark_bar_node()) {
    // We only care about nodes on the bookmark bar.
    return;
  }
  int index = model->bookmark_bar_node()->GetIndexOf(node);
  DCHECK_NE(-1, index);
  if (size_t{index} >= bookmark_buttons_.size())
    return;  // Buttons are created as needed.
  views::LabelButton* button = bookmark_buttons_[size_t{index}];
  const int old_pref_width = button->GetPreferredSize().width();
  ConfigureButton(node, button);
  if (old_pref_width != button->GetPreferredSize().width())
    LayoutAndPaint();
}

void BookmarkBarView::ShowDropFolderForNode(const BookmarkNode* node) {
  if (bookmark_drop_menu_) {
    if (bookmark_drop_menu_->node() == node) {
      // Already showing for the specified node.
      return;
    }
    bookmark_drop_menu_->Cancel();
  }

  views::MenuButton* menu_button = GetMenuButtonForNode(node);
  if (!menu_button)
    return;

  size_t start_index = 0;
  if (node == model_->bookmark_bar_node())
    start_index = GetFirstHiddenNodeIndex();

  drop_info_->is_menu_showing = true;
  bookmark_drop_menu_ = new BookmarkMenuController(
      browser_, page_navigator_, GetWidget(), node, start_index, true);
  bookmark_drop_menu_->set_observer(this);
  bookmark_drop_menu_->RunMenuAt(this);

  for (BookmarkBarViewObserver& observer : observers_)
    observer.OnDropMenuShown();
}

void BookmarkBarView::StopShowFolderDropMenuTimer() {
  show_folder_method_factory_.InvalidateWeakPtrs();
}

void BookmarkBarView::StartShowFolderDropMenuTimer(const BookmarkNode* node) {
  if (!animations_enabled) {
    // So that tests can run as fast as possible disable the delay during
    // testing.
    ShowDropFolderForNode(node);
    return;
  }
  show_folder_method_factory_.InvalidateWeakPtrs();
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&BookmarkBarView::ShowDropFolderForNode,
                     show_folder_method_factory_.GetWeakPtr(), node),
      base::TimeDelta::FromMilliseconds(views::GetMenuShowDelay()));
}

void BookmarkBarView::CalculateDropLocation(const DropTargetEvent& event,
                                            const BookmarkNodeData& data,
                                            DropLocation* location) {
  DCHECK(model_);
  DCHECK(model_->loaded());
  DCHECK(data.is_valid());

  *location = DropLocation();

  // The drop event uses the screen coordinates while the child Views are
  // always laid out from left to right (even though they are rendered from
  // right-to-left on RTL locales). Thus, in order to make sure the drop
  // coordinates calculation works, we mirror the event's X coordinate if the
  // locale is RTL.
  int mirrored_x = GetMirroredXInView(event.x());

  bool found = false;
  const int other_delta_x = mirrored_x - other_bookmarks_button_->x();
  Profile* profile = browser_->profile();
  if (other_bookmarks_button_->GetVisible() && other_delta_x >= 0 &&
      other_delta_x < other_bookmarks_button_->width()) {
    // Mouse is over 'other' folder.
    location->button_type = DROP_OTHER_FOLDER;
    location->on = true;
    found = true;
  } else if (bookmark_buttons_.empty()) {
    // No bookmarks, accept the drop.
    location->index = 0;
    const BookmarkNode* node = data.GetFirstNode(model_, profile->GetPath());
    int ops = node && managed_->CanBeEditedByUser(node)
                  ? ui::DragDropTypes::DRAG_MOVE
                  : ui::DragDropTypes::DRAG_COPY | ui::DragDropTypes::DRAG_LINK;
    location->operation = chrome::GetPreferredBookmarkDropOperation(
        event.source_operations(), ops);
    return;
  }

  for (size_t i = 0; i < bookmark_buttons_.size() &&
                     bookmark_buttons_[i]->GetVisible() && !found;
       i++) {
    views::LabelButton* button = bookmark_buttons_[i];
    int button_x = mirrored_x - button->x();
    int button_w = button->width();
    if (button_x < button_w) {
      found = true;
      const BookmarkNode* node =
          model_->bookmark_bar_node()->children()[i].get();
      if (node->is_folder()) {
        if (button_x <= views::kDropBetweenPixels) {
          location->index = i;
        } else if (button_x < button_w - views::kDropBetweenPixels) {
          location->index = i;
          location->on = true;
        } else {
          location->index = i + 1;
        }
      } else if (button_x < button_w / 2) {
        location->index = i;
      } else {
        location->index = i + 1;
      }
      break;
    }
  }

  if (!found) {
    if (overflow_button_->GetVisible()) {
      // Are we over the overflow button?
      int overflow_delta_x = mirrored_x - overflow_button_->x();
      if (overflow_delta_x >= 0 &&
          overflow_delta_x < overflow_button_->width()) {
        // Mouse is over overflow button.
        location->index = GetFirstHiddenNodeIndex();
        location->button_type = DROP_OVERFLOW;
      } else if (overflow_delta_x < 0) {
        // Mouse is after the last visible button but before overflow button;
        // use the last visible index.
        location->index = GetFirstHiddenNodeIndex();
      } else {
        return;
      }
    } else if (!other_bookmarks_button_->GetVisible() ||
               mirrored_x < other_bookmarks_button_->x()) {
      // Mouse is after the last visible button but before more recently
      // bookmarked; use the last visible index.
      location->index = GetFirstHiddenNodeIndex();
    } else {
      return;
    }
  }

  if (location->on) {
    const BookmarkNode* parent = (location->button_type == DROP_OTHER_FOLDER)
                                     ? model_->other_node()
                                     : model_->bookmark_bar_node()
                                           ->children()[location->index.value()]
                                           .get();
    location->operation = chrome::GetBookmarkDropOperation(
        profile, event, data, parent, parent->children().size());
    if (!location->operation && !data.has_single_url() &&
        data.GetFirstNode(model_, profile->GetPath()) == parent) {
      // Don't open a menu if the node being dragged is the menu to open.
      location->on = false;
    }
  } else {
    location->operation = chrome::GetBookmarkDropOperation(
        profile, event, data, model_->bookmark_bar_node(),
        location->index.value());
  }
}

const BookmarkNode* BookmarkBarView::GetNodeForSender(View* sender) const {
  const auto i =
      std::find(bookmark_buttons_.cbegin(), bookmark_buttons_.cend(), sender);
  DCHECK(i != bookmark_buttons_.cend());
  size_t child = i - bookmark_buttons_.cbegin();
  return model_->bookmark_bar_node()->children()[child].get();
}

void BookmarkBarView::WriteBookmarkDragData(const BookmarkNode* node,
                                            ui::OSExchangeData* data) {
  DCHECK(node && data);
  BookmarkNodeData drag_data(node);
  drag_data.Write(browser_->profile()->GetPath(), data);
}

void BookmarkBarView::StartThrobbing(const BookmarkNode* node,
                                     bool overflow_only) {
  DCHECK(!throbbing_view_);

  // Determine which visible button is showing the bookmark (or is an ancestor
  // of the bookmark).
  const BookmarkNode* bbn = model_->bookmark_bar_node();
  const BookmarkNode* parent_on_bb = node;
  while (parent_on_bb) {
    const BookmarkNode* parent = parent_on_bb->parent();
    if (parent == bbn)
      break;
    parent_on_bb = parent;
  }
  if (parent_on_bb) {
    size_t index = size_t{bbn->GetIndexOf(parent_on_bb)};
    if (index >= GetFirstHiddenNodeIndex()) {
      // Node is hidden, animate the overflow button.
      throbbing_view_ = overflow_button_;
    } else if (!overflow_only) {
      throbbing_view_ = static_cast<Button*>(bookmark_buttons_[index]);
    }
  } else if (bookmarks::IsDescendantOf(node, managed_->managed_node())) {
    throbbing_view_ = managed_bookmarks_button_;
  } else if (!overflow_only) {
    throbbing_view_ = other_bookmarks_button_;
  }

  // Use a large number so that the button continues to throb.
  if (throbbing_view_)
    throbbing_view_->StartThrobbing(std::numeric_limits<int>::max());
}

views::Button* BookmarkBarView::DetermineViewToThrobFromRemove(
    const BookmarkNode* parent,
    size_t old_index) {
  const BookmarkNode* bbn = model_->bookmark_bar_node();
  const BookmarkNode* old_node = parent;
  size_t old_index_on_bb = old_index;
  while (old_node && old_node != bbn) {
    const BookmarkNode* parent = old_node->parent();
    if (parent == bbn) {
      old_index_on_bb = size_t{bbn->GetIndexOf(old_node)};
      break;
    }
    old_node = parent;
  }
  if (old_node) {
    if (old_index_on_bb >= GetFirstHiddenNodeIndex()) {
      // Node is hidden, animate the overflow button.
      return overflow_button_;
    }
    return static_cast<Button*>(bookmark_buttons_[old_index_on_bb]);
  }
  if (bookmarks::IsDescendantOf(parent, managed_->managed_node()))
    return managed_bookmarks_button_;
  // Node wasn't on the bookmark bar, use the "Other Bookmarks" button.
  return other_bookmarks_button_;
}

void BookmarkBarView::UpdateAppearanceForTheme() {
  // We don't always have a theme provider (ui tests, for example).
  const ui::ThemeProvider* theme_provider = GetThemeProvider();
  if (!theme_provider)
    return;
  for (size_t i = 0; i < bookmark_buttons_.size(); ++i) {
    ConfigureButton(model_->bookmark_bar_node()->children()[i].get(),
                    bookmark_buttons_[i]);
  }

  const SkColor color =
      theme_provider->GetColor(ThemeProperties::COLOR_BOOKMARK_TEXT);
  other_bookmarks_button_->SetEnabledTextColors(color);
  other_bookmarks_button_->SetImage(views::Button::STATE_NORMAL,
                                    chrome::GetBookmarkFolderIcon(color));
  managed_bookmarks_button_->SetEnabledTextColors(color);
  managed_bookmarks_button_->SetImage(
      views::Button::STATE_NORMAL, chrome::GetBookmarkManagedFolderIcon(color));

  if (apps_page_shortcut_->GetVisible())
    apps_page_shortcut_->SetEnabledTextColors(color);

  const SkColor overflow_color =
      theme_provider->GetColor(ThemeProperties::COLOR_TOOLBAR_BUTTON_ICON);
  overflow_button_->SetImage(
      views::Button::STATE_NORMAL,
      gfx::CreateVectorIcon(
          MD::touch_ui() ? kBookmarkbarTouchOverflowIcon : kOverflowChevronIcon,
          overflow_color));

  // Redraw the background.
  SchedulePaint();
}

bool BookmarkBarView::UpdateOtherAndManagedButtonsVisibility() {
  bool has_other_children = !model_->other_node()->children().empty();
  bool update_other =
      has_other_children != other_bookmarks_button_->GetVisible();
  if (update_other) {
    other_bookmarks_button_->SetVisible(has_other_children);
    UpdateBookmarksSeparatorVisibility();
  }

  bool show_managed = !managed_->managed_node()->children().empty() &&
                      browser_->profile()->GetPrefs()->GetBoolean(
                          bookmarks::prefs::kShowManagedBookmarksInBookmarkBar);
  bool update_managed = show_managed != managed_bookmarks_button_->GetVisible();
  if (update_managed)
    managed_bookmarks_button_->SetVisible(show_managed);

  return update_other || update_managed;
}

void BookmarkBarView::UpdateBookmarksSeparatorVisibility() {
#if defined(OS_CHROMEOS)
  // Ash does not paint the bookmarks separator line because it looks odd on
  // the flat background.  We keep it present for layout, but don't draw it.
  bookmarks_separator_view_->SetVisible(false);
#else
  bookmarks_separator_view_->SetVisible(other_bookmarks_button_->GetVisible());
#endif
}

void BookmarkBarView::OnAppsPageShortcutVisibilityPrefChanged() {
  DCHECK(apps_page_shortcut_);
  // Only perform layout if required.
  bool visible =
      chrome::ShouldShowAppsShortcutInBookmarkBar(browser_->profile());
  if (apps_page_shortcut_->GetVisible() == visible)
    return;
  apps_page_shortcut_->SetVisible(visible);
  UpdateBookmarksSeparatorVisibility();
  LayoutAndPaint();
}

void BookmarkBarView::OnShowManagedBookmarksPrefChanged() {
  if (UpdateOtherAndManagedButtonsVisibility())
    LayoutAndPaint();
}

void BookmarkBarView::InsertBookmarkButtonAtIndex(views::View* button,
                                                  size_t index) {
// All of the secondary buttons are always in the view hierarchy, even if
// they're not visible. The order should be: [Apps shortcut] [Managed bookmark
// button] ..bookmark buttons.. [Overflow chevron] [Other bookmarks]
#if DCHECK_IS_ON()
  auto i = children().cbegin();
  DCHECK_EQ(*i++, apps_page_shortcut_);
  DCHECK_EQ(*i++, managed_bookmarks_button_);
  const auto is_bookmark_button = [this](const auto* v) {
    const char* class_name = v->GetClassName();
    return (class_name == BookmarkButton::kViewClassName ||
            class_name == BookmarkFolderButton::kViewClassName) &&
           v != overflow_button_ && v != other_bookmarks_button_;
  };
  i = std::find_if_not(i, children().cend(), is_bookmark_button);
  DCHECK_EQ(*i++, overflow_button_);
  DCHECK_EQ(*i++, other_bookmarks_button_);
#endif
  AddChildViewAt(button,
                 GetIndexOf(managed_bookmarks_button_) + 1 + int{index});
}

size_t BookmarkBarView::GetIndexForButton(views::View* button) {
  auto it =
      std::find(bookmark_buttons_.cbegin(), bookmark_buttons_.cend(), button);
  if (it == bookmark_buttons_.cend())
    return size_t{-1};

  return size_t{it - bookmark_buttons_.cbegin()};
}
