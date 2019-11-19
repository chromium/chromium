// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/app_menu.h"

#include <stdint.h>

#include <algorithm>
#include <cmath>
#include <set>

#include "base/bind.h"
#include "base/i18n/number_formatting.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/utf_string_conversions.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/ui/bookmarks/bookmark_stats.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/global_error/global_error_service_factory.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/toolbar/app_menu_model.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_menu_delegate.h"
#include "chrome/browser/ui/views/frame/app_menu_button.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/toolbar/extension_toolbar_menu_view.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/zoom/page_zoom.h"
#include "components/zoom/zoom_controller.h"
#include "components/zoom/zoom_event_manager.h"
#include "content/public/browser/host_zoom_map.h"
#include "content/public/browser/web_contents.h"
#include "extensions/common/feature_switch.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/layout.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/gfx/skia_util.h"
#include "ui/gfx/text_utils.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/menu/menu_config.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/menu_model_adapter.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/controls/menu/menu_scroll_view_container.h"
#include "ui/views/controls/menu/submenu_view.h"
#include "ui/views/widget/widget.h"

using base::UserMetricsAction;
using bookmarks::BookmarkModel;
using content::WebContents;
using ui::ButtonMenuItemModel;
using ui::MenuModel;
using views::Button;
using views::ImageButton;
using views::Label;
using views::LabelButton;
using views::MenuConfig;
using views::MenuItemView;
using views::View;

namespace {

// Horizontal padding on the edges of the in-menu buttons.
const int kHorizontalPadding = 15;

#if defined(OS_CHROMEOS)
// Extra horizontal space to reserve for the fullscreen button.
const int kFullscreenPadding = 74;
// Padding to left and right of the XX% label.
const int kZoomLabelHorizontalPadding = kHorizontalPadding;
#else
const int kFullscreenPadding = 38;
const int kZoomLabelHorizontalPadding = 2;
#endif

// Returns true if |command_id| identifies a bookmark menu item.
bool IsBookmarkCommand(int command_id) {
  return command_id >= IDC_FIRST_BOOKMARK_MENU;
}

// Returns true if |command_id| identifies a recent tabs menu item.
bool IsRecentTabsCommand(int command_id) {
  return command_id >= AppMenuModel::kMinRecentTabsCommandId &&
         command_id <= AppMenuModel::kMaxRecentTabsCommandId;
}

// Subclass of ImageButton whose preferred size includes the size of the border.
class FullscreenButton : public ImageButton {
 public:
  explicit FullscreenButton(views::ButtonListener* listener)
      : ImageButton(listener) { }
  FullscreenButton(const FullscreenButton&) = delete;
  FullscreenButton& operator=(const FullscreenButton&) = delete;

  // Overridden from ImageButton.
  gfx::Size CalculatePreferredSize() const override {
    gfx::Size pref = ImageButton::CalculatePreferredSize();
    if (border()) {
      gfx::Insets insets = border()->GetInsets();
      pref.Enlarge(insets.width(), insets.height());
    }
    return pref;
  }

  void GetAccessibleNodeData(ui::AXNodeData* node_data) override {
    ImageButton::GetAccessibleNodeData(node_data);
    node_data->role = ax::mojom::Role::kMenuItem;
  }
};

// Combination border/background for the buttons contained in the menu. The
// painting of the border/background is done here as LabelButton does not always
// paint the border.
class InMenuButtonBackground : public views::Background {
 public:
  enum ButtonType {
    // A rectangular button with no drawn border.
    NO_BORDER,

    // A rectangular button with a border drawn along the leading (left) side.
    LEADING_BORDER,

    // A button with no drawn border and a rounded background.
    ROUNDED_BUTTON,
  };

  explicit InMenuButtonBackground(ButtonType type) : type_(type) {}
  InMenuButtonBackground(const InMenuButtonBackground&) = delete;
  InMenuButtonBackground& operator=(const InMenuButtonBackground&) = delete;

  // Overridden from views::Background.
  void Paint(gfx::Canvas* canvas, View* view) const override {
    Button* button = Button::AsButton(view);
    int h = view->height();

    // Draw leading border if desired.
    gfx::Rect bounds(view->GetLocalBounds());
    if (type_ == LEADING_BORDER) {
      // We need to flip the canvas for RTL iff the button is not auto-flipping
      // already, so we end up flipping exactly once.
      gfx::ScopedCanvas scoped_canvas(canvas);
      if (!view->flip_canvas_on_paint_for_rtl_ui())
        scoped_canvas.FlipIfRTL(view->width());
      ui::NativeTheme::ExtraParams params;
      gfx::Rect separator_bounds =
          gfx::Rect(0, 0, MenuConfig::instance().separator_thickness, h);
      params.menu_separator.paint_rect = &separator_bounds;
      params.menu_separator.type = ui::VERTICAL_SEPARATOR;
      view->GetNativeTheme()->Paint(
          canvas->sk_canvas(), ui::NativeTheme::kMenuPopupSeparator,
          ui::NativeTheme::kNormal, separator_bounds, params);
      bounds.Inset(
          gfx::Insets(0, MenuConfig::instance().separator_thickness, 0, 0));
    }

    // Fill in background for state.
    views::Button::ButtonState state =
        button ? button->state() : views::Button::STATE_NORMAL;
    DrawBackground(canvas, view, view->GetMirroredRect(bounds), state);
  }

