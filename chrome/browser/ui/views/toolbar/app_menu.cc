// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/app_menu.h"

#include <stdint.h>

#include <algorithm>
#include <cmath>
#include <memory>
#include <optional>
#include <set>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/i18n/number_formatting.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/not_fatal_until.h"
#include "base/strings/utf_string_conversions.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/sharing_hub/sharing_hub_features.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/bookmarks/bookmark_stats.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/global_error/global_error_service_factory.h"
#include "chrome/browser/ui/hats/trust_safety_sentiment_service_factory.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/profiles/profile_view_utils.h"
#include "chrome/browser/ui/safety_hub/safety_hub_hats_service.h"
#include "chrome/browser/ui/safety_hub/safety_hub_hats_service_factory.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_service_factory.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/toolbar/app_menu_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_menu_delegate.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/frame/app_menu_button.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/upgrade_detector/upgrade_detector.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/saved_tab_groups/public/features.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/zoom/page_zoom.h"
#include "components/zoom/zoom_controller.h"
#include "components/zoom/zoom_event_manager.h"
#include "content/public/browser/host_zoom_map.h"
#include "content/public/browser/web_contents.h"
#include "extensions/common/feature_switch.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/accelerators/menu_label_accelerator_util.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/themed_vector_icon.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/window_open_disposition_utils.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/compositor/layer_tree_owner.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/gfx/text_elider.h"
#include "ui/gfx/text_utils.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
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
#include "ui/views/layout/layout_provider.h"
#include "ui/views/style/typography.h"
#include "ui/views/style/typography_provider.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_utils.h"
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

constexpr int kBookmarksCommandIdOffset =
    AppMenuModel::kMinBookmarksCommandId - IDC_FIRST_UNBOUNDED_MENU;
constexpr int kRecentTabsCommandIdOffset =
    AppMenuModel::kMinRecentTabsCommandId - IDC_FIRST_UNBOUNDED_MENU;
constexpr int kTabGroupsCommandIdOffset =
    AppMenuModel::kMinTabGroupsCommandId - IDC_FIRST_UNBOUNDED_MENU;

#if BUILDFLAG(IS_CHROMEOS)
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
  return command_id >= IDC_FIRST_UNBOUNDED_MENU &&
         ((command_id - IDC_FIRST_UNBOUNDED_MENU) %
              AppMenuModel::kNumUnboundedMenuTypes ==
          kBookmarksCommandIdOffset);
}

// Returns true if |command_id| identifies a recent tabs menu item.
bool IsRecentTabsCommand(int command_id) {
  return command_id >= IDC_FIRST_UNBOUNDED_MENU &&
         ((command_id - IDC_FIRST_UNBOUNDED_MENU) %
              AppMenuModel::kNumUnboundedMenuTypes ==
          kRecentTabsCommandIdOffset);
}

// Returns true if |command_id| identifies a tab group menu item.
bool IsTabGroupsCommand(int command_id) {
  return command_id >= IDC_FIRST_UNBOUNDED_MENU &&
         ((command_id - IDC_FIRST_UNBOUNDED_MENU) %
              AppMenuModel::kNumUnboundedMenuTypes ==
          kTabGroupsCommandIdOffset);
}

// Combination border/background for the buttons contained in the menu. The
// painting of the border/background is done here as LabelButton does not always
// paint the border.
class InMenuButtonBackground : public views::Background {
 public:
  enum class ButtonType {
    // A rectangular button with no drawn border.
    kNoBorder,

    // A rectangular button with a border drawn along the leading (left) side.
    kLeadingBorder,

    // A button with no drawn border and a rounded background.
    kRoundedButton,
  };

  enum class ButtonShape {
    // A rectagular or rounded rectangular button.
    kRectangular,
    // A circular button centered in the bounds.
    kCircular,
  };

  explicit InMenuButtonBackground(ButtonType type,
                                  ButtonShape shape = ButtonShape::kRectangular)
      : type_(type), shape_(shape) {}
  InMenuButtonBackground(const InMenuButtonBackground&) = delete;
  InMenuButtonBackground& operator=(const InMenuButtonBackground&) = delete;
  ~InMenuButtonBackground() override = default;

  // Overridden from views::Background.
  void Paint(gfx::Canvas* canvas, View* view) const override {
    // Draw leading border if desired.
    gfx::Rect bounds = view->GetLocalBounds();
    if (type_ == ButtonType::kLeadingBorder) {
      // We need to flip the canvas for RTL iff the button is not auto-flipping
      // already, so we end up flipping exactly once.
      gfx::ScopedCanvas scoped_canvas(canvas);
      if (!view->GetFlipCanvasOnPaintForRTLUI())
        scoped_canvas.FlipIfRTL(view->width());
      ui::NativeTheme::MenuSeparatorExtraParams menu_separator;
      const gfx::Rect separator_bounds(gfx::Size(
          MenuConfig::instance().separator_thickness, view->height()));
      menu_separator.paint_rect = &separator_bounds;
      menu_separator.type = ui::VERTICAL_SEPARATOR;
      view->GetNativeTheme()->Paint(
          canvas->sk_canvas(), view->GetColorProvider(),
          ui::NativeTheme::kMenuPopupSeparator, ui::NativeTheme::kNormal,
          separator_bounds, ui::NativeTheme::ExtraParams(menu_separator));
      bounds.Inset(gfx::Insets::TLBR(
          0, MenuConfig::instance().separator_thickness, 0, 0));
    }

    // Fill in background for state.
    DrawBackground(canvas, view, view->GetMirroredRect(bounds),
                   views::AsViewClass<views::Button>(view)->GetState());
  }

 private:
  void DrawBackground(gfx::Canvas* canvas,
                      const views::View* view,
                      const gfx::Rect& bounds,
                      views::Button::ButtonState state) const {
    if (state == views::Button::STATE_DISABLED) {
      return;
    }

    gfx::Rect bounds_rect = bounds;
    ui::NativeTheme::MenuItemExtraParams menu_item;
    if (type_ == ButtonType::kRoundedButton) {
      // Consistent with a hover corner radius (kInkDropSmallCornerRadius).
      const int kBackgroundCornerRadius = 2;
      menu_item.corner_radius = kBackgroundCornerRadius;
    } else if (shape_ == ButtonShape::kCircular) {
      constexpr int kCircularButtonSize = 28;
      bounds_rect.ClampToCenteredSize(
          gfx::Size(kCircularButtonSize, kCircularButtonSize));
      menu_item.corner_radius = kCircularButtonSize / 2;
    }
    const auto* const color_provider = view->GetColorProvider();
      cc::PaintFlags flags;
      flags.setColor(color_provider->GetColor(
          state == views::Button::STATE_NORMAL
              ? ui::kColorMenuButtonBackground
              : ui::kColorMenuButtonBackgroundSelected));
      canvas->DrawRoundRect(gfx::RectF(bounds_rect), menu_item.corner_radius,
                            flags);
      return;
  }

