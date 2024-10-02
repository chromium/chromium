// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bookmarks/bookmark_bar_view.h"

#include <stdint.h>

#include <algorithm>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/callback_list.h"
#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/i18n/rtl.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/notreached.h"
#include "base/observer_list.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/bookmarks/managed_bookmark_service_factory.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/favicon/favicon_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/bookmarks/bookmark_drag_drop.h"
#include "chrome/browser/ui/bookmarks/bookmark_tab_helper.h"
#include "chrome/browser/ui/bookmarks/bookmark_utils.h"
#include "chrome/browser/ui/bookmarks/bookmark_utils_desktop.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_utils.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/tab_group_sync_service_proxy.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_bar_view_observer.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_button.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_button_util.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_context_menu.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_menu_button_base.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_menu_controller_views.h"
#include "chrome/browser/ui/views/bookmarks/saved_tab_groups/saved_tab_group_bar.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/event_utils.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/top_container_background.h"
#include "chrome/browser/ui/views/side_panel/side_panel_ui.h"
#include "chrome/browser/ui/views/side_panel/side_panel_util.h"
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
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/bookmarks/managed/managed_bookmark_service.h"
#include "components/metrics/metrics_service.h"
#include "components/prefs/pref_service.h"
#include "components/profile_metrics/browser_profile_type.h"
#include "components/saved_tab_groups/public/features.h"
#include "components/url_formatter/elide_url.h"
#include "components/url_formatter/url_formatter.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom-shared.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model_utils.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/pointer/touch_ui_controller.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/theme_provider.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/window_open_disposition.h"
#include "ui/base/window_open_disposition_utils.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/events/event_constants.h"
#include "ui/events/types/event_type.h"
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
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_highlight.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/animation/ink_drop_mask.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/button_drag_utils.h"
#include "ui/views/cascading_property.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/label_button_border.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/button/menu_button_controller.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/separator.h"
#include "ui/views/drag_utils.h"
#include "ui/views/metrics.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_constants.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/tooltip_manager.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/non_client_view.h"

namespace {

using ::bookmarks::BookmarkModel;
using ::bookmarks::BookmarkNode;
using ::ui::mojom::DragOperation;
using ::views::Background;
using ::views::Border;
using ::views::LabelButtonBorder;
using ::views::MenuButton;

// Used to globally disable rich animations.
bool animations_enabled = true;

// Thickness of the separator (|) in dips (density-independent pixels).
constexpr int kBookmarkBarSeparatorThickness = 2;

gfx::ImageSkia* GetImageSkiaNamed(int id) {
  return ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(id);
}

const std::u16string& GetFolderButtonAccessibleName(
    const std::u16string& folder_title) {
  static const std::u16string& fallback_name =
      l10n_util::GetStringUTF16(IDS_UNNAMED_BOOKMARK_FOLDER);
  return folder_title.empty() ? fallback_name : folder_title;
}

// ShortcutButton -------------------------------------------------------------

// Buttons used for the shortcuts on the bookmark bar.

class ShortcutButton : public BookmarkButtonBase {
  METADATA_HEADER(ShortcutButton, BookmarkButtonBase)

 public:
  ShortcutButton(PressedCallback callback, const std::u16string& title)
      : BookmarkButtonBase(std::move(callback), title) {}
  ShortcutButton(const ShortcutButton&) = delete;
  ShortcutButton& operator=(const ShortcutButton&) = delete;
};

BEGIN_METADATA(ShortcutButton)
END_METADATA

// BookmarkFolderButton -------------------------------------------------------

// Buttons used for folders on the bookmark bar, including the 'other folders'
// button.
class BookmarkFolderButton : public BookmarkMenuButtonBase {
  METADATA_HEADER(BookmarkFolderButton, BookmarkMenuButtonBase)

 public:
  explicit BookmarkFolderButton(PressedCallback callback,
                                const std::u16string& title = std::u16string())
      : BookmarkMenuButtonBase(std::move(callback), title) {
    show_animation_ = std::make_unique<gfx::SlideAnimation>(this);
    if (!animations_enabled) {
      // For some reason during testing the events generated by animating
      // throw off the test. So, don't animate while testing.
      show_animation_->Reset(1);
    } else {
      show_animation_->Show();
    }
    SetHideInkDropWhenShowingContextMenu(false);

    views::FocusRing::Get(this)->SetOutsetFocusRingDisabled(true);

    SetImageLabelSpacing(
        GetLayoutConstant(BOOKMARK_BAR_BUTTON_IMAGE_LABEL_PADDING));

    // ui::EF_MIDDLE_MOUSE_BUTTON opens all bookmarked links in separate tabs.
    SetTriggerableEventFlags(ui::EF_LEFT_MOUSE_BUTTON |
                             ui::EF_MIDDLE_MOUSE_BUTTON);

    GetViewAccessibility().SetName(GetAccessibleText());
    GetViewAccessibility().SetRoleDescription(l10n_util::GetStringUTF8(
        IDS_ACCNAME_BOOKMARK_FOLDER_BUTTON_ROLE_DESCRIPTION));

    text_changed_callback_ =
        label()->AddTextChangedCallback(base::BindRepeating(
            &BookmarkFolderButton::OnTextChanged, base::Unretained(this)));
  }

  BookmarkFolderButton(const BookmarkFolderButton&) = delete;
  BookmarkFolderButton& operator=(const BookmarkFolderButton&) = delete;

  std::u16string GetTooltipText(const gfx::Point& p) const override {
    return GetAccessibleText();
  }

  bool OnMousePressed(const ui::MouseEvent& event) override {
    if (event.IsOnlyLeftMouseButton()) {
      // TODO(bruthig): The ACTION_PENDING triggering logic should be in
      // MenuButton::OnPressed() however there is a bug with the pressed state
      // logic in MenuButton. See http://crbug.com/567252.
      views::InkDrop::Get(this)->AnimateToState(
          views::InkDropState::ACTION_PENDING, &event);
    }
    return BookmarkMenuButtonBase::OnMousePressed(event);
  }

  void OnTextChanged() { GetViewAccessibility().SetName(GetAccessibleText()); }

  const std::u16string GetAccessibleText() const {
    // If the folder is unnamed, set the name to a default string for unnamed
    // folders; otherwise set the name to the user-supplied folder name.
    return GetText().empty()
               ? l10n_util::GetStringUTF16(IDS_UNNAMED_BOOKMARK_FOLDER)
               : GetText();
  }

 private:
  std::unique_ptr<gfx::SlideAnimation> show_animation_;
  base::CallbackListSubscription text_changed_callback_;
};

BEGIN_METADATA(BookmarkFolderButton)
END_METADATA

// BookmarkTabGroupButton
// -------------------------------------------------------

void RecordAppLaunch(Profile* profile, const GURL& url) {
  const extensions::Extension* extension =
      extensions::ExtensionRegistry::Get(profile)
          ->enabled_extensions()
          .GetAppByURL(url);
  if (!extension) {
    return;
  }

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
  std::optional<size_t> index;

  // Drop constants.
  DragOperation operation = DragOperation::kNone;

  // If true, the user is dropping on a folder.
  bool on = false;

  // Type of button.
  DropButtonType button_type = DROP_BOOKMARK;
};

// DropInfo -------------------------------------------------------------------

// Tracks drops on the BookmarkBarView.

struct BookmarkBarView::DropInfo {
  // Whether the data is valid.
  bool valid = false;

  // If true, the menu is being shown.
  bool is_menu_showing = false;

  // Coordinates of the drag (in terms of the BookmarkBarView).
  int x = 0;
  int y = 0;

  // DropData for the drop.
  bookmarks::BookmarkNodeData data;

  DropLocation location;
};

// ButtonSeparatorView  --------------------------------------------------------

class BookmarkBarView::ButtonSeparatorView : public views::Separator {
  METADATA_HEADER(ButtonSeparatorView, views::Separator)

 public:
  ButtonSeparatorView() {
    const int leading_padding = 8;
    const int trailing_padding = 8;
    separator_thickness_ = kBookmarkBarSeparatorThickness;
    const gfx::Insets border_insets =
        gfx::Insets::TLBR(0, leading_padding, 0, trailing_padding);
    const ui::ColorId color_id = kColorBookmarkBarSeparatorChromeRefresh;

    SetColorId(color_id);
    UpdateBorderAndPreferredSize(border_insets);

    GetViewAccessibility().SetRole(ax::mojom::Role::kSplitter);
    GetViewAccessibility().SetName(
        l10n_util::GetStringUTF8(IDS_ACCNAME_SEPARATOR));
  }
  ButtonSeparatorView(const ButtonSeparatorView&) = delete;
  ButtonSeparatorView& operator=(const ButtonSeparatorView&) = delete;
  ~ButtonSeparatorView() override = default;

  void UpdateBorderAndPreferredSize(gfx::Insets border_insets) {
    SetPreferredSize(gfx::Size(
        border_insets.left() + separator_thickness_ + border_insets.right(),
        gfx::kFaviconSize));

    SetBorder(views::CreateThemedRoundedRectBorder(
        separator_thickness_ / 2, separator_thickness_ / 2, border_insets,
        kColorBookmarkBarSeparatorChromeRefresh));
  }