 private:
  static SkColor BackgroundColor(const View* view,
                                 views::Button::ButtonState state) {
    const ui::NativeTheme* theme = view->GetNativeTheme();
    switch (state) {
      case views::Button::STATE_PRESSED:
        return theme->GetSystemColor(
            ui::NativeTheme::kColorId_FocusedMenuItemBackgroundColor);
      case views::Button::STATE_HOVERED:
        // Hovered should be handled in DrawBackground.
        NOTREACHED();
        FALLTHROUGH;
      default:
        return theme->GetSystemColor(
            ui::NativeTheme::kColorId_MenuBackgroundColor);
    }
  }

  void DrawBackground(gfx::Canvas* canvas,
                      const views::View* view,
                      const gfx::Rect& bounds,
                      views::Button::ButtonState state) const {
    if (state == views::Button::STATE_HOVERED ||
        state == views::Button::STATE_PRESSED) {
      ui::NativeTheme::ExtraParams params;
      if (type_ == ROUNDED_BUTTON) {
        // Consistent with a hover corner radius (kInkDropSmallCornerRadius).
        const int kBackgroundCornerRadius = 2;
        params.menu_item.corner_radius = kBackgroundCornerRadius;
      }
      view->GetNativeTheme()->Paint(canvas->sk_canvas(),
                                    ui::NativeTheme::kMenuItemBackground,
                                    ui::NativeTheme::kHovered, bounds, params);
    }
  }

  const ButtonType type_;
};

base::string16 GetAccessibleNameForAppMenuItem(ButtonMenuItemModel* model,
                                               int item_index,
                                               int accessible_string_id,
                                               bool add_accelerator_text) {
  base::string16 accessible_name =
      l10n_util::GetStringUTF16(accessible_string_id);
  base::string16 accelerator_text;

  ui::Accelerator menu_accelerator;
  if (add_accelerator_text &&
      model->GetAcceleratorAt(item_index, &menu_accelerator)) {
    accelerator_text =
        ui::Accelerator(menu_accelerator.key_code(),
                        menu_accelerator.modifiers()).GetShortcutText();
  }

  return MenuItemView::GetAccessibleNameForMenuItem(
      accessible_name, accelerator_text);
}

// A button that lives inside a menu item.
class InMenuButton : public LabelButton {
 public:
  InMenuButton(views::ButtonListener* listener, const base::string16& text)
      : LabelButton(listener, text) {}
  InMenuButton(const InMenuButton&) = delete;
  InMenuButton& operator=(const InMenuButton&) = delete;

  ~InMenuButton() override {}

  void Init(InMenuButtonBackground::ButtonType type) {
    // An InMenuButton should always be focusable regardless of the platform.
    // Hence we don't use SetFocusForPlatform().
    SetFocusBehavior(FocusBehavior::ALWAYS);
    SetHorizontalAlignment(gfx::ALIGN_CENTER);

    SetBackground(std::make_unique<InMenuButtonBackground>(type));
    SetBorder(
        views::CreateEmptyBorder(0, kHorizontalPadding, 0, kHorizontalPadding));
    label()->SetFontList(MenuConfig::instance().font_list);
  }

  void GetAccessibleNodeData(ui::AXNodeData* node_data) override {
    LabelButton::GetAccessibleNodeData(node_data);
    node_data->role = ax::mojom::Role::kMenuItem;
  }

  // views::LabelButton
  void OnThemeChanged() override {
    ui::NativeTheme* theme = GetNativeTheme();
    if (theme) {
      SetTextColor(
          views::Button::STATE_DISABLED,
          theme->GetSystemColor(
              ui::NativeTheme::kColorId_DisabledMenuItemForegroundColor));
      SetTextColor(
          views::Button::STATE_HOVERED,
          theme->GetSystemColor(
              ui::NativeTheme::kColorId_SelectedMenuItemForegroundColor));
      SetTextColor(
          views::Button::STATE_PRESSED,
          theme->GetSystemColor(
              ui::NativeTheme::kColorId_SelectedMenuItemForegroundColor));
      SetTextColor(
          views::Button::STATE_NORMAL,
          theme->GetSystemColor(
              ui::NativeTheme::kColorId_EnabledMenuItemForegroundColor));
    }
  }
};

// AppMenuView is a view that can contain label buttons.
class AppMenuView : public views::View, public views::ButtonListener {
 public:
  AppMenuView(AppMenu* menu, ButtonMenuItemModel* menu_model)
      : menu_(menu->AsWeakPtr()), menu_model_(menu_model) {}
  AppMenuView(const AppMenuView&) = delete;
  AppMenuView& operator=(const AppMenuView&) = delete;

  ~AppMenuView() override = default;

  // Overridden from views::View.
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override {
    views::View::GetAccessibleNodeData(node_data);
    node_data->role = ax::mojom::Role::kMenu;
  }

  InMenuButton* CreateAndConfigureButton(
      int string_id,
      InMenuButtonBackground::ButtonType type,
      int index) {
    return CreateButtonWithAccName(string_id, type, index, string_id,
                                   /*add_accelerator_text*/ true);
  }

  InMenuButton* CreateButtonWithAccName(int string_id,
                                        InMenuButtonBackground::ButtonType type,
                                        int index,
                                        int acc_string_id,
                                        bool add_accelerator_text) {
    // Should only be invoked during construction when |menu_| is valid.
    DCHECK(menu_);
    InMenuButton* button = new InMenuButton(
        this, gfx::RemoveAcceleratorChar(l10n_util::GetStringUTF16(string_id),
                                         '&', nullptr, nullptr));
    button->Init(type);
    button->SetAccessibleName(GetAccessibleNameForAppMenuItem(
        menu_model_, index, acc_string_id, add_accelerator_text));
    button->set_tag(index);
    button->SetEnabled(menu_model_->IsEnabledAt(index));

    AddChildView(button);
    // all buttons on menu should must be a custom button in order for
    // the keyboard nativigation work.
    DCHECK(Button::AsButton(button));
    return button;
  }