  const ButtonType type_;
  const ButtonShape shape_;
};

std::u16string GetAccessibleNameForAppMenuItem(ButtonMenuItemModel* model,
                                               size_t item_index,
                                               int accessible_string_id,
                                               bool add_accelerator_text) {
  std::u16string accessible_name =
      l10n_util::GetStringUTF16(accessible_string_id);
  std::u16string accelerator_text;

  ui::Accelerator menu_accelerator;
  if (add_accelerator_text &&
      model->GetAcceleratorAt(item_index, &menu_accelerator)) {
    accelerator_text = ui::Accelerator(menu_accelerator.key_code(),
                                       menu_accelerator.modifiers())
                           .GetShortcutText();
  }

  return MenuItemView::GetAccessibleNameForMenuItem(accessible_name,
                                                    accelerator_text, false);
}

// A button that lives inside a menu item.
class InMenuButton : public LabelButton {
  METADATA_HEADER(InMenuButton, LabelButton)

 public:
  using LabelButton::LabelButton;
  InMenuButton(const InMenuButton&) = delete;
  InMenuButton& operator=(const InMenuButton&) = delete;
  ~InMenuButton() override = default;

  void Init(InMenuButtonBackground::ButtonType type) {
    // An InMenuButton should always be focusable regardless of the platform.
    SetFocusBehavior(FocusBehavior::ALWAYS);
    SetHorizontalAlignment(gfx::ALIGN_CENTER);

    SetBackground(std::make_unique<InMenuButtonBackground>(type));
    SetBorder(views::CreateEmptyBorder(
        gfx::Insets::TLBR(0, kHorizontalPadding, 0, kHorizontalPadding)));
    label()->SetFontList(MenuConfig::instance().font_list);

    SetTextColorId(views::Button::STATE_DISABLED,
                   ui::kColorMenuItemForegroundDisabled);
    SetTextColorId(views::Button::STATE_HOVERED,
                   ui::kColorMenuItemForegroundSelected);
    SetTextColorId(views::Button::STATE_PRESSED,
                   ui::kColorMenuItemForegroundSelected);
    SetTextColorId(views::Button::STATE_NORMAL, ui::kColorMenuItemForeground);

    GetViewAccessibility().SetRole(ax::mojom::Role::kMenuItem);
  }
};

BEGIN_METADATA(InMenuButton)
END_METADATA

// A button with an image inside a menu item.
class InMenuImageButton : public ImageButton {
  METADATA_HEADER(InMenuImageButton, ImageButton)

 public:
  using ImageButton::ImageButton;

  void Init(InMenuButtonBackground::ButtonType type,
            InMenuButtonBackground::ButtonShape shape,
            const ui::ImageModel& image_model) {
    SetFocusBehavior(FocusBehavior::ALWAYS);
    SetImageModel(views::Button::STATE_NORMAL, image_model);
    SetImageHorizontalAlignment(ImageButton::ALIGN_CENTER);
    SetImageVerticalAlignment(ImageButton::ALIGN_MIDDLE);
    SetBackground(std::make_unique<InMenuButtonBackground>(type, shape));
    SetBorder(views::CreateEmptyBorder(
        gfx::Insets::TLBR(0, kHorizontalPadding, 0, kHorizontalPadding)));

    GetViewAccessibility().SetRole(ax::mojom::Role::kMenuItem);
  }
};

BEGIN_METADATA(InMenuImageButton)
END_METADATA

// Conditionally return the update app menu item substring text based on upgrade
// detector state.
std::u16string GetUpgradeDialogSubstringText() {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING) && \
    (BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX))
  if (!UpgradeDetector::GetInstance()->is_outdated_install() &&
      !UpgradeDetector::GetInstance()->is_outdated_install_no_au()) {
    {
      return {l10n_util::GetStringUTF16(IDS_RELAUNCH_TO_UPDATE_ALT_MINOR_TEXT)};
    }
  }
#endif
  return std::u16string();
}

std::u16string GetSigninStatusChipString(Profile* profile) {
  const AccountInfo account_info = GetAccountInfoFromProfile(profile);

  if (IsSyncPaused(profile) || account_info.IsEmpty()) {
    return l10n_util::GetStringUTF16(IDS_PROFILES_LOCAL_PROFILE_STATE);
  }

  if (signin_util::IsSigninPending(
          IdentityManagerFactory::GetForProfile(profile))) {
    // TODO(b/347011002): Use a non sync related string.
    return l10n_util::GetStringUTF16(IDS_AVATAR_BUTTON_SYNC_PAUSED);
  }

  return l10n_util::GetStringUTF16(IDS_PROFILE_ROW_SIGNED_IN_MESSAGE);
}

// Helper method that adds a bespoke chip to the profile related menu items.
void AddSignedInChipToProfileMenuItem(
    Profile* profile,
    views::MenuItemView* item,
    const int horizontal_padding,
    std::vector<base::CallbackListSubscription>&
        profile_menu_subscription_list) {
  if (!profile->GetPrefs()->GetBoolean(prefs::kSigninAllowed) ||
      profile->IsIncognitoProfile()) {
    return;
  }
  constexpr int profile_chip_corner_radii = 100;
  raw_ptr<views::Label> profile_chip_label;
  const MenuConfig& config = MenuConfig::instance();

  // We need to have the label of the profile chip within a
  // BoxLayoutView and inside border insets because MenuItemView will
  // layout the child items to the full height of the menu item in
  // MenuItemView::Layout().
  auto profile_chip =
      views::Builder<views::BoxLayoutView>()
          .SetInsideBorderInsets(
              gfx::Insets::VH(config.item_vertical_margin, 0))
          .AddChildren(
              views::Builder<views::Label>()
                  .SetText(GetSigninStatusChipString(profile))
                  .CopyAddressTo(&profile_chip_label)
                  .SetBackground(views::CreateThemedRoundedRectBackground(
                      item->IsSelected()
                          ? ui::kColorAppMenuProfileRowChipHovered
                          : ui::kColorAppMenuProfileRowChipBackground,
                      profile_chip_corner_radii))
                  // Add additional horizontal padding. Vertical
                  // padding depends on menu margins to get alignment
                  // with other items in the menu.
                  .SetBorder(views::CreateEmptyBorder(gfx::Insets::VH(
                      0, views::LayoutProvider::Get()
                             ->GetInsetsMetric(views::INSETS_LABEL_BUTTON)
                             .left()))))
          .Build();

  // MenuItemView has specific layout logic for child views which does not work
  // very well with more custom menu items. We use this view to add the correct
  // spacing between the profile chip and the edge of the menu.
  auto profile_chip_edge_spacing_view =
      views::Builder<views::View>()
          .SetPreferredSize(gfx::Size(horizontal_padding, 0))
          .Build();
  profile_menu_subscription_list.push_back(
      item->AddSelectedChangedCallback(base::BindRepeating(
          [](MenuItemView* menu_item_view, View* child_view,
             int corner_radius) {
            child_view->SetBackground(views::CreateThemedRoundedRectBackground(
                menu_item_view->IsSelected()
                    ? ui::kColorAppMenuProfileRowChipHovered
                    : ui::kColorAppMenuProfileRowChipBackground,
                corner_radius));
          },
          item, profile_chip_label, profile_chip_corner_radii)));
  item->AddChildView(std::move(profile_chip));
  item->AddChildView(std::move(profile_chip_edge_spacing_view));
  item->SetHighlightWhenSelectedWithChildViews(true);
}