 private:
  int separator_thickness_;
};

BEGIN_METADATA(BookmarkBarView, ButtonSeparatorView)
END_METADATA

// BookmarkBarView ------------------------------------------------------------

BookmarkBarView::BookmarkBarView(Browser* browser, BrowserView* browser_view)
    : AnimationDelegateViews(this),
      browser_(browser),
      browser_view_(browser_view) {
  SetID(VIEW_ID_BOOKMARK_BAR);
  SetProperty(views::kElementIdentifierKey, kBookmarkBarElementId);

  // TODO(lgrey): This layer was introduced to support clipping the bookmark
  // bar to bounds to prevent it from drawing over the toolbar while animating.
  // This is no longer necessary, so the masking was removed; however removing
  // the layer now makes the animation jerky (or jerkier). The animation should
  // be fixed and, if the layer is no longer necessary, itshould be removed.
  // See https://crbug.com/844037.
  SetPaintToLayer();

  size_animation_.Reset(1);
  if (!gfx::Animation::ShouldRenderRichAnimation()) {
    animations_enabled = false;
  }

  // May be null for tests.
  if (browser_view) {
    SetBackground(std::make_unique<TopContainerBackground>(browser_view));
  }

  views::SetCascadingColorProviderColor(this, views::kCascadingBackgroundColor,
                                        kColorBookmarkBarBackground);
  GetViewAccessibility().SetRole(ax::mojom::Role::kToolbar);
  GetViewAccessibility().SetName(
      l10n_util::GetStringUTF8(IDS_ACCNAME_BOOKMARKS));

  Init();
}

BookmarkBarView::~BookmarkBarView() {
  if (bookmark_model_) {
    bookmark_model_->RemoveObserver(this);
  }

  // It's possible for the menu to outlive us, reset the observer to make sure
  // it doesn't have a reference to us.
  if (bookmark_menu_) {
    bookmark_menu_->set_observer(nullptr);
    bookmark_menu_->clear_bookmark_bar();
  }

  StopShowFolderDropMenuTimer();
}

// static
bool BookmarkBarView::GetAnimationsEnabled() {
  return animations_enabled;
}

void BookmarkBarView::DisableAnimationsForTesting(bool disabled) {
  animations_enabled = !disabled;
}

void BookmarkBarView::AddObserver(BookmarkBarViewObserver* observer) {
  observers_.AddObserver(observer);
}

void BookmarkBarView::RemoveObserver(BookmarkBarViewObserver* observer) {
  observers_.RemoveObserver(observer);
}

void BookmarkBarView::SetPageNavigator(content::PageNavigator* navigator) {
  page_navigator_ = navigator;
  if (saved_tab_group_bar_) {
    saved_tab_group_bar_->SetPageNavigator(navigator);
  }
}

void BookmarkBarView::SetInfoBarVisible(bool infobar_visible) {
  if (infobar_visible == infobar_visible_) {
    return;
  }
  infobar_visible_ = infobar_visible;
  OnPropertyChanged(&infobar_visible_, views::kPropertyEffectsLayout);
}

bool BookmarkBarView::GetInfoBarVisible() const {
  return infobar_visible_;
}

void BookmarkBarView::SetBookmarkBarState(
    BookmarkBar::State state,
    BookmarkBar::AnimateChangeType animate_type) {
  if (animate_type == BookmarkBar::ANIMATE_STATE_CHANGE && animations_enabled) {
    if (state == BookmarkBar::SHOW) {
      size_animation_.Show();
    } else {
      size_animation_.Hide();
    }
  } else {
    size_animation_.Reset(state == BookmarkBar::SHOW ? 1 : 0);
    if (!animations_enabled) {
      AnimationEnded(&size_animation_);
    }
  }

#if BUILDFLAG(IS_MAC)
  // Ensure screen readers can't traverse bookmark bar children while
  // hidden.
  if (state == BookmarkBar::SHOW) {
    GetViewAccessibility().SetIsLeaf(false);
    NotifyAccessibilityEvent(ax::mojom::Event::kTreeChanged, true);
  } else {
    GetViewAccessibility().SetIsLeaf(true);
    NotifyAccessibilityEvent(ax::mojom::Event::kTreeChanged, true);
  }
#endif

  bookmark_bar_state_ = state;
}

const BookmarkNode* BookmarkBarView::GetNodeForButtonAtModelIndex(
    const gfx::Point& loc,
    size_t* model_start_index) {
  *model_start_index = 0;

  if (loc.x() < 0 || loc.x() >= width() || loc.y() < 0 || loc.y() >= height()) {
    return nullptr;
  }

  gfx::Point adjusted_loc(GetMirroredXInView(loc.x()), loc.y());

  // Check the managed button first.
  if (managed_bookmarks_button_->GetVisible() &&
      managed_bookmarks_button_->bounds().Contains(adjusted_loc)) {
    return managed_->managed_node();
  }

  // TODO: add logic to get saved groups node(crbug.com/1223929 and
  // crbug.com/1223919)

  // Then check the bookmark buttons.
  for (size_t i = 0; i < bookmark_buttons_.size(); ++i) {
    views::View* child = bookmark_buttons_[i];
    if (!child->GetVisible()) {
      break;
    }
    if (child->bounds().Contains(adjusted_loc)) {
      return bookmark_model_->bookmark_bar_node()->children()[i].get();
    }
  }

  // Then the overflow button.
  if (overflow_button_->GetVisible() &&
      overflow_button_->bounds().Contains(adjusted_loc)) {
    *model_start_index = GetFirstHiddenNodeIndex();
    return bookmark_model_->bookmark_bar_node();
  }

  // And finally the all bookmarks button.
  if (all_bookmarks_button_->GetVisible() &&
      all_bookmarks_button_->bounds().Contains(adjusted_loc)) {
    return bookmark_model_->other_node();
  }

  return nullptr;
}

MenuButton* BookmarkBarView::GetMenuButtonForNode(const BookmarkNode* node) {
  if (node == managed_->managed_node()) {
    return managed_bookmarks_button_;
  }
  if (node == bookmark_model_->other_node()) {
    return all_bookmarks_button_;
  }
  if (node == bookmark_model_->bookmark_bar_node()) {
    return overflow_button_;
  }
  // TODO: add logic to handle saved groups node(crbug.com/1223929 and
  // crbug.com/1223919)
  std::optional<size_t> index =
      bookmark_model_->bookmark_bar_node()->GetIndexOf(node);
  if (!index.has_value() || !node->is_folder()) {
    return nullptr;
  }
  return static_cast<MenuButton*>(bookmark_buttons_[index.value()]);
}

void BookmarkBarView::GetAnchorPositionForButton(
    MenuButton* button,
    views::MenuAnchorPosition* anchor) {
  using Position = views::MenuAnchorPosition;
  if (button == all_bookmarks_button_ || button == overflow_button_) {
    *anchor = Position::kTopRight;
  } else {
    *anchor = Position::kTopLeft;
  }
}

int BookmarkBarView::GetLeadingMargin() const {
  static constexpr int kBookmarksBarLeadingMarginWithoutSavedTabGroups = 6;
  static constexpr int kBookmarksBarLeadingMarginWithSavedTabGroups = 12;

  if (saved_tab_groups_separator_view_ &&
      saved_tab_groups_separator_view_->GetVisible()) {
    return kBookmarksBarLeadingMarginWithSavedTabGroups;
  }
  return kBookmarksBarLeadingMarginWithoutSavedTabGroups;
}

const views::MenuItemView* BookmarkBarView::GetMenu() const {
  return bookmark_menu_ ? bookmark_menu_->menu() : nullptr;
}

views::MenuItemView* BookmarkBarView::GetContextMenu() {
  return bookmark_menu_ ? bookmark_menu_->context_menu() : nullptr;
}

views::MenuItemView* BookmarkBarView::GetDropMenu() {
  return bookmark_drop_menu_ ? bookmark_drop_menu_->menu() : nullptr;
}

// static
std::u16string BookmarkBarView::CreateToolTipForURLAndTitle(
    int max_width,
    const gfx::FontList& tt_fonts,
    const GURL& url,
    const std::u16string& title) {
  std::u16string result;

  // First the title.
  if (!title.empty()) {
    std::u16string localized_title = title;
    base::i18n::AdjustStringForLocaleDirection(&localized_title);
    result.append(
        gfx::ElideText(localized_title, tt_fonts, max_width, gfx::ELIDE_TAIL));
  }

  // Only show the URL if the url and title differ.
  if (title != base::UTF8ToUTF16(url.spec())) {
    if (!result.empty()) {
      result.push_back('\n');
    }

    // We need to explicitly specify the directionality of the URL's text to
    // make sure it is treated as an LTR string when the context is RTL. For
    // example, the URL "http://www.yahoo.com/" appears as
    // "/http://www.yahoo.com" when rendered, as is, in an RTL context since
    // the Unicode BiDi algorithm puts certain characters on the left by
    // default.
    std::u16string elided_url(
        url_formatter::ElideUrl(url, tt_fonts, max_width));
    elided_url = base::i18n::GetDisplayStringInLTRDirectionality(elided_url);
    result.append(elided_url);
  }
  return result;
}

gfx::Size BookmarkBarView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  gfx::Size prefsize;
  // 34 dp = 28 height + 6px center margin
  int preferred_height = GetLayoutConstant(BOOKMARK_BAR_HEIGHT);
  if (browser_view_ && browser_view_->browser() &&
      browser_view_->browser()->profile() &&
      chrome::ShouldUseCompactMode(browser_view_->browser()->profile())) {
    preferred_height = 30;
  }
  prefsize.set_height(
      static_cast<int>(preferred_height * size_animation_.GetCurrentValue()));
  return prefsize;
}