 protected:
  base::WeakPtr<AppMenu> menu() { return menu_; }
  base::WeakPtr<const AppMenu> menu() const { return menu_; }
  ButtonMenuItemModel* menu_model() {
    // The menu and the items in the menu model have similar lifetimes; it's
    // only safe to access model items if the menu is still alive.
    DCHECK(menu_);
    return menu_model_;
  }

 private:
  // Hosting AppMenu.
  base::WeakPtr<AppMenu> menu_;

  // The menu model containing the increment/decrement/reset items.
  ButtonMenuItemModel* menu_model_;
};

}  // namespace

// CutCopyPasteView ------------------------------------------------------------

// CutCopyPasteView is the view containing the cut/copy/paste buttons.
class AppMenu::CutCopyPasteView : public AppMenuView {
 public:
  CutCopyPasteView(AppMenu* menu,
                   ButtonMenuItemModel* menu_model,
                   int cut_index,
                   int copy_index,
                   int paste_index)
      : AppMenuView(menu, menu_model) {
    CreateAndConfigureButton(IDS_CUT, InMenuButtonBackground::LEADING_BORDER,
                             cut_index);
    CreateAndConfigureButton(IDS_COPY, InMenuButtonBackground::LEADING_BORDER,
                             copy_index);
    CreateAndConfigureButton(IDS_PASTE, InMenuButtonBackground::LEADING_BORDER,
                             paste_index);
  }
  CutCopyPasteView(const CutCopyPasteView&) = delete;
  CutCopyPasteView& operator=(const CutCopyPasteView&) = delete;

  // Overridden from View.
  gfx::Size CalculatePreferredSize() const override {
    // Returned height doesn't matter as MenuItemView forces everything to the
    // height of the menuitemview.
    return {GetMaxChildViewPreferredWidth() * int{children().size()}, 0};
  }

  void Layout() override {
    // All buttons are given the same width.
    int width = GetMaxChildViewPreferredWidth();
    int x = 0;
    for (auto* child : children()) {
      child->SetBounds(x, 0, width, height());
      x += width;
    }
  }

  // Overridden from ButtonListener.
  void ButtonPressed(views::Button* sender, const ui::Event& event) override {
    menu()->CancelAndEvaluate(menu_model(), sender->tag());
  }

 private:
  // Returns the max preferred width of all the children.
  int GetMaxChildViewPreferredWidth() const {
    int width = 0;
    for (const auto* child : children())
      width = std::max(width, child->GetPreferredSize().width());
    return width;
  }
};

// ZoomView --------------------------------------------------------------------


// ZoomView contains the various zoom controls: two buttons to increase/decrease
// the zoom, a label showing the current zoom percent, and a button to go
// full-screen.
class AppMenu::ZoomView : public AppMenuView {
 public:
  ZoomView(AppMenu* menu,
           ButtonMenuItemModel* menu_model,
           int decrement_index,
           int increment_index,
           int fullscreen_index)
      : AppMenuView(menu, menu_model),
        fullscreen_index_(fullscreen_index),
        increment_button_(nullptr),
        zoom_label_(nullptr),
        decrement_button_(nullptr),
        fullscreen_button_(nullptr),
        zoom_label_max_width_(0),
        zoom_label_max_width_valid_(false) {
    browser_zoom_subscription_ =
        zoom::ZoomEventManager::GetForBrowserContext(menu->browser_->profile())
            ->AddZoomLevelChangedCallback(
                base::Bind(&AppMenu::ZoomView::OnZoomLevelChanged,
                           base::Unretained(this)));

    decrement_button_ = CreateButtonWithAccName(
        IDS_ZOOM_MINUS2, InMenuButtonBackground::LEADING_BORDER,
        decrement_index, IDS_ACCNAME_ZOOM_MINUS2,
        /*add_accelerator_text*/ false);

    zoom_label_ = new Label(base::FormatPercent(100));
    zoom_label_->SetAutoColorReadabilityEnabled(false);
    zoom_label_->SetHorizontalAlignment(gfx::ALIGN_RIGHT);

    zoom_label_->SetBackground(std::make_unique<InMenuButtonBackground>(
        InMenuButtonBackground::NO_BORDER));

    // An accessibility role of kAlert will ensure that any updates to the zoom
    // level can be picked up by screen readers.
    zoom_label_->GetViewAccessibility().OverrideRole(ax::mojom::Role::kAlert);

    AddChildView(zoom_label_);
    zoom_label_max_width_valid_ = false;

    increment_button_ = CreateButtonWithAccName(
        IDS_ZOOM_PLUS2, InMenuButtonBackground::NO_BORDER, increment_index,
        IDS_ACCNAME_ZOOM_PLUS2, /*add_accelerator_text*/ false);

    fullscreen_button_ = new FullscreenButton(this);
    // all buttons on menu should must be a custom button in order for
    // the keyboard navigation to work.
    DCHECK(Button::AsButton(fullscreen_button_));

    // Since |fullscreen_button_| will reside in a menu, make it ALWAYS
    // focusable regardless of the platform.
    fullscreen_button_->SetFocusBehavior(FocusBehavior::ALWAYS);
    fullscreen_button_->set_tag(fullscreen_index);
    fullscreen_button_->SetImageHorizontalAlignment(ImageButton::ALIGN_CENTER);
    fullscreen_button_->SetImageVerticalAlignment(ImageButton::ALIGN_MIDDLE);
    fullscreen_button_->SetBackground(std::make_unique<InMenuButtonBackground>(
        InMenuButtonBackground::LEADING_BORDER));
    fullscreen_button_->SetAccessibleName(GetAccessibleNameForAppMenuItem(
        menu_model, fullscreen_index, IDS_ACCNAME_FULLSCREEN,
        /*add_accelerator_text*/ true));
    AddChildView(fullscreen_button_);

    // Need to set a font list for the zoom label width calculations.
    OnThemeChanged();
    UpdateZoomControls();
  }
  ZoomView(const ZoomView&) = delete;
  ZoomView& operator=(const ZoomView&) = delete;
  ~ZoomView() override = default;