// AppMenuView is a view that can contain label buttons.
class AppMenuView : public views::View {
  METADATA_HEADER(AppMenuView, views::View)

 public:
  AppMenuView(AppMenu* menu, ButtonMenuItemModel* menu_model)
      : menu_(menu->AsWeakPtr()), menu_model_(menu_model) {
    GetViewAccessibility().SetRole(ax::mojom::Role::kMenu);
  }
  AppMenuView(const AppMenuView&) = delete;
  AppMenuView& operator=(const AppMenuView&) = delete;
  ~AppMenuView() override = default;

  views::Button* CreateAndConfigureButton(
      views::Button::PressedCallback callback,
      int string_id,
      InMenuButtonBackground::ButtonType type,
      size_t index) {
    return CreateButtonWithAccessibleName(
        std::move(callback), string_id, type, index, string_id,
        /*add_accelerator_text=*/true,
        /*use_accessible_name_as_tooltip_text=*/false);
  }

  views::Button* CreateButtonWithAccessibleName(
      views::Button::PressedCallback callback,
      int string_id,
      InMenuButtonBackground::ButtonType type,
      size_t index,
      int accessible_name_id,
      bool add_accelerator_text,
      bool use_accessible_name_as_tooltip_text,
      const ui::ImageModel image_model = ui::ImageModel()) {
    // Should only be invoked during construction when |menu_| is valid.
    DCHECK(menu_);

    std::unique_ptr<views::Button> menu_button;
      auto button = std::make_unique<InMenuImageButton>(std::move(callback));
      button->Init(type, InMenuButtonBackground::ButtonShape::kCircular,
                   image_model);
      menu_button = std::move(button);
    menu_button->GetViewAccessibility().SetName(GetAccessibleNameForAppMenuItem(
        menu_model_, index, accessible_name_id, add_accelerator_text));
    menu_button->set_tag(index);
    menu_button->SetEnabled(menu_model_->IsEnabledAt(index));
    if (use_accessible_name_as_tooltip_text) {
      menu_button->SetTooltipText(
          l10n_util::GetStringUTF16(accessible_name_id));
    }

    // all buttons on menu should must be a custom button in order for
    // the keyboard nativigation work.
    DCHECK(views::IsViewClass<views::Button>(menu_button.get()));

    return AddChildView(std::move(menu_button));
  }

 protected:
  base::WeakPtr<AppMenu> menu() { return menu_; }
  base::WeakPtr<const AppMenu> menu() const { return menu_; }

 private:
  // Hosting AppMenu.
  base::WeakPtr<AppMenu> menu_;

  // The menu model containing the increment/decrement/reset items.
  raw_ptr<ButtonMenuItemModel> menu_model_;
};

BEGIN_METADATA(AppMenuView)
END_METADATA

// Subclass of ImageButton whose preferred size includes the size of the border.
class FullscreenButton : public ImageButton {
  METADATA_HEADER(FullscreenButton, ImageButton)

 public:
  explicit FullscreenButton(PressedCallback callback,
                            ButtonMenuItemModel* menu_model,
                            size_t fullscreen_index,
                            bool is_in_fullscreen)
      : ImageButton(std::move(callback)) {
    // Since |fullscreen_button_| will reside in a menu, make it ALWAYS
    // focusable regardless of the platform.
    SetFocusBehavior(FocusBehavior::ALWAYS);
    set_tag(fullscreen_index);
    SetImageHorizontalAlignment(ImageButton::ALIGN_CENTER);
    SetImageVerticalAlignment(ImageButton::ALIGN_MIDDLE);
    SetBackground(std::make_unique<InMenuButtonBackground>(
        InMenuButtonBackground::ButtonType::kLeadingBorder,
        InMenuButtonBackground::ButtonShape::kCircular));
    const int accname_string_id =
        is_in_fullscreen ? IDS_ACCNAME_EXIT_FULLSCREEN : IDS_ACCNAME_FULLSCREEN;
    SetTooltipText(l10n_util::GetStringUTF16(accname_string_id));
    GetViewAccessibility().SetRole(ax::mojom::Role::kMenuItem);
    GetViewAccessibility().SetName(GetAccessibleNameForAppMenuItem(
        menu_model, fullscreen_index, accname_string_id,
#if BUILDFLAG(IS_CHROMEOS_ASH)
        // ChromeOS uses a dedicated "fullscreen" media key for fullscreen
        // mode on most ChromeOS devices which cannot be specified in the
        // standard way here, so omit the accelerator to avoid providing
        // misleading or confusing information to screen reader users.
        // See crbug.com/1110468 for more context.
        /*add_accelerator_text=*/false
#else
        /*add_accelerator_text=*/true
#endif
        ));
  }
  FullscreenButton(const FullscreenButton&) = delete;
  FullscreenButton& operator=(const FullscreenButton&) = delete;

  // Overridden from ImageButton.
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override {
    gfx::Size pref = ImageButton::CalculatePreferredSize(available_size);
    const gfx::Insets insets = GetInsets();
    pref.Enlarge(insets.width(), insets.height());
    return pref;
  }
};

BEGIN_METADATA(FullscreenButton)
END_METADATA

}  // namespace

// CutCopyPasteView ------------------------------------------------------------

// CutCopyPasteView is the view containing the cut/copy/paste buttons.
class AppMenu::CutCopyPasteView : public AppMenuView {
  METADATA_HEADER(CutCopyPasteView, AppMenuView)