gfx::Size BookmarkBarView::GetMinimumSize() const {
  // The minimum width of the bookmark bar should at least contain the overflow
  // button, by which one can access all the Bookmark Bar items, and the "All
  // Bookmarks" folder, along with appropriate margins and button padding.
  // It should also contain the Managed Bookmarks folder, if it is visible.
  int width = GetLeadingMargin();

  int height = GetLayoutConstant(BOOKMARK_BAR_HEIGHT);
  if (browser_view_ && browser_view_->browser() &&
      browser_view_->browser()->profile() &&
      chrome::ShouldUseCompactMode(browser_view_->browser()->profile())) {
    height = 30;
  }

  const int bookmark_bar_button_padding =
      GetLayoutConstant(BOOKMARK_BAR_BUTTON_PADDING);

  if (managed_bookmarks_button_->GetVisible()) {
    gfx::Size size = managed_bookmarks_button_->GetPreferredSize();
    width += size.width() + bookmark_bar_button_padding;
  }
  if (all_bookmarks_button_->GetVisible()) {
    gfx::Size size = all_bookmarks_button_->GetPreferredSize();
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
  if (saved_tab_groups_separator_view_ &&
      saved_tab_groups_separator_view_->GetVisible()) {
    gfx::Size size = saved_tab_groups_separator_view_->GetPreferredSize();
    width += size.width();
  }
  if (apps_page_shortcut_->GetVisible()) {
    gfx::Size size = apps_page_shortcut_->GetPreferredSize();
    width += size.width() + bookmark_bar_button_padding;
  }

  return gfx::Size(width, height);
}

// TODO(crbug.com/40648316, crbug.com/330763840): Remove Layout override and
// transition BookmarkBarView to use a layout manager. Afterwards, for most if
// not all calls to LayoutAndPaint can be migrated to only schedule paint and
// not invalidate layout because the layout manager will invalidate layout
// automatically for things like adding and removing child views.
void BookmarkBarView::Layout(PassKey) {
  // Skip layout during destruction, when no model exists.
  if (!bookmark_model_) {
    return;
  }

  int x = GetLeadingMargin();
  static constexpr int kBookmarkBarTrailingMargin = 8;
  int width = View::width() - x - kBookmarkBarTrailingMargin;

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
  const auto center_y = [total_height, toolbar_bottom_margin](int height) {
    const int top_margin = (total_height - height) / 2;
    // Calculate the top inset in the bookmarks bar itself (not counting the
    // space in the toolbar) but do not allow the buttons to leave the bookmarks
    // bar.
    return std::max(0, top_margin - toolbar_bottom_margin);
  };
  const int y = center_y(button_height);

  gfx::Size all_bookmarks_pref = all_bookmarks_button_->GetVisible()
                                     ? all_bookmarks_button_->GetPreferredSize()
                                     : gfx::Size();
  gfx::Size overflow_pref = overflow_button_->GetPreferredSize();
  gfx::Size bookmarks_separator_pref =
      bookmarks_separator_view_->GetPreferredSize();
  gfx::Size apps_page_shortcut_pref =
      apps_page_shortcut_->GetVisible()
          ? apps_page_shortcut_->GetPreferredSize()
          : gfx::Size();

  int bookmark_bar_button_padding =
      GetLayoutConstant(BOOKMARK_BAR_BUTTON_PADDING);
  const bool should_use_compact_mode =
      browser_view_ &&
      chrome::ShouldUseCompactMode(browser_view_->browser()->profile());
  if (should_use_compact_mode) {
    bookmark_bar_button_padding = 0;
  }

  int max_x = GetLeadingMargin() + width - overflow_pref.width() -
              bookmarks_separator_pref.width();
  if (all_bookmarks_button_->GetVisible()) {
    max_x -= all_bookmarks_pref.width() + bookmark_bar_button_padding;
  }

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

  int saved_tab_group_bar_width = 0;
  if (saved_tab_group_bar_ && saved_tab_group_bar_->GetVisible()) {
    // Calculate the maximum size needed for the tab group buttons.
    // In v2 UI we need to allocate space for both saved tab group and
    // bookmarks to prevent one overwhelming the other.
    int saved_tab_groups_bar_available_width;
    // Manually set the overflow(Everything) button's preferred width to be
    // the `button_height` which will be used to calculate the preferred width
    // of `saved_tab_group_bar_` below. Later the overflow button will be laid
    // out with both width and height the same as `button_height` (i.e. the
    // height of `saved_tab_group_bar_`).
    saved_tab_group_bar_->overflow_button()->SetPreferredSize(
        gfx::Size(button_height, button_height));
    if (tab_groups::IsTabGroupsSaveUIUpdateEnabled()) {
      // Calculate the save tab group width without any restriction.
      int saved_tab_group_max_width =
          saved_tab_group_bar_->CalculatePreferredWidthRestrictedBy(INT_MAX);
      // Estimate the bookmark buttons width with existing buttons because not
      // all the bookmarks have button created.
      int estimate_bookmark_buttons_width = 0;
      for (views::LabelButton* button : bookmark_buttons_) {
        estimate_bookmark_buttons_width += button->GetPreferredSize().width();
      }
      int bookmark_count =
          bookmark_model_->loaded()
              ? bookmark_model_->bookmark_bar_node()->children().size()
              : 0;
      int button_count = bookmark_buttons_.size();
      if (button_count > 0) {
        estimate_bookmark_buttons_width *=
            (1.0 * bookmark_count / button_count);
      }
      estimate_bookmark_buttons_width +=
          (bookmark_count - 1) * bookmark_bar_button_padding;

      saved_tab_groups_bar_available_width =
          GetAvailableWidthForSavedTabGroupsBar(
              saved_tab_group_max_width, estimate_bookmark_buttons_width,
              max_x - x -
                  saved_tab_groups_separator_view_->GetPreferredSize().width());
    } else {
      saved_tab_groups_bar_available_width = max_x - x;
    }

    saved_tab_group_bar_width =
        saved_tab_group_bar_->CalculatePreferredWidthRestrictedBy(
            saved_tab_groups_bar_available_width);
    saved_tab_group_bar_->SetBounds(x, y, saved_tab_group_bar_width,
                                    button_height);

    x += saved_tab_group_bar_width;

    // Add the separator width even if its not shown for correct positioning of
    // the bookmark buttons.
    if (saved_tab_group_bar_width > 0) {
      // Possibly update the paddings of the separator
      constexpr int kSeparatorPadding = 8;
      const int left_padding = saved_tab_group_bar_->IsOverflowButtonVisible()
                                   ? 0
                                   : kSeparatorPadding;
      saved_tab_groups_separator_view_->UpdateBorderAndPreferredSize(
          gfx::Insets::TLBR(0, left_padding, 0,
                            should_use_compact_mode ? 0 : kSeparatorPadding));

      // Update the bounds for the separator.
      gfx::Size saved_tab_groups_separator_view_pref =
          saved_tab_groups_separator_view_->GetPreferredSize();
      saved_tab_groups_separator_view_->SetBounds(
          x, center_y(saved_tab_groups_separator_view_pref.height()),
          saved_tab_groups_separator_view_pref.width(),
          saved_tab_groups_separator_view_pref.height());

      // The right padding of the separator is included in the width.
      x += saved_tab_groups_separator_view_pref.width();
    }
  }

  if (bookmark_model_->loaded() &&
      !bookmark_model_->bookmark_bar_node()->children().empty()) {
    bool can_render_button_bounds = x < max_x;
    size_t button_count = bookmark_buttons_.size();
    for (size_t i = 0; i <= button_count; ++i) {
      if (i == button_count) {
        // Add another button if there is room for it (and there is another
        // button to load).
        if (!can_render_button_bounds ||
            bookmark_model_->bookmark_bar_node()->children().size() <=
                button_count) {
          break;
        }
        InsertBookmarkButtonAtIndex(
            CreateBookmarkButton(
                bookmark_model_->bookmark_bar_node()->children()[i].get()),
            i);
        button_count = bookmark_buttons_.size();
      }
      views::View* child = bookmark_buttons_[i];

      // If the child view can fit in the bookmarks comfortably, make it visible
      // and set its bounds.
      gfx::Size pref = child->GetPreferredSize();
      int next_x = x + pref.width() + bookmark_bar_button_padding;
      can_render_button_bounds = next_x < max_x;
      child->SetVisible(can_render_button_bounds);
      // Only need to set bounds if the view is actually visible.
      if (can_render_button_bounds) {
        child->SetBounds(x, y, pref.width(), button_height);
      }
      x = next_x;
    }
  }

  const bool show_bookmarks_overflow =
      bookmark_model_->loaded() &&
      (bookmark_model_->bookmark_bar_node()->children().size() >
           bookmark_buttons_.size() ||
       (!bookmark_buttons_.empty() && !bookmark_buttons_.back()->GetVisible()));

  // Set the visibility of the tab group separator if there are groups and
  // bookmarks.
  if (saved_tab_groups_separator_view_) {
    const bool bookmarks_button_visible =
        !bookmark_buttons_.empty() && bookmark_buttons_.front()->GetVisible();
    saved_tab_groups_separator_view_->SetVisible(
        saved_tab_group_bar_width > 0 &&
        (bookmarks_button_visible || show_bookmarks_overflow));
  }

  // Layout the right side buttons.
  x = max_x + bookmark_bar_button_padding;

  // The overflow button.
  overflow_button_->SetBounds(x, y, overflow_pref.width(), button_height);
  overflow_button_->SetVisible(show_bookmarks_overflow);
  x += overflow_pref.width();

  // Bookmarks Separator.
  if (bookmarks_separator_view_->GetVisible()) {
    bookmarks_separator_view_->SetBounds(
        x, center_y(bookmarks_separator_pref.height()),
        bookmarks_separator_pref.width(), bookmarks_separator_pref.height());

    x += bookmarks_separator_pref.width();
  }

  // The "All Bookmarks" button.
  if (all_bookmarks_button_->GetVisible()) {
    all_bookmarks_button_->SetBounds(x, y, all_bookmarks_pref.width(),
                                     button_height);
    x += all_bookmarks_pref.width();
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
      InvalidateLayout();
    }
  }
}