  // Overridden from View.
  gfx::Size CalculatePreferredSize() const override {
    // The increment/decrement button are forced to the same width.
    int button_width = std::max(increment_button_->GetPreferredSize().width(),
                                decrement_button_->GetPreferredSize().width());
    int fullscreen_width =
        fullscreen_button_->GetPreferredSize().width() + kFullscreenPadding;
    // Returned height doesn't matter as MenuItemView forces everything to the
    // height of the menuitemview. Note that we have overridden the height when
    // constructing the menu.
    return gfx::Size(
        button_width + ZoomLabelMaxWidth() + button_width + fullscreen_width,
        0);
  }

  void Layout() override {
    int x = 0;
    int button_width = std::max(increment_button_->GetPreferredSize().width(),
                                decrement_button_->GetPreferredSize().width());
    gfx::Rect bounds(0, 0, button_width, height());

    decrement_button_->SetBoundsRect(bounds);

    x += bounds.width();
    bounds.set_x(x);
    bounds.set_width(ZoomLabelMaxWidth());
    zoom_label_->SetBoundsRect(bounds);

    x += bounds.width();
    bounds.set_x(x);
    bounds.set_width(button_width);
    increment_button_->SetBoundsRect(bounds);

    x += bounds.width();
    bounds.set_x(x);
    bounds.set_width(fullscreen_button_->GetPreferredSize().width() +
                     kFullscreenPadding);
    fullscreen_button_->SetBoundsRect(bounds);
  }

  void OnThemeChanged() override {
    AppMenuView::OnThemeChanged();

    zoom_label_->SetBorder(views::CreateEmptyBorder(
        0, kZoomLabelHorizontalPadding, 0, kZoomLabelHorizontalPadding));
    zoom_label_->SetFontList(MenuConfig::instance().font_list);
    zoom_label_max_width_valid_ = false;

    ui::NativeTheme* theme = GetNativeTheme();
    zoom_label_->SetEnabledColor(theme->GetSystemColor(
        ui::NativeTheme::kColorId_EnabledMenuItemForegroundColor));

    fullscreen_button_->SetImage(
        ImageButton::STATE_NORMAL,
        gfx::CreateVectorIcon(
            kFullscreenIcon,
            GetNativeTheme()->GetSystemColor(
                ui::NativeTheme::kColorId_EnabledMenuItemForegroundColor)));
    gfx::ImageSkia hovered_fullscreen_image = gfx::CreateVectorIcon(
        kFullscreenIcon,
        theme->GetSystemColor(
            ui::NativeTheme::kColorId_SelectedMenuItemForegroundColor));
    fullscreen_button_->SetImage(ImageButton::STATE_HOVERED,
                                 hovered_fullscreen_image);
    fullscreen_button_->SetImage(ImageButton::STATE_PRESSED,
                                 hovered_fullscreen_image);
  }

  // Overridden from ButtonListener.
  void ButtonPressed(views::Button* sender, const ui::Event& event) override {
    if (sender->tag() == fullscreen_index_) {
      menu()->CancelAndEvaluate(menu_model(), sender->tag());
    } else {
      // Zoom buttons don't close the menu.
      menu_model()->ActivatedAt(sender->tag());
    }
  }

 private:
  content::WebContents* GetActiveWebContents() const {
    return menu() ?
        menu()->browser_->tab_strip_model()->GetActiveWebContents() :
        nullptr;
  }

  void OnZoomLevelChanged(const content::HostZoomMap::ZoomLevelChange& change) {
    UpdateZoomControls();
  }

  void UpdateZoomControls() {
    WebContents* contents = GetActiveWebContents();
    int zoom = 100;
    if (contents) {
      auto* zoom_controller = zoom::ZoomController::FromWebContents(contents);
      if (zoom_controller)
        zoom = zoom_controller->GetZoomPercent();
      increment_button_->SetEnabled(zoom <
                                    contents->GetMaximumZoomPercent());
      decrement_button_->SetEnabled(zoom >
                                    contents->GetMinimumZoomPercent());
    }
    zoom_label_->SetText(base::FormatPercent(zoom));
    // An alert notification will ensure that the zoom label is always announced
    // even if is not focusable.
    zoom_label_->NotifyAccessibilityEvent(ax::mojom::Event::kAlert, true);
    zoom_label_max_width_valid_ = false;
  }

  // Returns the max width the zoom string can be.
  int ZoomLabelMaxWidth() const {
    if (!zoom_label_max_width_valid_) {
      const gfx::FontList& font_list = zoom_label_->font_list();
      int border_width = zoom_label_->border()
                             ? zoom_label_->border()->GetInsets().width()
                             : 0;

      int max_w = 0;

      WebContents* selected_tab = GetActiveWebContents();
      if (selected_tab) {
        auto* zoom_controller =
            zoom::ZoomController::FromWebContents(selected_tab);
        DCHECK(zoom_controller);
        // Enumerate all zoom factors that can be used in PageZoom::Zoom.
        std::vector<double> zoom_factors = zoom::PageZoom::PresetZoomFactors(
            zoom_controller->GetZoomPercent());
        for (auto zoom : zoom_factors) {
          int w = gfx::GetStringWidth(
              base::FormatPercent(static_cast<int>(std::round(zoom * 100))),
              font_list);
          max_w = std::max(w, max_w);
        }
      } else {
        max_w = gfx::GetStringWidth(base::FormatPercent(100), font_list);
      }
      zoom_label_max_width_ = max_w + border_width;

      zoom_label_max_width_valid_ = true;
    }
    return zoom_label_max_width_;
  }