 public:
  CutCopyPasteView(AppMenu* menu,
                   ButtonMenuItemModel* menu_model,
                   size_t cut_index,
                   size_t copy_index,
                   size_t paste_index)
      : AppMenuView(menu, menu_model) {
    const auto cancel_and_evaluate =
        [](AppMenu* menu, ButtonMenuItemModel* menu_model, size_t index) {
          menu->CancelAndEvaluate(menu_model, index);
        };
    CreateAndConfigureButton(
        base::BindRepeating(cancel_and_evaluate, base::Unretained(menu),
                            menu_model, cut_index),
        IDS_CUT, InMenuButtonBackground::ButtonType::kLeadingBorder, cut_index);
    CreateAndConfigureButton(
        base::BindRepeating(cancel_and_evaluate, base::Unretained(menu),
                            menu_model, copy_index),
        IDS_COPY, InMenuButtonBackground::ButtonType::kLeadingBorder,
        copy_index);
    CreateAndConfigureButton(
        base::BindRepeating(cancel_and_evaluate, base::Unretained(menu),
                            menu_model, paste_index),
        IDS_PASTE, InMenuButtonBackground::ButtonType::kLeadingBorder,
        paste_index);
  }
  CutCopyPasteView(const CutCopyPasteView&) = delete;
  CutCopyPasteView& operator=(const CutCopyPasteView&) = delete;

  // Overridden from View.
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override {
    // Returned height doesn't matter as MenuItemView forces everything to the
    // height of the menuitemview.
    return {
        GetMaxChildViewPreferredWidth() * static_cast<int>(children().size()),
        0};
  }

  void Layout(PassKey) override {
    // All buttons are given the same width.
    int width = GetMaxChildViewPreferredWidth();
    int x = 0;
    for (views::View* child : children()) {
      child->SetBounds(x, 0, width, height());
      x += width;
    }
  }

 private:
  // Returns the max preferred width of all the children.
  int GetMaxChildViewPreferredWidth() const {
    int width = 0;
    for (const views::View* child : children()) {
      width = std::max(width, child->GetPreferredSize().width());
    }
    return width;
  }
};

BEGIN_METADATA(AppMenu, CutCopyPasteView)
ADD_READONLY_PROPERTY_METADATA(int, MaxChildViewPreferredWidth)
END_METADATA

// ZoomView --------------------------------------------------------------------

// ZoomView contains the various zoom controls: two buttons to increase/decrease
// the zoom, a label showing the current zoom percent, and a button to go
// full-screen.
class AppMenu::ZoomView : public AppMenuView {
  METADATA_HEADER(ZoomView, AppMenuView)

 public:
  ZoomView(AppMenu* menu,
           ButtonMenuItemModel* menu_model,
           size_t decrement_index,
           size_t increment_index,
           size_t fullscreen_index)
      : AppMenuView(menu, menu_model) {
    browser_zoom_subscription_ =
        zoom::ZoomEventManager::GetForBrowserContext(menu->browser_->profile())
            ->AddZoomLevelChangedCallback(
                base::BindRepeating(&AppMenu::ZoomView::OnZoomLevelChanged,
                                    base::Unretained(this)));

    const auto activate = [](ButtonMenuItemModel* menu_model, size_t index) {
      menu_model->ActivatedAt(index);
    };
    auto image_model = ui::ImageModel::FromVectorIcon(
        kZoomMinusMenuRefreshIcon, ui::kColorMenuItemForeground);
    decrement_button_ = CreateButtonWithAccessibleName(
        base::BindRepeating(activate, menu_model, decrement_index),
        IDS_ZOOM_MINUS2, InMenuButtonBackground::ButtonType::kNoBorder,
        decrement_index, IDS_ACCNAME_ZOOM_MINUS2,
        /*add_accelerator_text=*/false,
        /*use_accessible_name_as_tooltip_text=*/true,
        /*image_model=*/image_model);

    auto zoom_label = std::make_unique<Label>(base::FormatPercent(100));
    zoom_label->SetAutoColorReadabilityEnabled(false);
    zoom_label->SetHorizontalAlignment(gfx::ALIGN_RIGHT);
    zoom_label->SetBorder(views::CreateEmptyBorder(gfx::Insets::TLBR(
        0, kZoomLabelHorizontalPadding, 0, kZoomLabelHorizontalPadding)));
    zoom_label->SetEnabledColorId(ui::kColorMenuItemForeground);

    // Need to set a font list for the zoom label width calculations.
    zoom_label->SetFontList(MenuConfig::instance().font_list);

    // TODO(accessibility): an alert role below is necessary because it
    // guarantees and forces screen readers across platforms to read out.
    // However, it is not ideal because an alert role, according to ARIA
    // standards, gets treated like a live region. Thus, it may be read out on
    // creation and mutation. Screen readers also prefixes the read out with
    // 'alert' giving zoom read outs questionable priority.
    //
    // Do not do this for ChromeOS and investigate whether it is possible to do
    // so for all other platforms.
#if !BUILDFLAG(IS_CHROMEOS)
    // An accessibility role of kAlert will ensure that any updates to the zoom
    // level can be picked up by screen readers.
    zoom_label->GetViewAccessibility().SetRole(ax::mojom::Role::kAlert);
#endif  // !BUILDFLAG(IS_CHROMEOS)

    zoom_label_ = AddChildView(std::move(zoom_label));

    image_model = ui::ImageModel::FromVectorIcon(kZoomPlusMenuRefreshIcon,
                                                 ui::kColorMenuItemForeground);
    increment_button_ = CreateButtonWithAccessibleName(
        base::BindRepeating(activate, menu_model, increment_index),
        IDS_ZOOM_PLUS2, InMenuButtonBackground::ButtonType::kNoBorder,
        increment_index, IDS_ACCNAME_ZOOM_PLUS2, /*add_accelerator_text=*/false,
        /*use_accessible_name_as_tooltip_text=*/true,
        /*image_model=*/image_model);

    auto fullscreen_button = std::make_unique<FullscreenButton>(
        base::BindRepeating(
            [](AppMenu* menu, ButtonMenuItemModel* menu_model, size_t index) {
              menu->CancelAndEvaluate(menu_model, index);
            },
            menu, menu_model, fullscreen_index),
        menu_model, fullscreen_index,
        menu->browser_->window() && menu->browser_->window()->IsFullscreen());
    const auto& fullscreen_icon = kFullscreenRefreshIcon;
    fullscreen_button->SetImageModel(
        ImageButton::STATE_NORMAL,
        ui::ImageModel::FromVectorIcon(fullscreen_icon,
                                       ui::kColorMenuItemForeground));
    auto hovered_fullscreen_image = ui::ImageModel::FromVectorIcon(
        fullscreen_icon, ui::kColorMenuItemForegroundSelected);
    fullscreen_button->SetImageModel(ImageButton::STATE_HOVERED,
                                     hovered_fullscreen_image);
    fullscreen_button->SetImageModel(ImageButton::STATE_PRESSED,
                                     hovered_fullscreen_image);

    // all buttons on menu should must be a custom button in order for
    // the keyboard navigation to work.
    DCHECK(views::IsViewClass<views::Button>(fullscreen_button.get()));
    fullscreen_button_ = AddChildView(std::move(fullscreen_button));

    // The max width for `zoom_label_` should not be valid until the calls into
    // UpdateZoomControls().
    DCHECK(!zoom_label_max_width_.has_value());
    UpdateZoomControls(/*on_construction=*/true);
  }
  ZoomView(const ZoomView&) = delete;
  ZoomView& operator=(const ZoomView&) = delete;
  ~ZoomView() override = default;