void BookmarkBarView::PaintChildren(const views::PaintInfo& paint_info) {
  View::PaintChildren(paint_info);

  if (drop_info_ && drop_info_->valid &&
      drop_info_->location.operation != DragOperation::kNone &&
      drop_info_->location.index.has_value() &&
      drop_info_->location.button_type != DROP_OVERFLOW &&
      !drop_info_->location.on) {
    size_t index = drop_info_->location.index.value();
    DCHECK_LE(index, bookmark_buttons_.size());
    int x = 0;
    int y = 0;
    int h = height();
    if (index == bookmark_buttons_.size()) {
      if (index != 0) {
        x = bookmark_buttons_[index - 1]->bounds().right();
      } else if (managed_bookmarks_button_->GetVisible()) {
        x = managed_bookmarks_button_->bounds().right();
      } else if (apps_page_shortcut_->GetVisible()) {
        x = apps_page_shortcut_->bounds().right();
      } else {
        x = GetLeadingMargin();
      }
    } else {
      x = bookmark_buttons_[index]->x();
    }
    if (!bookmark_buttons_.empty() && bookmark_buttons_.front()->GetVisible()) {
      y = bookmark_buttons_.front()->y();
      h = bookmark_buttons_.front()->height();
    }

    // Since the drop indicator is painted directly onto the canvas, we must
    // make sure it is painted in the right location if the locale is RTL.
    constexpr int kDropIndicatorWidth = 2;
    gfx::Rect indicator_bounds = GetMirroredRect(
        gfx::Rect(x - kDropIndicatorWidth / 2, y, kDropIndicatorWidth, h));

    ui::PaintRecorder recorder(paint_info.context(), size());
    // TODO(sky/glen): make me pretty!
    recorder.canvas()->FillRect(
        indicator_bounds,
        GetColorProvider()->GetColor(kColorBookmarkBarForeground));
  }
}

bool BookmarkBarView::GetDropFormats(
    int* formats,
    std::set<ui::ClipboardFormatType>* format_types) {
  if (!bookmark_model_ || !bookmark_model_->loaded()) {
    return false;
  }
  *formats = ui::OSExchangeData::URL;
  format_types->insert(bookmarks::BookmarkNodeData::GetBookmarkFormatType());
  return true;
}

bool BookmarkBarView::AreDropTypesRequired() {
  return true;
}

bool BookmarkBarView::CanDrop(const ui::OSExchangeData& data) {
  if (!bookmark_model_ || !bookmark_model_->loaded() ||
      !browser_->profile()->GetPrefs()->GetBoolean(
          bookmarks::prefs::kEditBookmarksEnabled)) {
    return false;
  }

  if (!drop_info_.get()) {
    drop_info_ = std::make_unique<DropInfo>();
  }

  // Only accept drops of 1 node, which is the case for all data dragged from
  // bookmark bar and menus.
  return drop_info_->data.Read(data) && drop_info_->data.size() == 1;
}

void BookmarkBarView::OnDragEntered(const ui::DropTargetEvent& event) {}

int BookmarkBarView::OnDragUpdated(const ui::DropTargetEvent& event) {
  if (!drop_info_.get()) {
    return 0;
  }

  if (drop_info_->valid &&
      (drop_info_->x == event.x() && drop_info_->y == event.y())) {
    // The location of the mouse didn't change, return the last operation.
    return static_cast<int>(drop_info_->location.operation);
  }

  drop_info_->x = event.x();
  drop_info_->y = event.y();

  DropLocation location;
  CalculateDropLocation(event, drop_info_->data, &location);

  if (drop_info_->valid && drop_info_->location.Equals(location)) {
    // The position we're going to drop didn't change, return the last drag
    // operation we calculated. Copy of the operation in case it changed.
    drop_info_->location.operation = location.operation;
    return static_cast<int>(drop_info_->location.operation);
  }

  StopShowFolderDropMenuTimer();

  // TODO(sky): Optimize paint region.
  SchedulePaint();

  drop_info_->location = location;
  drop_info_->valid = true;

  if (drop_info_->is_menu_showing) {
    if (bookmark_drop_menu_) {
      bookmark_drop_menu_->Cancel();
    }
    drop_info_->is_menu_showing = false;
  }

  if (location.on || location.button_type == DROP_OVERFLOW ||
      location.button_type == DROP_ALL_BOOKMARKS_FOLDER) {
    const BookmarkNode* node;
    if (location.button_type == DROP_ALL_BOOKMARKS_FOLDER) {
      node = bookmark_model_->other_node();
    } else if (location.button_type == DROP_OVERFLOW) {
      node = bookmark_model_->bookmark_bar_node();
    } else {
      node = bookmark_model_->bookmark_bar_node()
                 ->children()[location.index.value()]
                 .get();
    }
    StartShowFolderDropMenuTimer(node);
  }

  return static_cast<int>(drop_info_->location.operation);
}

void BookmarkBarView::OnDragExited() {
  StopShowFolderDropMenuTimer();

  // NOTE: we don't hide the menu on exit as it's possible the user moved the
  // mouse over the menu, which triggers an exit on us.

  if (drop_info_->location.index.has_value()) {
    // TODO(sky): optimize the paint region.
    SchedulePaint();
  }
  drop_info_.reset();
}

views::View::DropCallback BookmarkBarView::GetDropCallback(
    const ui::DropTargetEvent& event) {
  StopShowFolderDropMenuTimer();

  if (bookmark_drop_menu_) {
    bookmark_drop_menu_->Cancel();
  }

  if (!drop_info_ || !drop_info_->valid ||
      drop_info_->location.operation == DragOperation::kNone) {
    return base::NullCallback();
  }

  size_t index = -1;
  const bookmarks::BookmarkNode* parent_node =
      GetParentNodeAndIndexForDrop(index);
  bool copy = drop_info_->location.operation == DragOperation::kCopy;
  bookmarks::BookmarkNodeData drop_data = drop_info_->data;
  drop_info_.reset();
  return base::BindOnce(&BookmarkBarView::PerformDrop,
                        drop_weak_ptr_factory_.GetWeakPtr(),
                        std::move(drop_data), parent_node, index, copy);
}

void BookmarkBarView::OnThemeChanged() {
  views::AccessiblePaneView::OnThemeChanged();
  UpdateAppearanceForTheme();
}

void BookmarkBarView::VisibilityChanged(View* starting_from, bool is_visible) {
  AccessiblePaneView::VisibilityChanged(starting_from, is_visible);

  if (starting_from == this) {
    observers_.Notify(&BookmarkBarViewObserver::OnBookmarkBarVisibilityChanged);
  }
}

void BookmarkBarView::ChildPreferredSizeChanged(views::View* child) {
  // only rerender
  if (child != saved_tab_group_bar_) {
    return;
  }

  InvalidateDrop();
}

void BookmarkBarView::AddedToWidget() {
  MaybeShowSavedTabGroupsIntroPromo();
}

void BookmarkBarView::AnimationProgressed(const gfx::Animation* animation) {
  // |browser_view_| can be null during tests.
  if (browser_view_) {
    browser_view_->ToolbarSizeChanged(true);
  }
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
  if (controller == bookmark_menu_) {
    bookmark_menu_ = nullptr;
  } else if (controller == bookmark_drop_menu_) {
    bookmark_drop_menu_ = nullptr;
  }
}

void BookmarkBarView::BookmarkModelLoaded(bool ids_reassigned) {
  // There should be no buttons. If non-zero it means Load was invoked more than
  // once, or we didn't properly clear things. Either of which shouldn't happen.
  // The actual bookmark buttons are added from Layout().
  DCHECK(bookmark_buttons_.empty());
  const std::u16string all_bookmarks_button_text =
      l10n_util::GetStringUTF16(IDS_BOOKMARKS_ALL_BOOKMARKS);
  all_bookmarks_button_->GetViewAccessibility().SetName(
      all_bookmarks_button_text);
  all_bookmarks_button_->SetText(all_bookmarks_button_text);
  const auto managed_title = managed_->managed_node()->GetTitle();
  managed_bookmarks_button_->GetViewAccessibility().SetName(
      GetFolderButtonAccessibleName(managed_title));
  managed_bookmarks_button_->SetText(managed_title);
  UpdateAppearanceForTheme();
  UpdateOtherAndManagedButtonsVisibility();
  all_bookmarks_button_->SetEnabled(true);
  managed_bookmarks_button_->SetEnabled(true);
  LayoutAndPaint();
}

void BookmarkBarView::BookmarkModelBeingDeleted() {
  NOTREACHED();
}

void BookmarkBarView::BookmarkNodeMoved(const BookmarkNode* old_parent,
                                        size_t old_index,
                                        const BookmarkNode* new_parent,
                                        size_t new_index) {
  // It is extremely rare for the model to mutate during a drop. Rather than
  // trying to validate the location (which may no longer be valid), this takes
  // the simple route of marking the drop as invalid. If the user moves the
  // mouse/touch-device, the location will update accordingly.
  InvalidateDrop();

  bool needs_layout_and_paint = BookmarkNodeRemovedImpl(old_parent, old_index);
  if (BookmarkNodeAddedImpl(new_parent, new_index)) {
    needs_layout_and_paint = true;
  }
  if (needs_layout_and_paint) {
    LayoutAndPaint();
  }

  drop_weak_ptr_factory_.InvalidateWeakPtrs();
}

void BookmarkBarView::BookmarkNodeAdded(const BookmarkNode* parent,
                                        size_t index,
                                        bool added_by_user) {
  // See comment in BookmarkNodeMoved() for details on this.
  InvalidateDrop();
  if (BookmarkNodeAddedImpl(parent, index)) {
    LayoutAndPaint();
  }

  drop_weak_ptr_factory_.InvalidateWeakPtrs();
}