  // Index of the fullscreen menu item in the model.
  const int fullscreen_index_;

  std::unique_ptr<content::HostZoomMap::Subscription>
      browser_zoom_subscription_;

  // Button for incrementing the zoom.
  LabelButton* increment_button_;

  // Label showing zoom as a percent.
  Label* zoom_label_;

  // Button for decrementing the zoom.
  LabelButton* decrement_button_;

  ImageButton* fullscreen_button_;

  // Cached width of how wide the zoom label string can be. This is the width at
  // 100%. This should not be accessed directly, use ZoomLabelMaxWidth()
  // instead. This value is cached because is depends on multiple calls to
  // gfx::GetStringWidth(...) which are expensive.
  mutable int zoom_label_max_width_;

  // Flag tracking whether calls to ZoomLabelMaxWidth() need to re-calculate
  // the label width, because the cached value may no longer be correct.
  mutable bool zoom_label_max_width_valid_;
};

// RecentTabsMenuModelDelegate  ------------------------------------------------

// Provides the ui::MenuModelDelegate implementation for RecentTabsSubMenuModel
// items.
class AppMenu::RecentTabsMenuModelDelegate : public ui::MenuModelDelegate {
 public:
  RecentTabsMenuModelDelegate(AppMenu* app_menu,
                              ui::MenuModel* model,
                              views::MenuItemView* menu_item)
      : app_menu_(app_menu), model_(model), menu_item_(menu_item) {
    model_->SetMenuModelDelegate(this);
  }
  RecentTabsMenuModelDelegate(const RecentTabsMenuModelDelegate&) = delete;
  RecentTabsMenuModelDelegate& operator=(const RecentTabsMenuModelDelegate&) =
      delete;
  ~RecentTabsMenuModelDelegate() override {
    model_->SetMenuModelDelegate(nullptr);
  }

  const gfx::FontList* GetLabelFontListAt(int index) const {
    return model_->GetLabelFontListAt(index);
  }

  // ui::MenuModelDelegate implementation:

  void OnIconChanged(int index) override {
    int command_id = model_->GetCommandIdAt(index);
    views::MenuItemView* item = menu_item_->GetMenuItemByID(command_id);
    DCHECK(item);
    gfx::Image icon;
    model_->GetIconAt(index, &icon);
    item->SetIcon(*icon.ToImageSkia());
  }

  void OnMenuStructureChanged() override {
    if (menu_item_->HasSubmenu()) {
      // Remove all menu items from submenu.
      menu_item_->RemoveAllMenuItems();

      // Remove all elements in |AppMenu::command_id_to_entry_| that map to
      // |model_|.
      auto iter = app_menu_->command_id_to_entry_.begin();
      while (iter != app_menu_->command_id_to_entry_.end()) {
        if (iter->second.first == model_)
          app_menu_->command_id_to_entry_.erase(iter++);
        else
          ++iter;
      }
    }

    // Add all menu items from |model| to submenu.
    for (int i = 0; i < model_->GetItemCount(); ++i) {
      app_menu_->AddMenuItem(menu_item_, i, model_, i, model_->GetTypeAt(i));
    }

    // In case recent tabs submenu was open when items were changing, force a
    // ChildrenChanged().
    menu_item_->ChildrenChanged();
  }

 private:
  AppMenu* app_menu_;
  ui::MenuModel* model_;
  views::MenuItemView* menu_item_;
};

// AppMenu ------------------------------------------------------------------

AppMenu::AppMenu(Browser* browser, int run_types, bool alert_reopen_tab_items)
    : browser_(browser),
      run_types_(run_types),
      alert_reopen_tab_items_(alert_reopen_tab_items) {
  global_error_observer_.Add(
      GlobalErrorServiceFactory::GetForProfile(browser->profile()));
}

AppMenu::~AppMenu() {
  if (bookmark_menu_delegate_.get()) {
    BookmarkModel* model =
        BookmarkModelFactory::GetForBrowserContext(browser_->profile());
    if (model)
      model->RemoveObserver(this);
  }
}

void AppMenu::Init(ui::MenuModel* model) {
  DCHECK(!root_);
  root_ = new MenuItemView(this);
  root_->set_has_icons(true);  // We have checks, radios and icons, set this
                               // so we get the taller menu style.
  PopulateMenu(root_, model);

  int32_t types = views::MenuRunner::HAS_MNEMONICS;
  if (for_drop()) {
    // We add NESTED_DRAG since currently the only operation to open the app
    // menu for is an extension action drag, which is controlled by the child
    // BrowserActionsContainer view.
    types |= views::MenuRunner::FOR_DROP | views::MenuRunner::NESTED_DRAG;
  }
  if (run_types_ & views::MenuRunner::SHOULD_SHOW_MNEMONICS)
    types |= views::MenuRunner::SHOULD_SHOW_MNEMONICS;

  menu_runner_ = std::make_unique<views::MenuRunner>(root_, types);
}

void AppMenu::RunMenu(views::MenuButtonController* host) {
  base::RecordAction(UserMetricsAction("ShowAppMenu"));

  menu_runner_->RunMenuAt(host->button()->GetWidget(), host,
                          host->button()->GetAnchorBoundsInScreen(),
                          views::MenuAnchorPosition::kTopRight,
                          ui::MENU_SOURCE_NONE);
}