  // Overridden from View.
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override {
    // The increment/decrement button are forced to the same width.
    int button_width = std::max(increment_button_->GetPreferredSize().width(),
                                decrement_button_->GetPreferredSize().width());
    int fullscreen_width =
        fullscreen_button_->GetPreferredSize().width() + kFullscreenPadding;
    // Returned height doesn't matter as MenuItemView forces everything to the
    // height of the menuitemview. Note that we have overridden the height when
    // constructing the menu.
    return gfx::Size(
        button_width + GetZoomLabelMaxWidth() + button_width + fullscreen_width,
        0);
  }

  void Layout(PassKey) override {
    int x = 0;
    int button_width = std::max(increment_button_->GetPreferredSize().width(),
                                decrement_button_->GetPreferredSize().width());
    gfx::Rect bounds(0, 0, button_width, height());

    decrement_button_->SetBoundsRect(bounds);

    x += bounds.width();
    bounds.set_x(x);
    bounds.set_width(GetZoomLabelMaxWidth());
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

 private:
  const content::WebContents* GetActiveWebContents() const {
    return menu() ? menu()->browser_->tab_strip_model()->GetActiveWebContents()
                  : nullptr;
  }
  content::WebContents* GetActiveWebContents() {
    return const_cast<content::WebContents*>(
        static_cast<const AppMenu::ZoomView*>(this)->GetActiveWebContents());
  }

  void OnZoomLevelChanged(const content::HostZoomMap::ZoomLevelChange& change) {
    UpdateZoomControls();
  }

  void UpdateZoomControls(bool on_construction = false) {
    WebContents* contents = GetActiveWebContents();
    int zoom = 100;
    if (contents) {
      const auto* zoom_controller =
          zoom::ZoomController::FromWebContents(contents);
      if (zoom_controller)
        zoom = zoom_controller->GetZoomPercent();
      increment_button_->SetEnabled(zoom < contents->GetMaximumZoomPercent());
      decrement_button_->SetEnabled(zoom > contents->GetMinimumZoomPercent());
    }
    zoom_label_->SetText(base::FormatPercent(zoom));
    if (!on_construction) {
      // An alert notification will ensure that the zoom label is always
      // announced even if is not focusable.
      zoom_label_->NotifyAccessibilityEvent(ax::mojom::Event::kAlert, true);
    }
    zoom_label_max_width_.reset();
  }

  // Returns the max width the zoom string can be.
  int GetZoomLabelMaxWidth() const {
    if (!zoom_label_max_width_) {
      const gfx::FontList& font_list = zoom_label_->font_list();
      const int border_width = zoom_label_->GetInsets().width();

      int max_w = 0;

      const WebContents* selected_tab = GetActiveWebContents();
      if (selected_tab) {
        const auto* zoom_controller =
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
    }
    return zoom_label_max_width_.value();
  }

  base::CallbackListSubscription browser_zoom_subscription_;

  // Button for incrementing the zoom.
  raw_ptr<Button> increment_button_ = nullptr;

  // Label showing zoom as a percent.
  raw_ptr<Label> zoom_label_ = nullptr;

  // Button for decrementing the zoom.
  raw_ptr<Button> decrement_button_ = nullptr;

  raw_ptr<Button> fullscreen_button_ = nullptr;

  // Cached width of how wide the zoom label string can be. This is the width at
  // 100%. This should not be accessed directly, use GetZoomLabelMaxWidth()
  // instead. This value is cached because is depends on multiple calls to
  // gfx::GetStringWidth(...) which are expensive.
  mutable std::optional<int> zoom_label_max_width_;
};

BEGIN_METADATA(AppMenu, ZoomView)
ADD_READONLY_PROPERTY_METADATA(int, ZoomLabelMaxWidth)
END_METADATA

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

  // ui::MenuModelDelegate implementation:

  void OnIconChanged(int command_id) override {
    ui::MenuModel* model = model_;
    size_t index;
    model_->GetModelAndIndexForCommandId(command_id, &model, &index);
    views::MenuItemView* item = menu_item_->GetMenuItemByID(command_id);
    DCHECK(item);
    item->SetIcon(model->GetIconAt(index));
  }

  void OnMenuStructureChanged() override {
    if (menu_item_->HasSubmenu()) {
      // Remove all menu items from submenu.
      menu_item_->RemoveAllMenuItems();

      // Remove all elements in |AppMenu::command_id_to_entry_| that map to
      // |model_| or any sub menu models.
      base::flat_set<int> descendant_command_ids;
      GetDescendantCommandIds(model_, &descendant_command_ids);
      auto iter = app_menu_->command_id_to_entry_.begin();
      while (iter != app_menu_->command_id_to_entry_.end()) {
        if (descendant_command_ids.find(iter->first) !=
            descendant_command_ids.end()) {
          app_menu_->command_id_to_entry_.erase(iter++);
        } else {
          ++iter;
        }
      }
    }

    // Add all menu items from |model| to submenu.
    BuildMenu(menu_item_, model_);

    // In case recent tabs submenu was open when items were changing, force a
    // ChildrenChanged().
    menu_item_->ChildrenChanged();
  }

 private:
  const raw_ptr<AppMenu> app_menu_;
  const raw_ptr<ui::MenuModel> model_;
  const raw_ptr<views::MenuItemView> menu_item_;

  // Recursive helper function for OnMenuStructureChanged() which builds the
  // |menu| and all descendant submenus.
  void BuildMenu(MenuItemView* menu, ui::MenuModel* model) {
    DCHECK(menu);
    DCHECK(model);
    const size_t item_count = model->GetItemCount();
    for (size_t i = 0; i < item_count; ++i) {
      MenuItemView* const item =
          app_menu_->AddMenuItem(menu, i, model, i, model->GetTypeAt(i));
      if (model->GetTypeAt(i) == ui::MenuModel::TYPE_SUBMENU ||
          model->GetTypeAt(i) == ui::MenuModel::TYPE_ACTIONABLE_SUBMENU) {
        DCHECK(item);
        DCHECK(item->GetType() == MenuItemView::Type::kSubMenu ||
               item->GetType() == MenuItemView::Type::kActionableSubMenu);
        BuildMenu(item, model->GetSubmenuModelAt(i));
      }
    }
  }

  // Populates out_set with all command ids referenced by the model, including
  // those referenced by sub menu models.
  void GetDescendantCommandIds(MenuModel* model, base::flat_set<int>* out_set) {
    const size_t item_count = model->GetItemCount();
    for (size_t i = 0; i < item_count; ++i) {
      out_set->insert(model->GetCommandIdAt(i));
      if (model->GetTypeAt(i) == ui::MenuModel::TYPE_SUBMENU ||
          model->GetTypeAt(i) == ui::MenuModel::TYPE_ACTIONABLE_SUBMENU) {
        GetDescendantCommandIds(model->GetSubmenuModelAt(i), out_set);
      }
    }
  }
};

// AppMenu ------------------------------------------------------------------

AppMenu::AppMenu(Browser* browser, ui::MenuModel* model, int run_types)
    : browser_(browser), model_(model), run_types_(run_types) {
  global_error_observation_.Observe(
      GlobalErrorServiceFactory::GetForProfile(browser->profile()));

  DCHECK(!root_);
  auto root = std::make_unique<MenuItemView>(/*delegate=*/this);
  root_ = root.get();
  PopulateMenu(root_, model);

  int32_t types = views::MenuRunner::HAS_MNEMONICS;
  if (for_drop()) {
    // We add NESTED_DRAG since currently the only operation to open the app
    // menu for is an extension action drag, which is controlled by the child
    // BrowserActionsContainer view.
    types |= views::MenuRunner::FOR_DROP | views::MenuRunner::NESTED_DRAG;
  }
  if (run_types_ & views::MenuRunner::SHOULD_SHOW_MNEMONICS) {
    types |= views::MenuRunner::SHOULD_SHOW_MNEMONICS;
  }

  menu_runner_ = std::make_unique<views::MenuRunner>(std::move(root), types);
}

AppMenu::~AppMenu() {
  if (bookmark_menu_delegate_.get()) {
    BookmarkModel* model =
        BookmarkModelFactory::GetForBrowserContext(browser_->profile());
    if (model) {
      model->RemoveObserver(this);
    }
  }
}

void AppMenu::RunMenu(views::MenuButtonController* host) {
  base::RecordAction(UserMetricsAction("ShowAppMenu"));
  UMA_HISTOGRAM_ENUMERATION("WrenchMenu.MenuAction", MENU_ACTION_MENU_OPENED,
                            LIMIT_MENU_ACTION);

  menu_runner_->RunMenuAt(
      host->button()->GetWidget(), host,
      host->button()->GetAnchorBoundsInScreen(),
      views::MenuAnchorPosition::kTopRight, ui::MENU_SOURCE_NONE,
      /*native_view_for_gestures=*/gfx::NativeView(), /*corners=*/std::nullopt,
      "Chrome.AppMenu.MenuHostInitToNextFramePresented");
}

void AppMenu::CloseMenu() {
  if (menu_runner_.get())
    menu_runner_->Cancel();
}

bool AppMenu::IsShowing() const {
  return menu_runner_.get() && menu_runner_->IsRunning();
}

const gfx::FontList* AppMenu::GetLabelFontList(int command_id) const {
  ui::MenuModel* model = model_;
  size_t index = 0;
  ui::MenuModel::GetModelAndIndexForCommandId(command_id, &model, &index);
  return model->GetLabelFontListAt(index);
}

std::optional<SkColor> AppMenu::GetLabelColor(int command_id) const {
  // Only return a color if there's a font list - otherwise this method will
  // return a color for every recent tab item, not just the header.
  // Ensure that we call GetColor() using the `root_`'s SubmenuView as this is
  // the content view for the menu's widget. The root MenuItemView itself is not
  // a member of a Widget hierarchy and thus does not have the necessary context
  // to correctly determine the label color as this requires querying the View's
  // hosting widget (crbug.com/1233392).
  return GetLabelFontList(command_id)
             ? std::make_optional(
                   root_->GetSubmenu()->GetColorProvider()->GetColor(
                       views::TypographyProvider::Get().GetColorId(
                           views::style::CONTEXT_MENU,
                           views::style::STYLE_PRIMARY)))
             : std::nullopt;
}

std::u16string AppMenu::GetTooltipText(int command_id,
                                       const gfx::Point& p) const {
  return IsBookmarkCommand(command_id)
             ? bookmark_menu_delegate_->GetTooltipText(command_id, p)
             : std::u16string();
}

bool AppMenu::IsTriggerableEvent(views::MenuItemView* menu,
                                 const ui::Event& e) {
  return IsBookmarkCommand(menu->GetCommand())
             ? bookmark_menu_delegate_->IsTriggerableEvent(menu, e)
             : MenuDelegate::IsTriggerableEvent(menu, e);
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

ui::mojom::DragOperation AppMenu::GetDropOperation(
    MenuItemView* item,
    const ui::DropTargetEvent& event,
    DropPosition* position) {
  return IsBookmarkCommand(item->GetCommand())
             ? bookmark_menu_delegate_->GetDropOperation(item, event, position)
             : ui::mojom::DragOperation::kNone;
}

views::View::DropCallback AppMenu::GetDropCallback(
    views::MenuItemView* menu,
    DropPosition position,
    const ui::DropTargetEvent& event) {
  if (!IsBookmarkCommand(menu->GetCommand()))
    return base::DoNothing();

  return bookmark_menu_delegate_->GetDropCallback(menu, position, event);
}

bool AppMenu::ShowContextMenu(MenuItemView* source,
                              int command_id,
                              const gfx::Point& p,
                              ui::MenuSourceType source_type) {
  return IsBookmarkCommand(command_id)
             ? bookmark_menu_delegate_->ShowContextMenu(source, command_id, p,
                                                        source_type)
             : false;
}

bool AppMenu::CanDrag(MenuItemView* menu) {
  return IsBookmarkCommand(menu->GetCommand())
             ? bookmark_menu_delegate_->CanDrag(menu)
             : false;
}

void AppMenu::WriteDragData(MenuItemView* sender, ui::OSExchangeData* data) {
  DCHECK(IsBookmarkCommand(sender->GetCommand()));
  return bookmark_menu_delegate_->WriteDragData(sender, data);
}

int AppMenu::GetDragOperations(MenuItemView* sender) {
  return IsBookmarkCommand(sender->GetCommand())
             ? bookmark_menu_delegate_->GetDragOperations(sender)
             : MenuDelegate::GetDragOperations(sender);
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
  if (command_id <= 0) {
    return false;  // The root item, a separator, or a title.
  }

  if (command_id == IDC_CREATE_NEW_TAB_GROUP) {
    return true;
  }

  if (IsTabGroupsCommand(command_id)) {
    return stg_everything_menu_->ShouldEnableCommand(command_id);
  }

  if (IsBookmarkCommand(command_id) ||
      command_id == IDC_SHOW_BOOKMARK_SIDE_PANEL) {
    return true;
  }

  if (command_id == IDC_MORE_TOOLS_MENU) {
    return true;
  }

  if (features::IsExtensionMenuInRootAppMenu() &&
      command_id == IDC_EXTENSIONS_SUBMENU) {
    return true;
  }

  if (command_id == IDC_SHARING_HUB_MENU)
    return true;

  // The items representing the cut menu (cut/copy/paste), zoom menu
  // (increment/decrement/reset) and extension toolbar view are always enabled.
  // The child views of these items enabled state updates appropriately.
  if (command_id == IDC_EDIT_MENU || command_id == IDC_ZOOM_MENU)
    return true;

  // If `command_id` is not added to App Menu via MenuModel, you should handle
  // it in the code above. `command_id_to_entry_` traces only MenuModel entries.
  auto it = command_id_to_entry_.find(command_id);
  CHECK(it != command_id_to_entry_.end());

  const Entry& entry = it->second;
  return entry.first->IsEnabledAt(entry.second);
}

void AppMenu::ExecuteCommand(int command_id, int mouse_event_flags) {
  if (command_id == IDC_CREATE_NEW_TAB_GROUP ||
      IsTabGroupsCommand(command_id)) {
    if (command_id != IDC_CREATE_NEW_TAB_GROUP) {
      base::RecordAction(base::UserMetricsAction(
          "TabGroups_SavedTabGroups_OpenedFromAppMenu"));
    }
    stg_everything_menu_->ExecuteCommand(command_id, mouse_event_flags);
    return;
  }

  if (IsBookmarkCommand(command_id)) {
    UMA_HISTOGRAM_MEDIUM_TIMES("WrenchMenu.TimeToAction.OpenBookmark",
                               menu_opened_timer_.Elapsed());
    UMA_HISTOGRAM_ENUMERATION("WrenchMenu.MenuAction",
                              MENU_ACTION_BOOKMARK_OPEN, LIMIT_MENU_ACTION);
    bookmark_menu_delegate_->ExecuteCommand(command_id, mouse_event_flags);
    return;
  }

  if (command_id == IDC_EDIT_MENU || command_id == IDC_ZOOM_MENU) {
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
  if (command_id < 0) {
    // This is a non-interactive title.
    return false;
  }

  if (command_id == IDC_CREATE_NEW_TAB_GROUP ||
      IsTabGroupsCommand(command_id)) {
    return false;
  }

  if (IsBookmarkCommand(command_id) ||
      command_id == IDC_SHOW_BOOKMARK_SIDE_PANEL) {
    return false;
  }

  if (command_id == IDC_EDIT_MENU || command_id == IDC_ZOOM_MENU) {
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
  if (menu == saved_tab_groups_menu_) {
    UMA_HISTOGRAM_MEDIUM_TIMES("WrenchMenu.TimeToAction.ShowSavedTabGroups",
                               menu_opened_timer_.Elapsed());
    UMA_HISTOGRAM_ENUMERATION("WrenchMenu.MenuAction",
                              MENU_ACTION_SHOW_SAVED_TAB_GROUPS,
                              LIMIT_MENU_ACTION);
    stg_everything_menu_ =
          std::make_unique<tab_groups::STGEverythingMenu>(nullptr, browser_);
    stg_everything_menu_->SetShowSubmenu(true);
    stg_everything_menu_->PopulateMenu(menu);
  } else if (IsTabGroupsCommand(menu->GetCommand())) {
    stg_everything_menu_->PopulateTabGroupSubMenu(menu);
  } else if (menu == bookmark_menu_) {
    CreateBookmarkMenu();
  } else if (bookmark_menu_delegate_) {
    bookmark_menu_delegate_->WillShowMenu(menu);
  }
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
    menu->GetSubmenu()->GetWidget()->SetVisibilityChangedAnimationsEnabled(
        false);
  }
}

bool AppMenu::ShouldCloseOnDragComplete() {
  return false;
}

void AppMenu::OnMenuClosed(views::MenuItemView* menu) {
  // If the menu contained a Safety Hub notification, mark as trigger for HaTS
  // survey if the menu was open for at least 5 seconds.
  static constexpr auto kSafetyHubCommandIds =
      std::array{IDC_OPEN_SAFETY_HUB, IDC_SAFETY_HUB_MANAGE_EXTENSIONS,
                 IDC_SAFETY_HUB_SHOW_PASSWORD_CHECKUP};
  const bool has_safety_hub_notification = base::ranges::any_of(
      kSafetyHubCommandIds,
      [&](int id) { return command_id_to_entry_.contains(id); });
  if (has_safety_hub_notification &&
      menu_opened_timer_.Elapsed() >= base::Seconds(5)) {
    if (SafetyHubHatsService* hats_service =
            SafetyHubHatsServiceFactory::GetForProfile(browser_->profile())) {
      hats_service->SafetyHubNotificationSeen();
    }
  }

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
  return (IsRecentTabsCommand(command_id) && event.IsMouseEvent() &&
          (ui::DispositionFromEventFlags(event.flags()) ==
           WindowOpenDisposition::NEW_BACKGROUND_TAB)) ||
         (IsBookmarkCommand(command_id) &&
          bookmark_menu_delegate_->ShouldExecuteCommandWithoutClosingMenu(
              command_id, event)) ||
         // TODO(crbug.com/40272266) Currently the UI for
         // OtherProfileCommand has bespoke child views which will block
         // activation in ui/views/controls/menu/menu_controller.cc. The correct
         // fix will be to have the child view as part of
         // ui/views/controls/menu/menu_item_view.cc but we will currently
         // bypass this by using this method to allow the command to trigger.
         (IsOtherProfileCommand(command_id) && event.IsMouseEvent());
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

views::View* AppMenu::GetZoomAppMenuViewForTest() {
  std::optional<int> zoom_view_command_id = IDC_ZOOM_MENU;
  auto* menu_item = root_->GetMenuItemByID(zoom_view_command_id.value());
  DCHECK(menu_item);

  for (views::View* child : menu_item->children()) {
    if (views::IsViewClass<ZoomView>(child)) {
      return child;
    }
  }
  return nullptr;
}

void AppMenu::PopulateMenu(MenuItemView* parent, MenuModel* model) {
  for (size_t i = 0, max = model->GetItemCount(); i < max; ++i) {
    // Add the menu item at the end.
    size_t menu_index = parent->HasSubmenu()
                            ? parent->GetSubmenu()->children().size()
                            : size_t{0};
    MenuItemView* item =
        AddMenuItem(parent, menu_index, model, i, model->GetTypeAt(i));

    if (model->GetTypeAt(i) == MenuModel::TYPE_SUBMENU) {
      PopulateMenu(item, model->GetSubmenuModelAt(i));
    }

    // Helper method that adds a background to a menu item.
    auto add_menu_row_background = [item](int vertical_margin,
                                          ui::ColorId background_color_id) {
      constexpr int kBackgroundCornerRadius = 12;
      item->set_vertical_margin(vertical_margin);
      item->SetMenuItemBackground(MenuItemView::MenuItemBackground(
          background_color_id, kBackgroundCornerRadius));
      item->SetSelectedColorId(ui::kColorAppMenuRowBackgroundHovered);
    };

    switch (model->GetCommandIdAt(i)) {
      case IDC_PROFILE_MENU_IN_APP_MENU: {
          add_menu_row_background(
              ChromeLayoutProvider::Get()->GetDistanceMetric(
                  DISTANCE_CONTENT_LIST_VERTICAL_MULTI),
              ui::kColorAppMenuProfileRowBackground);
          ProfileAttributesEntry* profile_attributes =
              GetProfileAttributesFromProfile(browser_->profile());
          if (profile_attributes &&
              !profile_attributes->GetLocalProfileName().empty()) {
            const MenuConfig& config = MenuConfig::instance();
            AddSignedInChipToProfileMenuItem(
                browser_->profile(), item,
                config.arrow_to_edge_padding + config.arrow_size,
                profile_menu_item_selected_subscription_list_);
          }
        break;
      }
      case IDC_UPGRADE_DIALOG: {
        add_menu_row_background(12, ui::kColorAppMenuUpgradeRowBackground);
        if (const auto upgrade_substring_text = GetUpgradeDialogSubstringText();
            !upgrade_substring_text.empty()) {
          item->AddChildView(
              views::Builder<views::Label>()
                  .SetText(upgrade_substring_text)
                  .SetEnabledColorId(
                      ui::kColorAppMenuUpgradeRowSubstringForeground)
                  .SetEnabled(true)
                  .SetBorder(views::CreateEmptyBorder(gfx::Insets::VH(
                      0, views::LayoutProvider::Get()->GetDistanceMetric(
                             views::DISTANCE_RELATED_LABEL_HORIZONTAL) -
                             MenuItemView::kChildHorizontalPadding)))
                  .Build());
          item->SetHighlightWhenSelectedWithChildViews(true);
          item->SetTriggerActionWithNonIconChildViews(true);
        }
        break;
      }
      case IDC_SET_BROWSER_AS_DEFAULT: {
        // Only highlight the default browser item when it is first in the
        // AppMenu.
        if (i == 0) {
          add_menu_row_background(
              views::LayoutProvider::Get()->GetDistanceMetric(
                  views::DISTANCE_CONTROL_VERTICAL_TEXT_PADDING),
              ui::kColorAppMenuUpgradeRowBackground);
        }
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
        item->set_children_use_full_width(true);
        break;
      }

      case IDC_ZOOM_MENU: {
        ui::ButtonMenuItemModel* submodel = model->GetButtonMenuItemAt(i);
        DCHECK_EQ(IDC_ZOOM_MINUS, submodel->GetCommandIdAt(0));
        DCHECK_EQ(IDC_ZOOM_PLUS, submodel->GetCommandIdAt(1));
        DCHECK_EQ(IDC_FULLSCREEN, submodel->GetCommandIdAt(2));
        item->SetTitle(l10n_util::GetStringUTF16(IDS_ZOOM_MENU2));
        item->AddChildView(std::make_unique<ZoomView>(this, submodel, 0, 1, 2));
        item->set_children_use_full_width(true);
        break;
      }

      case IDC_BOOKMARKS_MENU:
        DCHECK(!bookmark_menu_);
        bookmark_menu_ = item;
        break;

      case IDC_SAVED_TAB_GROUPS_MENU:
        DCHECK(!saved_tab_groups_menu_);
        saved_tab_groups_menu_ = item;
        break;

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
      case IDC_FEEDBACK:
        DCHECK(!feedback_menu_item_);
        feedback_menu_item_ = item;
        break;
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
      case IDC_TAKE_SCREENSHOT:
        DCHECK(!screenshot_menu_item_);
        screenshot_menu_item_ = item;
        break;
#endif

      case IDC_RECENT_TABS_MENU:
        DCHECK(!recent_tabs_menu_model_delegate_.get());
        recent_tabs_menu_model_delegate_ =
            std::make_unique<RecentTabsMenuModelDelegate>(
                this, model->GetSubmenuModelAt(i), item);
        break;
      case IDC_SHOW_MANAGEMENT_PAGE:
        parent->SetTooltip(model->GetAccessibleNameAt(i),
                           IDC_SHOW_MANAGEMENT_PAGE);
        break;
      default:
        break;
    }
  }
}

MenuItemView* AppMenu::AddMenuItem(MenuItemView* parent,
                                   size_t menu_index,
                                   MenuModel* model,
                                   size_t model_index,
                                   MenuModel::ItemType menu_type) {
  const int command_id = model->GetCommandIdAt(model_index);
  if (command_id >= 0) {  // Don't add separators to |command_id_to_entry_|.
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
    menu_item->SetVisible(model->IsVisibleAt(model_index));

    if (menu_type == MenuModel::TYPE_COMMAND) {
      menu_item->SetIcon(model->GetIconAt(model_index));
    }
  }

  return menu_item;
}

void AppMenu::CancelAndEvaluate(ButtonMenuItemModel* model, size_t index) {
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
  bookmark_menu_delegate_ =
      std::make_unique<BookmarkMenuDelegate>(browser_, parent);
  bookmark_menu_delegate_->Init(this, bookmark_menu_,
                                model->bookmark_bar_node(), 0,
                                BookmarkMenuDelegate::SHOW_PERMANENT_FOLDERS,
                                BookmarkLaunchLocation::kAppMenu);
}

size_t AppMenu::ModelIndexFromCommandId(int command_id) const {
  auto ix = command_id_to_entry_.find(command_id);
  CHECK(ix != command_id_to_entry_.end(), base::NotFatalUntil::M130);
  return ix->second.second;
}

void AppMenu::SetTimerForTesting(base::ElapsedTimer timer) {
  menu_opened_timer_ = std::move(timer);
}