void BookmarkBarView::BookmarkNodeRemoved(const BookmarkNode* parent,
                                          size_t old_index,
                                          const BookmarkNode* node,
                                          const std::set<GURL>& removed_urls,
                                          const base::Location& location) {
  // See comment in BookmarkNodeMoved() for details on this.
  InvalidateDrop();

  // Close the menu if the menu is showing for the deleted node.
  if (bookmark_menu_ && bookmark_menu_->node() == node) {
    bookmark_menu_->Cancel();
  }
  if (BookmarkNodeRemovedImpl(parent, old_index)) {
    LayoutAndPaint();
  }

  drop_weak_ptr_factory_.InvalidateWeakPtrs();
}

void BookmarkBarView::BookmarkAllUserNodesRemoved(
    const std::set<GURL>& removed_urls,
    const base::Location& location) {
  // See comment in BookmarkNodeMoved() for details on this.
  InvalidateDrop();

  UpdateOtherAndManagedButtonsVisibility();

  // Remove the existing buttons.
  for (views::LabelButton* button : bookmark_buttons_) {
    delete button;
  }
  bookmark_buttons_.clear();

  LayoutAndPaint();

  drop_weak_ptr_factory_.InvalidateWeakPtrs();
}

void BookmarkBarView::BookmarkNodeChanged(const BookmarkNode* node) {
  BookmarkNodeChangedImpl(node);

  drop_weak_ptr_factory_.InvalidateWeakPtrs();
}

void BookmarkBarView::BookmarkNodeChildrenReordered(const BookmarkNode* node) {
  // See comment in BookmarkNodeMoved() for details on this.
  InvalidateDrop();

  if (node != bookmark_model_->bookmark_bar_node()) {
    return;  // We only care about reordering of the bookmark bar node.
  }

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

  drop_weak_ptr_factory_.InvalidateWeakPtrs();
}

void BookmarkBarView::BookmarkNodeFaviconChanged(const BookmarkNode* node) {
  BookmarkNodeChangedImpl(node);
}

void BookmarkBarView::WriteDragDataForView(View* sender,
                                           const gfx::Point& press_pt,
                                           ui::OSExchangeData* data) {
  base::RecordAction(base::UserMetricsAction("BookmarkBar_DragButton"));

  const auto* node = GetNodeForSender(sender);
  ui::ImageModel icon;
  if (node->is_url()) {
    const gfx::Image& image = bookmark_model_->GetFavicon(node);
    icon = image.IsEmpty()
               ? favicon::GetDefaultFaviconModel(kColorBookmarkBarBackground)
               : ui::ImageModel::FromImage(image);
  } else {
    icon = chrome::GetBookmarkFolderIcon(
        chrome::BookmarkFolderIconType::kNormal, ui::kColorMenuIcon);
  }

  button_drag_utils::SetDragImage(node->url(), node->GetTitle(),
                                  icon.Rasterize(GetColorProvider()), &press_pt,
                                  data);
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
      static_cast<MenuButton*>(sender)->Activate(nullptr);
      return false;
    }
  }
  return true;
}

void BookmarkBarView::AppsPageShortcutPressed(const ui::Event& event) {
  content::OpenURLParams params(GURL(chrome::kChromeUIAppsURL),
                                content::Referrer(),
                                ui::DispositionFromEventFlags(event.flags()),
                                ui::PAGE_TRANSITION_AUTO_BOOKMARK, false);
  page_navigator_->OpenURL(params, /*navigation_handle_callback=*/{});
  RecordBookmarkAppsPageOpen(BookmarkLaunchLocation::kAttachedBar);
}

void BookmarkBarView::OnButtonPressed(const bookmarks::BookmarkNode* node,
                                      const ui::Event& event) {
  // Only URL nodes have regular buttons on the bookmarks bar; folder clicks
  // are directed to ::OnMenuButtonPressed().
  DCHECK(node->is_url());
  RecordAppLaunch(browser_->profile(), node->url());
  chrome::OpenAllIfAllowed(
      browser_, {node}, ui::DispositionFromEventFlags(event.flags()),
      /*add_to_group=*/false,
      page_load_metrics::NavigationHandleUserData::InitiatorLocation::
          kBookmarkBar,
      {{BookmarkLaunchLocation::kAttachedBar, base::TimeTicks::Now()}});
  RecordBookmarkLaunch(
      BookmarkLaunchLocation::kAttachedBar,
      profile_metrics::GetBrowserProfileType(browser_->profile()));
}

void BookmarkBarView::OnMenuButtonPressed(const bookmarks::BookmarkNode* node,
                                          const ui::Event& event) {
  // Clicking the middle mouse button or clicking with Control/Command key down
  // opens all bookmarks in the folder in new tabs.
  if ((event.flags() & ui::EF_MIDDLE_MOUSE_BUTTON) ||
      (event.flags() & ui::EF_PLATFORM_ACCELERATOR)) {
    RecordBookmarkFolderLaunch(BookmarkLaunchLocation::kAttachedBar);
    chrome::OpenAllIfAllowed(
        browser_, {node}, ui::DispositionFromEventFlags(event.flags()),
        /*add_to_group=*/false,
        page_load_metrics::NavigationHandleUserData::InitiatorLocation::
            kBookmarkBar,
        {{BookmarkLaunchLocation::kAttachedBar, base::TimeTicks::Now()}});
  } else {
    RecordBookmarkFolderOpen(BookmarkLaunchLocation::kAttachedBar);
    const size_t start_index = (node == bookmark_model_->bookmark_bar_node())
                                   ? GetFirstHiddenNodeIndex()
                                   : 0;
    bookmark_menu_ = new BookmarkMenuController(browser_, GetWidget(), node,
                                                start_index, false);
    bookmark_menu_->set_observer(this);
    bookmark_menu_->RunMenuAt(this);
  }
}

void BookmarkBarView::ShowContextMenuForViewImpl(
    views::View* source,
    const gfx::Point& point,
    ui::MenuSourceType source_type) {
  if (!bookmark_model_->loaded()) {
    // Don't do anything if the model isn't loaded.
    return;
  }

  const BookmarkNode* parent = nullptr;
  std::vector<raw_ptr<const BookmarkNode, VectorExperimental>> nodes;
  if (source == all_bookmarks_button_) {
    parent = bookmark_model_->other_node();
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
    DCHECK_NE(static_cast<size_t>(-1), bookmark_button_index);
    DCHECK_LT(bookmark_button_index, bookmark_buttons_.size());
    const BookmarkNode* node = bookmark_model_->bookmark_bar_node()
                                   ->children()[bookmark_button_index]
                                   .get();
    nodes.push_back(node);
    parent = node->parent();
  } else {
    parent = bookmark_model_->bookmark_bar_node();
    nodes.push_back(parent);
  }
  // |close_on_remove| only matters for nested menus. We're not nested at this
  // point, so this value has no effect.
  const bool close_on_remove = true;

  context_menu_ = std::make_unique<BookmarkContextMenu>(
      GetWidget(), browser_, browser_->profile(),
      BookmarkLaunchLocation::kAttachedBar, parent, nodes, close_on_remove);
  context_menu_->RunMenuAt(point, source_type);
}

// Calculate the available width for the saved tab group bar.
// The following rules are applied to prevent one allocate too much space over
// another.
//
// 1. If saved tab group bar and bookmark buttons can both fit into the space,
// allocate as much as they need to both of them.
//
// 2. When saved tab group bar and bookmark buttons can't both fit into the
// space, if one of them is smaller than half of the available width and the
// other is bigger than it, then allocate enough space to fit the smaller one
// and the rest for the bigger one.
//
// 3. When saved tab group bar and bookmark buttons can't both fit into the
// space, if both of them are bigger than half of the available width, then
// allocate half of the available to each of them.
int BookmarkBarView::GetAvailableWidthForSavedTabGroupsBar(
    int saved_tab_group_bar_width,
    int bookmark_buttons_width,
    int available_width) {
  if (saved_tab_group_bar_width + bookmark_buttons_width <= available_width) {
    return available_width;
  } else if (saved_tab_group_bar_width <= available_width / 2) {
    return available_width;
  } else if (bookmark_buttons_width <= available_width / 2) {
    return available_width - bookmark_buttons_width;
  } else {
    return available_width / 2;
  }
}