void AppMenu::CloseMenu() {
  if (menu_runner_.get())
    menu_runner_->Cancel();
}

bool AppMenu::IsShowing() const {
  return menu_runner_.get() && menu_runner_->IsRunning();
}

void AppMenu::GetLabelStyle(int command_id, LabelStyle* style) const {
  if (IsRecentTabsCommand(command_id)) {
    const gfx::FontList* font_list =
        recent_tabs_menu_model_delegate_->GetLabelFontListAt(
            ModelIndexFromCommandId(command_id));
    // Only fill in |*color| if there's a font list - otherwise this method will
    // override the color for every recent tab item, not just the header.
    if (font_list) {
      // TODO(ellyjones): Use CONTEXT_MENU instead of CONTEXT_LABEL.
      style->foreground = views::style::GetColor(
          *root_, views::style::CONTEXT_LABEL, views::style::STYLE_PRIMARY);
      style->font_list = *font_list;
    }
  }
}

base::string16 AppMenu::GetTooltipText(int command_id,
                                       const gfx::Point& p) const {
  return IsBookmarkCommand(command_id) ?
      bookmark_menu_delegate_->GetTooltipText(command_id, p) : base::string16();
}

bool AppMenu::IsTriggerableEvent(views::MenuItemView* menu,
                                 const ui::Event& e) {
  return IsBookmarkCommand(menu->GetCommand()) ?
      bookmark_menu_delegate_->IsTriggerableEvent(menu, e) :
      MenuDelegate::IsTriggerableEvent(menu, e);
}

bool AppMenu::GetDropFormats(MenuItemView* menu,
                             int* formats,
                             std::set<ui::ClipboardFormatType>* format_types) {
  CreateBookmarkMenu();
  return bookmark_menu_delegate_.get() &&
      bookmark_menu_delegate_->GetDropFormats(menu, formats, format_types);
}

bool AppMenu::AreDropTypesRequired(MenuItemView* menu) {
  CreateBookmarkMenu();
  return bookmark_menu_delegate_.get() &&
      bookmark_menu_delegate_->AreDropTypesRequired(menu);
}

bool AppMenu::CanDrop(MenuItemView* menu, const ui::OSExchangeData& data) {
  CreateBookmarkMenu();
  return bookmark_menu_delegate_.get() &&
      bookmark_menu_delegate_->CanDrop(menu, data);
}

int AppMenu::GetDropOperation(MenuItemView* item,
                              const ui::DropTargetEvent& event,
                              DropPosition* position) {
  return IsBookmarkCommand(item->GetCommand()) ?
      bookmark_menu_delegate_->GetDropOperation(item, event, position) :
      ui::DragDropTypes::DRAG_NONE;
}

int AppMenu::OnPerformDrop(MenuItemView* menu,
                           DropPosition position,
                           const ui::DropTargetEvent& event) {
  if (!IsBookmarkCommand(menu->GetCommand()))
    return ui::DragDropTypes::DRAG_NONE;

  int result = bookmark_menu_delegate_->OnPerformDrop(menu, position, event);
  return result;
}

bool AppMenu::ShowContextMenu(MenuItemView* source,
                              int command_id,
                              const gfx::Point& p,
                              ui::MenuSourceType source_type) {
  return IsBookmarkCommand(command_id) ?
      bookmark_menu_delegate_->ShowContextMenu(source, command_id, p,
                                               source_type) :
      false;
}

bool AppMenu::CanDrag(MenuItemView* menu) {
  return IsBookmarkCommand(menu->GetCommand()) ?
      bookmark_menu_delegate_->CanDrag(menu) : false;
}

void AppMenu::WriteDragData(MenuItemView* sender, ui::OSExchangeData* data) {
  DCHECK(IsBookmarkCommand(sender->GetCommand()));
  return bookmark_menu_delegate_->WriteDragData(sender, data);
}

int AppMenu::GetDragOperations(MenuItemView* sender) {
  return IsBookmarkCommand(sender->GetCommand()) ?
      bookmark_menu_delegate_->GetDragOperations(sender) :
      MenuDelegate::GetDragOperations(sender);
}

int AppMenu::GetMaxWidthForMenu(MenuItemView* menu) {
  if (menu->GetCommand() == IDC_BOOKMARKS_MENU ||
      IsBookmarkCommand(menu->GetCommand())) {
    return bookmark_menu_delegate_->GetMaxWidthForMenu(menu);
  }
  return MenuDelegate::GetMaxWidthForMenu(menu);
}

bool AppMenu::IsItemChecked(int command_id) const {
  if (IsBookmarkCommand(command_id))
    return false;

  const Entry& entry = command_id_to_entry_.find(command_id)->second;
  return entry.first->IsItemCheckedAt(entry.second);
}

bool AppMenu::IsCommandEnabled(int command_id) const {
  if (IsBookmarkCommand(command_id))
    return true;

  if (command_id == 0)
    return false;  // The root item.

  if (command_id == IDC_MORE_TOOLS_MENU)
    return true;

  // The items representing the cut menu (cut/copy/paste), zoom menu
  // (increment/decrement/reset) and extension toolbar view are always enabled.
  // The child views of these items enabled state updates appropriately.
  if (command_id == IDC_EDIT_MENU || command_id == IDC_ZOOM_MENU ||
      command_id == IDC_EXTENSIONS_OVERFLOW_MENU)
    return true;

  const Entry& entry = command_id_to_entry_.find(command_id)->second;
  return entry.first->IsEnabledAt(entry.second);
}

void AppMenu::ExecuteCommand(int command_id, int mouse_event_flags) {
  if (IsBookmarkCommand(command_id)) {
    UMA_HISTOGRAM_MEDIUM_TIMES("WrenchMenu.TimeToAction.OpenBookmark",
                        menu_opened_timer_.Elapsed());
    UMA_HISTOGRAM_ENUMERATION("WrenchMenu.MenuAction",
                              MENU_ACTION_BOOKMARK_OPEN, LIMIT_MENU_ACTION);
    bookmark_menu_delegate_->ExecuteCommand(command_id, mouse_event_flags);
    return;
  }

  if (command_id == IDC_EDIT_MENU || command_id == IDC_ZOOM_MENU ||
      command_id == IDC_EXTENSIONS_OVERFLOW_MENU) {
    // These items are represented by child views. If ExecuteCommand is invoked
    // it means the user clicked on the area around the buttons and we should
    // not do anyting.
    return;
  }

  const Entry& entry = command_id_to_entry_.find(command_id)->second;
  return entry.first->ActivatedAt(entry.second, mouse_event_flags);
}

bool AppMenu::GetAccelerator(int command_id,
                             ui::Accelerator* accelerator) const {
  if (IsBookmarkCommand(command_id))
    return false;

  if (command_id == IDC_EDIT_MENU || command_id == IDC_ZOOM_MENU ||
      command_id == IDC_EXTENSIONS_OVERFLOW_MENU) {
    // These have special child views; don't show the accelerator for them.
    return false;
  }

  auto ix = command_id_to_entry_.find(command_id);
  const Entry& entry = ix->second;
  ui::Accelerator menu_accelerator;
  if (!entry.first->GetAcceleratorAt(entry.second, &menu_accelerator))
    return false;

  *accelerator = ui::Accelerator(menu_accelerator.key_code(),
                                 menu_accelerator.modifiers());
  return true;
}

void AppMenu::WillShowMenu(MenuItemView* menu) {
  if (menu == bookmark_menu_)
    CreateBookmarkMenu();
  else if (bookmark_menu_delegate_)
    bookmark_menu_delegate_->WillShowMenu(menu);
}

void AppMenu::WillHideMenu(MenuItemView* menu) {
  // Turns off the fade out animation of the app menus if |feedback_menu_item_|
  // or |screenshot_menu_item_| is selected. This excludes the app menu itself
  // from the screenshot.
  if (menu->HasSubmenu() &&
      ((feedback_menu_item_ && feedback_menu_item_->IsSelected()) ||
       (screenshot_menu_item_ && screenshot_menu_item_->IsSelected()))) {
    // It's okay to just turn off the animation and not turn it back on because
    // the menu widget will be recreated next time it's opened. See
    // ToolbarView::RunMenu() and Init() of this class.
    menu->GetSubmenu()->GetWidget()->
        SetVisibilityChangedAnimationsEnabled(false);
  }
}

bool AppMenu::ShouldCloseOnDragComplete() {
  return false;
}

void AppMenu::OnMenuClosed(views::MenuItemView* menu) {
  auto* browser_view = BrowserView::GetBrowserViewForBrowser(browser_);
  browser_view->toolbar_button_provider()->GetAppMenuButton()->OnMenuClosed();

  if (bookmark_menu_delegate_.get()) {
    BookmarkModel* model =
        BookmarkModelFactory::GetForBrowserContext(browser_->profile());
    if (model)
      model->RemoveObserver(this);
  }

  if (selected_menu_model_)
    selected_menu_model_->ActivatedAt(selected_index_);
}

bool AppMenu::ShouldExecuteCommandWithoutClosingMenu(int command_id,
                                                     const ui::Event& event) {
  if (IsRecentTabsCommand(command_id) && event.IsMouseEvent()) {
    const auto disposition = ui::DispositionFromEventFlags(event.flags());
    if (disposition == WindowOpenDisposition::NEW_BACKGROUND_TAB)
      return true;
  }
  return false;
}

void AppMenu::BookmarkModelChanged() {
  DCHECK(bookmark_menu_delegate_.get());
  if (!bookmark_menu_delegate_->is_mutating_model())
    root_->Cancel();
}

void AppMenu::OnGlobalErrorsChanged() {
  // A change in the global errors list can add or remove items from the
  // menu. Close the menu to avoid have a stale menu on-screen.
  if (root_)
    root_->Cancel();
}