void BookmarkBarView::Init() {
  // Note that at this point we're not in a hierarchy so GetColorProvider() will
  // return nullptr.  When we're inserted into a hierarchy, we'll call
  // UpdateAppearanceForTheme(), which will set the appropriate colors for all
  // the objects added in this function.

  // Child views are traversed in the order they are added. Make sure the order
  // they are added matches the visual order.
  apps_page_shortcut_ = AddChildView(CreateAppsPageShortcutButton());

  managed_bookmarks_button_ = AddChildView(CreateManagedBookmarksButton());
  // Also re-enabled when the model is loaded.
  managed_bookmarks_button_->SetEnabled(false);

  if (tab_groups::SavedTabGroupUtils::IsEnabledForProfile(
          browser_->profile())) {
    saved_tab_group_bar_ =
        AddChildView(std::make_unique<tab_groups::SavedTabGroupBar>(
            browser_, animations_enabled));
    saved_tab_group_bar_->SetVisible(
        chrome::ShouldShowTabGroupsInBookmarkBar(browser_->profile()));
    saved_tab_groups_separator_view_ =
        AddChildView(std::make_unique<ButtonSeparatorView>());
    saved_tab_groups_separator_view_->SetVisible(true);
  }

  overflow_button_ = AddChildView(CreateOverflowButton());

  all_bookmarks_button_ = AddChildView(CreateAllBookmarksButton());
  // We'll re-enable when the model is loaded.
  all_bookmarks_button_->SetEnabled(false);

  profile_pref_registrar_.Init(browser_->profile()->GetPrefs());
  profile_pref_registrar_.Add(
      bookmarks::prefs::kShowAppsShortcutInBookmarkBar,
      base::BindRepeating(
          &BookmarkBarView::OnAppsPageShortcutVisibilityPrefChanged,
          base::Unretained(this)));

  profile_pref_registrar_.Add(
      bookmarks::prefs::kShowTabGroupsInBookmarkBar,
      base::BindRepeating(&BookmarkBarView::OnTabGroupsVisibilityPrefChanged,
                          base::Unretained(this)));

  profile_pref_registrar_.Add(
      bookmarks::prefs::kShowManagedBookmarksInBookmarkBar,
      base::BindRepeating(&BookmarkBarView::OnShowManagedBookmarksPrefChanged,
                          base::Unretained(this)));

  if (base::FeatureList::IsEnabled(features::kCompactMode)) {
    profile_pref_registrar_.Add(
        prefs::kCompactModeEnabled,
        base::BindRepeating(&BookmarkBarView::OnCompactModeChanged,
                            base::Unretained(this)));
    is_compact_mode_ = chrome::ShouldUseCompactMode(browser_->profile());
  }

  apps_page_shortcut_->SetVisible(
      chrome::ShouldShowAppsShortcutInBookmarkBar(browser_->profile()));

  bookmarks_separator_view_ =
      AddChildView(std::make_unique<ButtonSeparatorView>());
  bookmarks_separator_view_->UpdateBorderAndPreferredSize(
      gfx::Insets::VH(0, 8));

  UpdateBookmarksSeparatorVisibility();

  set_context_menu_controller(this);

  bookmark_model_ =
      BookmarkModelFactory::GetForBrowserContext(browser_->profile());
  managed_ = ManagedBookmarkServiceFactory::GetForProfile(browser_->profile());
  if (bookmark_model_) {
    bookmark_model_->AddObserver(this);
    if (bookmark_model_->loaded()) {
      BookmarkModelLoaded(false);
    }
    // else case: we'll receive notification back from the BookmarkModel when
    // done loading, then we'll populate the bar.
  }
}

size_t BookmarkBarView::GetFirstHiddenNodeIndex() const {
  const auto i = base::ranges::find_if_not(bookmark_buttons_,
                                           &views::LabelButton::GetVisible);
  return i - bookmark_buttons_.cbegin();
}

std::unique_ptr<MenuButton> BookmarkBarView::CreateAllBookmarksButton() {
  auto button = std::make_unique<BookmarkFolderButton>(base::BindRepeating(
      [](BookmarkBarView* bar, const ui::Event& event) {
        bar->OnMenuButtonPressed(bar->bookmark_model_->other_node(), event);
      },
      base::Unretained(this)));
  button->SetID(VIEW_ID_ALL_BOOKMARKS);
  button->set_context_menu_controller(this);
  return button;
}

std::unique_ptr<MenuButton> BookmarkBarView::CreateManagedBookmarksButton() {
  // Title is set in Loaded.
  auto button = std::make_unique<BookmarkFolderButton>(base::BindRepeating(
      [](BookmarkBarView* bar, const ui::Event& event) {
        bar->OnMenuButtonPressed(bar->managed_->managed_node(), event);
      },
      base::Unretained(this)));
  button->SetID(VIEW_ID_MANAGED_BOOKMARKS);
  button->set_context_menu_controller(this);
  return button;
}

std::unique_ptr<MenuButton> BookmarkBarView::CreateOverflowButton() {
  auto button = std::make_unique<BookmarkMenuButtonBase>(base::BindRepeating(
      [](BookmarkBarView* bar, const ui::Event& event) {
        bar->OnMenuButtonPressed(bar->bookmark_model_->bookmark_bar_node(),
                                 event);
      },
      base::Unretained(this)));

  // The overflow button's image contains an arrow and therefore it is a
  // direction sensitive image and we need to flip it if the UI layout is
  // right-to-left.
  //
  // By default, menu buttons are not flipped because they generally contain
  // text and flipping the gfx::Canvas object will break text rendering. Since
  // the overflow button does not contain text, we can safely flip it.
  button->SetFlipCanvasOnPaintForRTLUI(true);
  views::FocusRing::Get(button.get())->SetOutsetFocusRingDisabled(true);
  // Make visible as necessary.
  button->SetVisible(false);
  // Set accessibility name.
  button->GetViewAccessibility().SetName(
      l10n_util::GetStringUTF16(IDS_ACCNAME_BOOKMARKS_CHEVRON));
  button->SetTooltipText(
      l10n_util::GetStringUTF16(IDS_BOOKMARK_BAR_OVERFLOW_BUTTON_TOOLTIP));
  return button;
}

std::unique_ptr<views::View> BookmarkBarView::CreateBookmarkButton(
    const BookmarkNode* node) {
  std::unique_ptr<views::LabelButton> button;
  if (node->is_url()) {
    button = std::make_unique<BookmarkButton>(
        base::BindRepeating(&BookmarkBarView::OnButtonPressed,
                            base::Unretained(this), node),
        node->url(), node->GetTitle(), browser());
    button->GetViewAccessibility().SetDescription(url_formatter::FormatUrl(
        node->url(), url_formatter::kFormatUrlOmitDefaults,
        base::UnescapeRule::SPACES, nullptr, nullptr, nullptr));
  } else {
    button = std::make_unique<BookmarkFolderButton>(
        base::BindRepeating(&BookmarkBarView::OnMenuButtonPressed,
                            base::Unretained(this), node),
        node->GetTitle());
  }
  ConfigureButton(node, button.get());
  size_t index = node->parent()->GetIndexOf(node).value();
  bookmark_buttons_.insert(bookmark_buttons_.cbegin() + index, button.get());
  return button;
}

std::unique_ptr<views::LabelButton>
BookmarkBarView::CreateAppsPageShortcutButton() {
  auto button = std::make_unique<ShortcutButton>(
      base::BindRepeating(&BookmarkBarView::AppsPageShortcutPressed,
                          base::Unretained(this)),
      l10n_util::GetStringUTF16(IDS_BOOKMARK_BAR_APPS_SHORTCUT_NAME));
  button->SetTooltipText(
      l10n_util::GetStringUTF16(IDS_BOOKMARK_BAR_APPS_SHORTCUT_TOOLTIP));
  button->SetID(VIEW_ID_BOOKMARK_BAR_ELEMENT);

  ui::ImageModel icon = ui::ImageModel::FromImageSkia(
      *GetImageSkiaNamed(IDR_BOOKMARK_BAR_APPS_SHORTCUT));
  button->SetImageModel(views::Button::STATE_NORMAL, icon);
  button->SetImageModel(
      views::Button::STATE_DISABLED,
      ui::GetDefaultDisabledIconFromImageModel(icon, GetColorProvider()));

  button->set_context_menu_controller(this);
  return button;
}

void BookmarkBarView::ConfigureButton(const BookmarkNode* node,
                                      views::LabelButton* button) {
  button->SetText(node->GetTitle());
  button->GetViewAccessibility().SetName(node->GetTitle());
  button->SetID(VIEW_ID_BOOKMARK_BAR_ELEMENT);
  // We don't always have a color provider (ui tests, for example).
  SkColor text_color = gfx::kPlaceholderColor;
  const ui::ColorProvider* const cp = GetColorProvider();
  if (cp) {
    text_color = cp->GetColor(kColorBookmarkBarForeground);
    button->SetEnabledTextColors(text_color);
    button->SetTextColorId(views::Button::ButtonState::STATE_DISABLED,
                           kColorBookmarkBarForegroundDisabled);
    if (node->is_folder()) {
      ui::ImageModel icon = chrome::GetBookmarkFolderIcon(
          chrome::BookmarkFolderIconType::kNormal, kColorBookmarkFolderIcon);
      button->SetImageModel(views::Button::STATE_NORMAL, icon);
      button->SetImageModel(
          views::Button::STATE_DISABLED,
          chrome::GetBookmarkFolderIcon(chrome::BookmarkFolderIconType::kNormal,
                                        ui::kColorIconDisabled));
    }
  }

  button->set_context_menu_controller(this);
  button->set_drag_controller(this);
  if (node->is_url()) {
    // Themify chrome:// favicons and the default one. This is similar to
    // code in the tabstrip.
    bool themify_icon = favicon::ShouldThemifyFavicon(node->url());
    // TODO(crbug.com/40137576): BookmarkModel::GetFavicon should be updated to
    // support ImageModel.
    auto favicon = ui::ImageModel::FromImage(bookmark_model_->GetFavicon(node));
    if (favicon.IsEmpty()) {
      if (ui::TouchUiController::Get()->touch_ui() && cp) {
        // This favicon currently does not match the default favicon icon used
        // elsewhere in the codebase.
        // See https://crbug/814447
        const gfx::ImageSkia icon =
            gfx::CreateVectorIcon(kDefaultTouchFaviconIcon, text_color);
        // The color used in `mask` is not relevant as long it is opaque; Only
        // the alpha channel matters.
        const gfx::ImageSkia mask =
            gfx::CreateVectorIcon(kDefaultTouchFaviconMaskIcon, SK_ColorWHITE);
        favicon = ui::ImageModel::FromImageSkia(
            gfx::ImageSkiaOperations::CreateMaskedImage(icon, mask));
      } else {
        favicon = favicon::GetDefaultFaviconModel(kColorBookmarkBarBackground);
      }
      themify_icon = true;
    }

    if (themify_icon && cp) {
      SkColor favicon_color = cp->GetColor(kColorBookmarkFavicon);
      if (favicon_color != SK_ColorTRANSPARENT) {
        favicon = ui::ImageModel::FromImageSkia(
            gfx::ImageSkiaOperations::CreateColorMask(favicon.Rasterize(cp),
                                                      favicon_color));
      }
    }

    button->SetImageModel(
        views::Button::STATE_DISABLED,
        ui::GetDefaultDisabledIconFromImageModel(favicon, GetColorProvider()));
    button->SetImageModel(views::Button::STATE_NORMAL, std::move(favicon));
  }

  button->SetMaxSize(gfx::Size(bookmark_button_util::kMaxButtonWidth, 0));
}