void AppMenu::PopulateMenu(MenuItemView* parent, MenuModel* model) {
  for (int i = 0, max = model->GetItemCount(); i < max; ++i) {
    // Add the menu item at the end.
    int menu_index =
        parent->HasSubmenu() ? int{parent->GetSubmenu()->children().size()} : 0;
    MenuItemView* item =
        AddMenuItem(parent, menu_index, model, i, model->GetTypeAt(i));

#if defined(OS_CHROMEOS)
    if (model->GetCommandIdAt(i) == IDC_EDIT_MENU ||
        model->GetCommandIdAt(i) == IDC_ZOOM_MENU) {
      // ChromeOS adds extra vertical space for the menu buttons.
      const MenuConfig& config = views::MenuConfig::instance();
      int top_margin = config.item_top_margin + config.separator_height / 2 + 4;
      int bottom_margin =
          config.item_bottom_margin + config.separator_height / 2 + 5;

      item->SetMargins(top_margin, bottom_margin);
    }
#endif

    if (model->GetTypeAt(i) == MenuModel::TYPE_SUBMENU)
      PopulateMenu(item, model->GetSubmenuModelAt(i));

    switch (model->GetCommandIdAt(i)) {
      case IDC_EXTENSIONS_OVERFLOW_MENU: {
        extension_toolbar_ = item->AddChildView(
            std::make_unique<ExtensionToolbarMenuView>(browser_, item));
        break;
      }

      case IDC_EDIT_MENU: {
        ui::ButtonMenuItemModel* submodel = model->GetButtonMenuItemAt(i);
        DCHECK_EQ(IDC_CUT, submodel->GetCommandIdAt(0));
        DCHECK_EQ(IDC_COPY, submodel->GetCommandIdAt(1));
        DCHECK_EQ(IDC_PASTE, submodel->GetCommandIdAt(2));
        item->SetTitle(l10n_util::GetStringUTF16(IDS_EDIT2));
        item->AddChildView(
            std::make_unique<CutCopyPasteView>(this, submodel, 0, 1, 2));
        break;
      }

      case IDC_ZOOM_MENU: {
        ui::ButtonMenuItemModel* submodel = model->GetButtonMenuItemAt(i);
        DCHECK_EQ(IDC_ZOOM_MINUS, submodel->GetCommandIdAt(0));
        DCHECK_EQ(IDC_ZOOM_PLUS, submodel->GetCommandIdAt(1));
        DCHECK_EQ(IDC_FULLSCREEN, submodel->GetCommandIdAt(2));
        item->SetTitle(l10n_util::GetStringUTF16(IDS_ZOOM_MENU2));
        item->AddChildView(std::make_unique<ZoomView>(this, submodel, 0, 1, 2));
        break;
      }

      case IDC_BOOKMARKS_MENU:
        DCHECK(!bookmark_menu_);
        bookmark_menu_ = item;
        break;

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
      case IDC_FEEDBACK:
        DCHECK(!feedback_menu_item_);
        feedback_menu_item_ = item;
        break;
#endif

#if defined(OS_CHROMEOS)
      case IDC_TAKE_SCREENSHOT:
        DCHECK(!screenshot_menu_item_);
        screenshot_menu_item_ = item;
        break;
#endif

      case IDC_RECENT_TABS_MENU:
        DCHECK(!recent_tabs_menu_model_delegate_.get());
        recent_tabs_menu_model_delegate_.reset(
            new RecentTabsMenuModelDelegate(this, model->GetSubmenuModelAt(i),
                                            item));
        break;

      default:
        break;
    }
  }
}

MenuItemView* AppMenu::AddMenuItem(MenuItemView* parent,
                                   int menu_index,
                                   MenuModel* model,
                                   int model_index,
                                   MenuModel::ItemType menu_type) {
  int command_id = model->GetCommandIdAt(model_index);
  DCHECK(command_id > -1 ||
         (command_id == -1 &&
          model->GetTypeAt(model_index) == MenuModel::TYPE_SEPARATOR));
  DCHECK_LT(command_id, IDC_FIRST_BOOKMARK_MENU);

  if (command_id > -1) {  // Don't add separators to |command_id_to_entry_|.
    // All command ID's should be unique except for IDC_SHOW_HISTORY which is
    // in both app menu and RecentTabs submenu,
    if (command_id != IDC_SHOW_HISTORY) {
      DCHECK(command_id_to_entry_.find(command_id) ==
             command_id_to_entry_.end())
          << "command ID " << command_id << " already exists!";
    }
    command_id_to_entry_[command_id].first = model;
    command_id_to_entry_[command_id].second = model_index;
  }

  MenuItemView* menu_item = views::MenuModelAdapter::AddMenuItemFromModelAt(
      model, model_index, parent, menu_index, command_id);

  if (menu_item) {
    // Flush all buttons to the right side of the menu for the new menu type.
    menu_item->set_use_right_margin(false);
    menu_item->SetVisible(model->IsVisibleAt(model_index));

    if (menu_type == MenuModel::TYPE_COMMAND && model->HasIcons()) {
      gfx::Image icon;
      if (model->GetIconAt(model_index, &icon))
        menu_item->SetIcon(*icon.ToImageSkia());
    }

    // If we want to show items relating to reopening the last-closed tab as
    // alerted, we specifically need to show alerts on the recent tabs submenu
    // and the last closed tab menu item.
    if (alert_reopen_tab_items_ &&
        ((command_id == IDC_RECENT_TABS_MENU) ||
         (command_id == AppMenuModel::kMinRecentTabsCommandId)))
      menu_item->SetAlerted();
  }

  return menu_item;
}

void AppMenu::CancelAndEvaluate(ButtonMenuItemModel* model, int index) {
  selected_menu_model_ = model;
  selected_index_ = index;
  root_->Cancel();
}

void AppMenu::CreateBookmarkMenu() {
  if (bookmark_menu_delegate_.get())
    return;  // Already created the menu.

  BookmarkModel* model =
      BookmarkModelFactory::GetForBrowserContext(browser_->profile());
  if (!model->loaded())
    return;

  model->AddObserver(this);

  // TODO(oshima): Replace with views only API.
  views::Widget* parent = views::Widget::GetWidgetForNativeWindow(
      browser_->window()->GetNativeWindow());
  bookmark_menu_delegate_.reset(
      new BookmarkMenuDelegate(browser_, browser_, parent));
  bookmark_menu_delegate_->Init(this,
                                bookmark_menu_,
                                model->bookmark_bar_node(),
                                0,
                                BookmarkMenuDelegate::SHOW_PERMANENT_FOLDERS,
                                BOOKMARK_LAUNCH_LOCATION_APP_MENU);
}

int AppMenu::ModelIndexFromCommandId(int command_id) const {
  auto ix = command_id_to_entry_.find(command_id);
  DCHECK(ix != command_id_to_entry_.end());
  return ix->second.second;
}