bool BookmarkBarView::BookmarkNodeAddedImpl(const BookmarkNode* parent,
                                            size_t index) {
  const bool needs_layout_and_paint = UpdateOtherAndManagedButtonsVisibility();
  if (parent != bookmark_model_->bookmark_bar_node()) {
    return needs_layout_and_paint;
  }
  if (index < bookmark_buttons_.size()) {
    const BookmarkNode* node = parent->children()[index].get();
    InsertBookmarkButtonAtIndex(CreateBookmarkButton(node), index);
    return true;
  }
  // If the new node was added after the last button we've created we may be
  // able to fit it. Assume we can by returning true, which forces a layout and
  // creation of the button (if it fits).
  return index == bookmark_buttons_.size();
}

bool BookmarkBarView::BookmarkNodeRemovedImpl(const BookmarkNode* parent,
                                              size_t index) {
  const bool needs_layout = UpdateOtherAndManagedButtonsVisibility();

  if (parent != bookmark_model_->bookmark_bar_node()) {
    // Only children of the bookmark_bar_node get buttons.
    return needs_layout;
  }
  if (index >= bookmark_buttons_.size()) {
    return needs_layout;
  }

  views::LabelButton* button = bookmark_buttons_[index];
  bookmark_buttons_.erase(bookmark_buttons_.cbegin() + index);
  // Set not visible before removing to advance focus if needed. See
  // crbug.com/1183980. TODO(crbug.com/40755614): remove this workaround if
  // FocusManager behavior is changed.
  button->SetVisible(false);
  RemoveChildViewT(button);

  return true;
}

void BookmarkBarView::BookmarkNodeChangedImpl(const BookmarkNode* node) {
  if (node == managed_->managed_node()) {
    // The managed node may have its title updated.
    // If the folder is unnamed, set the name to a default string for unnamed
    // folders; otherwise set the name to the user-supplied folder name.
    const auto managed_title = managed_->managed_node()->GetTitle();
    managed_bookmarks_button_->GetViewAccessibility().SetName(
        GetFolderButtonAccessibleName(managed_title));
    managed_bookmarks_button_->SetText(managed_title);
    return;
  }

  if (node->parent() != bookmark_model_->bookmark_bar_node()) {
    // We only care about nodes on the bookmark bar.
    return;
  }
  size_t index = bookmark_model_->bookmark_bar_node()->GetIndexOf(node).value();
  if (index >= bookmark_buttons_.size()) {
    return;  // Buttons are created as needed.
  }
  views::LabelButton* button = bookmark_buttons_[index];
  const int old_pref_width = button->GetPreferredSize().width();
  ConfigureButton(node, button);
  if (old_pref_width != button->GetPreferredSize().width()) {
    LayoutAndPaint();
  }
}

void BookmarkBarView::ShowDropFolderForNode(const BookmarkNode* node) {
  if (bookmark_drop_menu_) {
    if (bookmark_drop_menu_->node() == node) {
      // Already showing for the specified node.
      return;
    }
    bookmark_drop_menu_->Cancel();
  }

  MenuButton* menu_button = GetMenuButtonForNode(node);
  if (!menu_button) {
    return;
  }

  size_t start_index = 0;
  if (node == bookmark_model_->bookmark_bar_node()) {
    start_index = GetFirstHiddenNodeIndex();
  }

  drop_info_->is_menu_showing = true;
  bookmark_drop_menu_ = new BookmarkMenuController(browser_, GetWidget(), node,
                                                   start_index, true);
  bookmark_drop_menu_->set_observer(this);
  bookmark_drop_menu_->RunMenuAt(this);

  observers_.Notify(&BookmarkBarViewObserver::OnDropMenuShown);
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
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&BookmarkBarView::ShowDropFolderForNode,
                     show_folder_method_factory_.GetWeakPtr(), node),
      base::Milliseconds(views::GetMenuShowDelay()));
}

void BookmarkBarView::CalculateDropLocation(
    const ui::DropTargetEvent& event,
    const bookmarks::BookmarkNodeData& data,
    DropLocation* location) {
  DCHECK(bookmark_model_);
  DCHECK(bookmark_model_->loaded());
  DCHECK(data.is_valid());

  *location = DropLocation();

  // The drop event uses the screen coordinates while the child Views are
  // always laid out from left to right (even though they are rendered from
  // right-to-left on RTL locales). Thus, in order to make sure the drop
  // coordinates calculation works, we mirror the event's X coordinate if the
  // locale is RTL.
  int mirrored_x = GetMirroredXInView(event.x());

  bool found = false;
  const int other_delta_x = mirrored_x - all_bookmarks_button_->x();
  Profile* profile = browser_->profile();
  if (all_bookmarks_button_->GetVisible() && other_delta_x >= 0 &&
      other_delta_x < all_bookmarks_button_->width()) {
    // Mouse is over 'other' folder.
    location->button_type = DROP_ALL_BOOKMARKS_FOLDER;
    location->on = true;
    found = true;
  } else if (bookmark_buttons_.empty()) {
    // No bookmarks, accept the drop.
    location->index = 0;
    const BookmarkNode* node =
        data.GetFirstNode(bookmark_model_, profile->GetPath());
    int ops = node && !managed_->IsNodeManaged(node)
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
          bookmark_model_->bookmark_bar_node()->children()[i].get();
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
    } else if (!all_bookmarks_button_->GetVisible() ||
               mirrored_x < all_bookmarks_button_->x()) {
      // Mouse is after the last visible button but before more recently
      // bookmarked; use the last visible index.
      location->index = GetFirstHiddenNodeIndex();
    } else {
      return;
    }
  }

  if (location->on) {
    const BookmarkNode* parent =
        (location->button_type == DROP_ALL_BOOKMARKS_FOLDER)
            ? bookmark_model_->other_node()
            : bookmark_model_->bookmark_bar_node()
                  ->children()[location->index.value()]
                  .get();
    location->operation = chrome::GetBookmarkDropOperation(
        profile, event, data, parent, parent->children().size());
    if (location->operation != DragOperation::kNone && !data.has_single_url() &&
        data.GetFirstNode(bookmark_model_, profile->GetPath()) == parent) {
      // Don't open a menu if the node being dragged is the menu to open.
      location->on = false;
    }
  } else {
    location->operation = chrome::GetBookmarkDropOperation(
        profile, event, data, bookmark_model_->bookmark_bar_node(),
        location->index.value());
  }
}

void BookmarkBarView::InvalidateDrop() {
  if (drop_info_ && drop_info_->valid) {
    drop_info_->valid = false;
    SchedulePaint();
  }
  if (bookmark_drop_menu_) {
    bookmark_drop_menu_->Cancel();
  }
  StopShowFolderDropMenuTimer();
}

const BookmarkNode* BookmarkBarView::GetNodeForSender(View* sender) const {
  const auto i = base::ranges::find(bookmark_buttons_, sender);
  DCHECK(i != bookmark_buttons_.cend());
  size_t child = i - bookmark_buttons_.cbegin();
  return bookmark_model_->bookmark_bar_node()->children()[child].get();
}

void BookmarkBarView::WriteBookmarkDragData(const BookmarkNode* node,
                                            ui::OSExchangeData* data) {
  DCHECK(node && data);
  bookmarks::BookmarkNodeData drag_data(node);
  drag_data.Write(browser_->profile()->GetPath(), data);
}

void BookmarkBarView::UpdateAppearanceForTheme() {
  // We don't always have a color provider (ui tests, for example).
  const ui::ColorProvider* color_provider = GetColorProvider();
  if (!color_provider) {
    return;
  }
  for (size_t i = 0; i < bookmark_buttons_.size(); ++i) {
    ConfigureButton(bookmark_model_->bookmark_bar_node()->children()[i].get(),
                    bookmark_buttons_[i]);
  }

  const SkColor color = color_provider->GetColor(kColorBookmarkBarForeground);
  {
    all_bookmarks_button_->SetEnabledTextColors(color);
    ui::ImageModel all_bookmarks_icon = chrome::GetBookmarkFolderIcon(
        chrome::BookmarkFolderIconType::kNormal, kColorBookmarkFolderIcon);
    all_bookmarks_button_->SetImageModel(views::Button::STATE_NORMAL,
                                         all_bookmarks_icon);
    all_bookmarks_button_->SetImageModel(
        views::Button::STATE_DISABLED,
        chrome::GetBookmarkFolderIcon(chrome::BookmarkFolderIconType::kNormal,
                                      ui::kColorIconDisabled));
  }

  {
    managed_bookmarks_button_->SetEnabledTextColors(color);
    ui::ImageModel managed_bookmarks_button_icon =
        chrome::GetBookmarkFolderIcon(chrome::BookmarkFolderIconType::kManaged,
                                      kColorBookmarkFolderIcon);
    managed_bookmarks_button_->SetImageModel(views::Button::STATE_NORMAL,
                                             managed_bookmarks_button_icon);
    managed_bookmarks_button_->SetImageModel(
        views::Button::STATE_DISABLED,
        chrome::GetBookmarkFolderIcon(chrome::BookmarkFolderIconType::kNormal,
                                      ui::kColorIconDisabled));
  }

  apps_page_shortcut_->SetEnabledTextColors(color);

  const SkColor overflow_color =
      color_provider->GetColor(kColorBookmarkButtonIcon);
  ui::ImageModel overflow_button_icon = ui::ImageModel::FromVectorIcon(
      kBookmarkbarOverflowRefreshIcon, overflow_color);

  overflow_button_->SetImageModel(views::Button::STATE_NORMAL,
                                  overflow_button_icon);
  overflow_button_->SetImageModel(
      views::Button::STATE_DISABLED,
      ui::GetDefaultDisabledIconFromImageModel(overflow_button_icon,
                                               GetColorProvider()));

  // Redraw the background.
  SchedulePaint();
}

bool BookmarkBarView::UpdateOtherAndManagedButtonsVisibility() {
  bool has_other_children = !bookmark_model_->other_node()->children().empty();
  bool update_other = has_other_children != all_bookmarks_button_->GetVisible();
  if (update_other) {
    all_bookmarks_button_->SetVisible(has_other_children);
    UpdateBookmarksSeparatorVisibility();
  }

  bool show_managed = !managed_->managed_node()->children().empty() &&
                      browser_->profile()->GetPrefs()->GetBoolean(
                          bookmarks::prefs::kShowManagedBookmarksInBookmarkBar);
  bool update_managed = show_managed != managed_bookmarks_button_->GetVisible();
  if (update_managed) {
    managed_bookmarks_button_->SetVisible(show_managed);
  }

  return update_other || update_managed;
}

void BookmarkBarView::UpdateBookmarksSeparatorVisibility() {
#if BUILDFLAG(IS_CHROMEOS)
  // ChromeOS does not paint the bookmarks separator line because it looks odd
  // on the flat background. We keep it present for layout, but don't draw it.
  bookmarks_separator_view_->SetVisible(all_bookmarks_button_->GetVisible());
#else
  bookmarks_separator_view_->SetVisible(all_bookmarks_button_->GetVisible());
#endif
}

void BookmarkBarView::OnAppsPageShortcutVisibilityPrefChanged() {
  DCHECK(apps_page_shortcut_);
  // Only perform layout if required.
  bool visible =
      chrome::ShouldShowAppsShortcutInBookmarkBar(browser_->profile());
  if (apps_page_shortcut_->GetVisible() == visible) {
    return;
  }
  apps_page_shortcut_->SetVisible(visible);
  UpdateBookmarksSeparatorVisibility();
  LayoutAndPaint();
}

void BookmarkBarView::OnTabGroupsVisibilityPrefChanged() {
  // Incognito browsers also get triggered if the associated regular profile
  // browser is triggered. Early return because incognito has no
  // `saved_tab_group_bar_`.
  if (!tab_groups::SavedTabGroupUtils::IsEnabledForProfile(
          browser_->profile())) {
    return;
  }

  DCHECK(saved_tab_group_bar_);
  // Only perform layout if required.
  bool visible = chrome::ShouldShowTabGroupsInBookmarkBar(browser_->profile());
  if (saved_tab_group_bar_->GetVisible() == visible) {
    return;
  }
  saved_tab_group_bar_->SetVisible(visible);
  LayoutAndPaint();
}

void BookmarkBarView::OnShowManagedBookmarksPrefChanged() {
  if (UpdateOtherAndManagedButtonsVisibility()) {
    LayoutAndPaint();
  }
}

void BookmarkBarView::OnCompactModeChanged() {
  is_compact_mode_ = !is_compact_mode_;
}

void BookmarkBarView::InsertBookmarkButtonAtIndex(
    std::unique_ptr<views::View> bookmark_button,
    size_t index) {
// All of the secondary buttons are always in the view hierarchy, even if
// they're not visible. The order should be: [Apps shortcut] [Managed bookmark
// button] [saved tab group bar] [..bookmark buttons..] [Overflow chevron]
// [All bookmarks]
#if DCHECK_IS_ON()
  auto i = children().cbegin();
  DCHECK_EQ(*i++, apps_page_shortcut_);
  DCHECK_EQ(*i++, managed_bookmarks_button_);
  if (saved_tab_group_bar_) {
    DCHECK_EQ(*i++, saved_tab_group_bar_);
  }
  if (saved_tab_groups_separator_view_) {
    DCHECK_EQ(*i++, saved_tab_groups_separator_view_);
  }
  const auto is_bookmark_button = [this](const View* v) {
    return (views::IsViewClass<BookmarkButton>(v) ||
            views::IsViewClass<BookmarkFolderButton>(v)) &&
           v != overflow_button_ && v != all_bookmarks_button_;
  };
  i = std::find_if_not(i, children().cend(), is_bookmark_button);
  DCHECK_EQ(*i++, overflow_button_);
  DCHECK_EQ(*i++, all_bookmarks_button_);
#endif
  const auto view_index =
      saved_tab_groups_separator_view_
          ? GetIndexOf(saved_tab_groups_separator_view_).value()
          : GetIndexOf(managed_bookmarks_button_).value();
  AddChildViewAt(std::move(bookmark_button), view_index + 1 + index);
}

size_t BookmarkBarView::GetIndexForButton(views::View* button) {
  auto it = base::ranges::find(bookmark_buttons_, button);
  if (it == bookmark_buttons_.cend()) {
    return static_cast<size_t>(-1);
  }

  return static_cast<size_t>(it - bookmark_buttons_.cbegin());
}

const BookmarkNode* BookmarkBarView::GetParentNodeAndIndexForDrop(
    size_t& index) {
  const BookmarkNode* root =
      (drop_info_->location.button_type == DROP_ALL_BOOKMARKS_FOLDER)
          ? bookmark_model_->other_node()
          : bookmark_model_->bookmark_bar_node();

  if (drop_info_->location.index.has_value()) {
    // TODO(sky): optimize the SchedulePaint region.
    SchedulePaint();
  }

  const BookmarkNode* parent_node;
  if (drop_info_->location.button_type == DROP_ALL_BOOKMARKS_FOLDER) {
    parent_node = root;
    index = parent_node->children().size();
  } else if (drop_info_->location.on) {
    parent_node = root->children()[drop_info_->location.index.value()].get();
    index = parent_node->children().size();
  } else {
    parent_node = root;
    index = drop_info_->location.index.value();
  }
  return parent_node;
}

void BookmarkBarView::PerformDrop(
    const bookmarks::BookmarkNodeData data,
    const BookmarkNode* parent_node,
    const size_t index,
    const bool copy,
    const ui::DropTargetEvent& event,
    ui::mojom::DragOperation& output_drag_op,
    std::unique_ptr<ui::LayerTreeOwner> drag_image_layer_owner) {
  DCHECK(data.is_valid());
  DCHECK(parent_node);
  DCHECK_NE(index, static_cast<size_t>(-1));

  base::RecordAction(base::UserMetricsAction("BookmarkBar_DragEnd"));
  output_drag_op = chrome::DropBookmarks(
      browser_->profile(), data, parent_node, index, copy,
      chrome::BookmarkReorderDropTarget::kBookmarkBarView);
}

int BookmarkBarView::GetDropLocationModelIndexForTesting() const {
  return (drop_info_ && drop_info_->valid &&
          drop_info_->location.index.has_value())
             ? *(drop_info_->location.index)
             : -1;
}
const views::View* BookmarkBarView::GetSavedTabGroupsSeparatorViewForTesting()
    const {
  return saved_tab_groups_separator_view_;
}

void BookmarkBarView::MaybeShowSavedTabGroupsIntroPromo() const {
  // Only show this promo with the V2 enabled flag.
  if (!tab_groups::IsTabGroupsSaveV2Enabled()) {
    return;
  }

  // Check whether to show the synced, or unsyned version of the promo.
  tab_groups::TabGroupSyncService* tab_group_service =
      tab_groups::SavedTabGroupUtils::GetServiceForProfile(browser_->profile());
  if (!tab_group_service) {
    return;
  }

  // In order for IPH's for V2 to show up, there must be at least 1 group.
  if (tab_group_service->GetAllGroups().empty()) {
    return;
  }

  user_education::FeaturePromoParams params(
      feature_engagement::kIPHTabGroupsSaveV2IntroFeature);

  const bool everything_button_is_visible =
      saved_tab_group_bar_ && saved_tab_group_bar_->GetVisible();

  // TODO (crbug.com/343258921) Once A11y text is supported with string
  // substitutions, add them in.

  // If tabs groups are syncing...
  if (tab_groups::SavedTabGroupUtils::AreSavedTabGroupsSyncedForProfile(
          browser()->profile())) {
    // Anchor the IPH to the bookmarks bar if the everything button is visible.
    // Otherwise, anchor to the AppMenu.
    if (everything_button_is_visible) {
      params.body_params = {l10n_util::GetStringUTF16(
          IDS_SAVED_TAB_GROUPS_V2_INTRO_IPH_BOOKMARKS_BAR_SYNCED_BODY)};
    } else {
      params.body_params = {l10n_util::GetStringUTF16(
          IDS_SAVED_TAB_GROUPS_V2_INTRO_IPH_APP_MENU_SYNCED_BODY)};
    }
  } else {
    // Anchor the IPH to the bookmarks bar if the everything button is visible.
    // Otherwise, anchor to the AppMenu.
    if (everything_button_is_visible) {
      params.body_params =
          std::vector<std::u16string>{l10n_util::GetStringUTF16(
              IDS_SAVED_TAB_GROUPS_V2_INTRO_IPH_BOOKMARKS_BAR_NOT_SYNCED_BODY)};
    } else {
      params.body_params =
          std::vector<std::u16string>{l10n_util::GetStringUTF16(
              IDS_SAVED_TAB_GROUPS_V2_INTRO_IPH_APP_MENU_NOT_SYNCED_BODY)};
    }
  }

  browser_view_->MaybeShowStartupFeaturePromo(std::move(params));
}

BEGIN_METADATA(BookmarkBarView)
ADD_PROPERTY_METADATA(bool, InfoBarVisible)
ADD_READONLY_PROPERTY_METADATA(size_t, FirstHiddenNodeIndex)
END_METADATA
