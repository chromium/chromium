// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_group_editor_bubble_view.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/containers/adapters.h"
#include "base/containers/flat_map.h"
#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/collaboration/collaboration_service_factory.h"
#include "chrome/browser/data_sharing/data_sharing_service_factory.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_group_sync/feature_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/tabs/features.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_metrics.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_pref_names.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_utils.h"
#include "chrome/browser/ui/tabs/tab_group_deletion_dialog_controller.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_delegate.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/bubble_menu_item_factory.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/controls/hover_button.h"
#include "chrome/browser/ui/views/data_sharing/collaboration_controller_delegate_desktop.h"
#include "chrome/browser/ui/views/data_sharing/data_sharing_bubble_controller.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/global_media_controls/media_item_ui_helper.h"
#include "chrome/browser/ui/views/tabs/color_picker_view.h"
#include "chrome/browser/ui/views/tabs/groups/avatar_container_view.h"
#include "chrome/browser/ui/views/tabs/groups/manage_sharing_row.h"
#include "chrome/browser/ui/views/tabs/recent_activity_bubble_dialog_view.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/ui/views/toolbar/toolbar_ink_drop_util.h"
#include "chrome/browser/user_education/tutorial_identifiers.h"
#include "chrome/browser/user_education/user_education_service.h"
#include "chrome/browser/user_education/user_education_service_factory.h"
#include "chrome/grit/generated_resources.h"
#include "components/collaboration/public/collaboration_service.h"
#include "components/data_sharing/public/features.h"
#include "components/data_sharing/public/group_data.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/saved_tab_groups/public/features.h"
#include "components/saved_tab_groups/public/saved_tab_group.h"
#include "components/saved_tab_groups/public/types.h"
#include "components/tab_groups/tab_group_color.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "components/tabs/public/tab_group.h"
#include "tab_group_editor_bubble_view.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/dialog_model_field.h"
#include "ui/base/models/image_model.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/mojom/menu_source_type.mojom-forward.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/base/pointer/touch_ui_controller.h"
#include "ui/compositor/layer.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/range/range.h"
#include "ui/gfx/text_constants.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_host.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/toggle_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/separator.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_utils.h"

namespace {
// The amount of vertical padding in dips the separator should have to
// prevent menu items from being visually too close to each other.
constexpr int kSeparatorPadding = 8;
// The width of the tab group editor bubble.
constexpr int kDialogWidth = 240;
// The default size of icons that are displayed in the tab group editor bubble.
static constexpr int kDefaultIconSize = 20;
// The maximum number of times we will show the footer section with the learn
// more link.
constexpr int kFooterDisplayLimit = 5;

int GetHorizontalSpacing() {
  return ChromeLayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_RELATED_CONTROL_HORIZONTAL);
}

int GetVerticalSpacing() {
  return ChromeLayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_RELATED_CONTROL_VERTICAL);
}

gfx::Insets GetControlInsets() {
  const int horizontal_spacing = GetHorizontalSpacing();
  const int vertical_spacing = GetVerticalSpacing();

  return ui::TouchUiController::Get()->touch_ui()
             ? gfx::Insets::VH(5 * vertical_spacing / 4, horizontal_spacing)
             : gfx::Insets::VH(vertical_spacing, horizontal_spacing);
}

std::unique_ptr<views::LabelButton> CreateMenuItem(
    int button_id,
    const std::u16string& name,
    views::Button::PressedCallback callback,
    const ui::ImageModel& icon = ui::ImageModel(),
    const std::u16string& accelerator_text = std::u16string()) {
  const gfx::Insets control_insets = GetControlInsets();
  auto button = CreateBubbleMenuItem(button_id, name, std::move(callback), icon,
                                     accelerator_text);
  button->SetBorder(views::CreateEmptyBorder(control_insets));
  button->SetLabelStyle(views::style::STYLE_BODY_3_EMPHASIS);

  return button;
}

std::u16string GetAcceleratorText(int command_id, const Browser* browser) {
  if (!browser || !tabs::AreTabGroupShortcutsEnabled()) {
    return std::u16string();
  }

  ui::Accelerator accelerator;
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  if (!browser_view ||
      !browser_view->GetAccelerator(command_id, &accelerator)) {
    return std::u16string();
  }

  return accelerator.GetShortcutText();
}

}  // namespace

namespace saved_tab_group_prefs = tab_groups::saved_tab_groups::prefs;
namespace shared_tab_group_metrics = tab_groups::saved_tab_groups::metrics;

// static
views::Widget* TabGroupEditorBubbleView::Show(
    const Browser* browser,
    const tab_groups::TabGroupId& group,
    TabGroupHeader* header_view,
    std::optional<gfx::Rect> anchor_rect,
    views::View* anchor_view,
    bool stop_context_menu_propagation) {
  feature_engagement::TrackerFactory::GetForBrowserContext(browser->profile())
      ->NotifyEvent("tab_group_editor_shown");

  // If `header_view` is not null, use `header_view` as the `anchor_view`.
  TabGroupEditorBubbleView* tab_group_editor_bubble_view =
      new TabGroupEditorBubbleView(browser, group,
                                   header_view ? header_view : anchor_view,
                                   anchor_rect, stop_context_menu_propagation);
  views::Widget* const widget =
      BubbleDialogDelegateView::CreateBubble(tab_group_editor_bubble_view);
  tab_group_editor_bubble_view->set_adjust_if_offscreen(true);
  tab_group_editor_bubble_view->GetBubbleFrameView()
      ->SetPreferredArrowAdjustment(
          views::BubbleFrameView::PreferredArrowAdjustment::kOffset);
  widget->Show();
  return widget;
}

views::View* TabGroupEditorBubbleView::GetInitiallyFocusedView() {
  return title_field_;
}

gfx::Rect TabGroupEditorBubbleView::GetAnchorRect() const {
  // We want to avoid calling BubbleDialogDelegateView::GetAnchorRect() if
  // `anchor_rect_` has been set. This is because the default behavior uses the
  // anchor view's bounds and also updates `anchor_rect_` to the views bounds.
  // It does this so that the bubble does not jump when the anchoring view is
  // deleted.
  if (use_set_anchor_rect_) {
    return anchor_rect().value();
  }
  return BubbleDialogDelegateView::GetAnchorRect();
}

void TabGroupEditorBubbleView::AddedToWidget() {
  const auto* const color_provider = GetColorProvider();

  for (views::LabelButton* menu_item : simple_menu_items_) {
    const bool enabled = menu_item->GetEnabled();
    views::Button::ButtonState button_state =
        enabled ? views::Button::STATE_NORMAL : views::Button::STATE_DISABLED;

    const SkColor text_color = menu_item->GetCurrentTextColor();

    const SkColor enabled_icon_color =
        color_provider->GetColor(kColorTabGroupDialogIconEnabled);
    const SkColor icon_color = enabled ? enabled_icon_color : text_color;

    const std::optional<ui::ImageModel>& old_image_model =
        menu_item->GetImageModel(button_state);
    if (old_image_model.has_value() && !old_image_model->IsEmpty() &&
        old_image_model->IsVectorIcon()) {
      ui::VectorIconModel vector_icon_model = old_image_model->GetVectorIcon();
      const gfx::VectorIcon* icon = vector_icon_model.vector_icon();
      const ui::ImageModel new_image_model = ui::ImageModel::FromVectorIcon(
          *icon, icon_color, old_image_model->Size().width());
      menu_item->SetImageModel(button_state, new_image_model);
    }
  }

  if (save_group_icon_) {
    DCHECK(save_group_label_);

    const bool enabled = save_group_icon_->GetEnabled();
    const SkColor text_color = save_group_label_->GetEnabledColor();
    const SkColor enabled_icon_color =
        color_provider->GetColor(kColorTabGroupDialogIconEnabled);
    const SkColor icon_color = enabled ? enabled_icon_color : text_color;

    const ui::ImageModel& old_image_model = save_group_icon_->GetImageModel();
    ui::VectorIconModel vector_icon_model = old_image_model.GetVectorIcon();
    const gfx::VectorIcon* icon = vector_icon_model.vector_icon();

    const ui::ImageModel saved_tab_group_line_image_model =
        ui::ImageModel::FromVectorIcon(*icon, icon_color);
    save_group_icon_->SetImage(saved_tab_group_line_image_model);
  }
}

TabGroupEditorBubbleView::TabGroupEditorBubbleView(
    const Browser* browser,
    const tab_groups::TabGroupId& group,
    views::View* anchor_view,
    std::optional<gfx::Rect> anchor_rect,
    bool stop_context_menu_propagation)
    : BubbleDialogDelegateView(anchor_view,
                               views::BubbleBorder::Arrow::TOP_LEFT,
                               views::BubbleBorder::DIALOG_SHADOW,
                               true),
      title_field_controller_(this),
      browser_(browser),
      group_(group),
      use_set_anchor_rect_(anchor_rect),
      title_at_opening_(GetGroupTitle()),
      stop_context_menu_propagation_(stop_context_menu_propagation) {
  // This dialog should only show up if the browser supports tab groups.
  DCHECK(browser_->tab_strip_model()->SupportsTabGroups());

  // `anchor_view` should always be defined as it will be used to source the
  // `anchor_widget_`.
  DCHECK(anchor_view);

  // Initialize the bubble.
  if (anchor_rect) {
    SetAnchorRect(anchor_rect.value());
  }
  set_margins(gfx::Insets());

  SetModalType(ui::mojom::ModalType::kNone);
  SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
  SetCloseCallback(base::BindOnce(&TabGroupEditorBubbleView::OnBubbleClose,
                                  base::Unretained(this)));

  RebuildMenuContents();

  // Add a separator for the delete menu item and footer v2 enabled.
  if (CanSaveGroups() && ShouldShowSavedFooter()) {
    PrefService* pref_service = browser_->profile()->GetPrefs();
    saved_tab_group_prefs::IncrementLearnMoreFooterShownCountPref(pref_service);
  }

  // Setting up the layout.
  const gfx::Insets control_insets = GetControlInsets();
  const int vertical_spacing = control_insets.top();

  gfx::Insets interior_margins = gfx::Insets::VH(vertical_spacing, 0);
  if (footer_) {
    interior_margins.set_bottom(0);
  }

  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical)
      .SetInteriorMargin(interior_margins);

  browser_->tab_strip_model()->AddObserver(this);
}

void TabGroupEditorBubbleView::RebuildMenuContents() {
  simple_menu_items_.clear();
  std::unique_ptr<TitleField> title_field;
  if (title_field_) {
    // Reuse title_field_ if one exists, but update with the latest title. This
    // is different from what we do for other child views, as creating a new
    // title_field_ with the same controller causes a crash.
    title_at_opening_ = GetGroupTitle();
    title_field = RemoveChildViewT(title_field_);
    title_field->SetText(title_at_opening_);
  }
  manage_shared_group_button_ = nullptr;
  color_selector_ = nullptr;
  save_group_icon_ = nullptr;
  save_group_label_ = nullptr;
  footer_ = nullptr;

  RemoveAllChildViews();

  title_field_ = AddChildView(title_field ? std::move(title_field)
                                          : BuildTitleField(title_at_opening_));
  color_selector_ = AddChildView(BuildColorPicker());
  AddChildView(BuildSeparator());

  if (!CanSaveGroups()) {
    simple_menu_items_.push_back(AddChildView(BuildNewTabInGroupButton()));
    simple_menu_items_.push_back(
        AddChildView(BuildMoveGroupToNewWindowButton()));
    AddChildView(BuildSeparator());
    simple_menu_items_.push_back(AddChildView(BuildUngroupButton()));
    simple_menu_items_.push_back(AddChildView(BuildCloseGroupButton()));
  } else {
    simple_menu_items_.push_back(AddChildView(BuildNewTabInGroupButton()));
    simple_menu_items_.push_back(
        AddChildView(BuildMoveGroupToNewWindowButton()));

    if (CanShareGroups()) {
      if (IsGroupShared()) {
        manage_shared_group_button_ = AddChildView(BuildManageSharingButton());
        manage_shared_group_button_->UpdateInsetsToMatchLabelButton(
            simple_menu_items_[0]);
        manage_shared_group_button_->SetProperty(
            views::kElementIdentifierKey,
            kTabGroupEditorBubbleManageSharedGroupButtonId);
      }
      if (IsAllowedToCreateSharedGroup()) {
        simple_menu_items_.push_back(AddChildView(BuildShareGroupButton()));
      }
      simple_menu_items_.push_back(AddChildView(BuildRecentActivityButton()));
    }

    simple_menu_items_.push_back(AddChildView(BuildCloseGroupButton()));
    AddChildView(BuildSeparator());

    if (!IsGroupShared()) {
      simple_menu_items_.push_back(AddChildView(BuildUngroupButton()));
    }

    if (OwnsGroup()) {
      simple_menu_items_.push_back(AddChildView(BuildDeleteGroupButton()));
    } else {
      simple_menu_items_.push_back(AddChildView(BuildLeaveGroupButton()));
    }

    if (ShouldShowSavedFooter()) {
      footer_ = AddChildView(std::make_unique<Footer>(browser_));
    }
  }

  UpdateMenuContentsVisibility();
}

void TabGroupEditorBubbleView::UpdateMenuContentsVisibility() {
  // iterate through the menu list setting the correct visibility.
  for (views::LabelButton* button : simple_menu_items_) {
    switch (button->GetID()) {
      case TAB_GROUP_HEADER_CXMENU_SHARE: {
        button->SetVisible(CanShareGroups() && !IsGroupShared());
        break;
      }
      case TAB_GROUP_HEADER_CXMENU_DELETE_GROUP: {
        button->SetVisible(IsGroupSaved() || IsGroupShared());
        break;
      }
      case TAB_GROUP_HEADER_CXMENU_MOVE_GROUP_TO_NEW_WINDOW: {
        button->SetVisible(CanMoveGroupToNewWindow());
        break;
      }
      case TAB_GROUP_HEADER_CXMENU_RECENT_ACTIVITY: {
        button->SetVisible(IsGroupShared());
        break;
      }
    }
  }
  if (manage_shared_group_button_) {
    manage_shared_group_button_->SetVisible(IsGroupShared());
  }
}

std::unique_ptr<views::Separator> TabGroupEditorBubbleView::BuildSeparator() {
  std::unique_ptr<views::Separator> separator =
      std::make_unique<views::Separator>();
  separator->SetProperty(views::kMarginsKey,
                         gfx::Insets::VH(GetControlInsets().top(), 0));
  return separator;
}

std::unique_ptr<ColorPickerView> TabGroupEditorBubbleView::BuildColorPicker() {
  const gfx::Insets control_insets = GetControlInsets();
  const int horizontal_spacing = control_insets.left();
  const tab_groups::TabGroupColorId initial_color_id = InitColorSet();

  std::unique_ptr<ColorPickerView> color_selector =
      std::make_unique<ColorPickerView>(
          this, colors_, initial_color_id,
          base::BindRepeating(&TabGroupEditorBubbleView::UpdateGroup,
                              base::Unretained(this)));

  color_selector->SetProperty(views::kMarginsKey,
                              gfx::Insets::VH(0, horizontal_spacing));

  return color_selector;
}

std::unique_ptr<TabGroupEditorBubbleView::TitleField>
TabGroupEditorBubbleView::BuildTitleField(const std::u16string& title) {
  std::unique_ptr<TitleField> title_field =
      std::make_unique<TitleField>(stop_context_menu_propagation_);
  title_field->SetText(title);
  title_field->GetViewAccessibility().SetName(l10n_util::GetStringUTF16(
      IDS_TAB_GROUP_HEADER_CXMENU_TAB_GROUP_TITLE_ACCESSIBLE_NAME));
  title_field->SetPlaceholderText(
      l10n_util::GetStringUTF16(IDS_TAB_GROUP_HEADER_BUBBLE_TITLE_PLACEHOLDER));
  title_field->set_controller(&title_field_controller_);
  title_field->SetProperty(views::kElementIdentifierKey,
                           kTabGroupEditorBubbleId);

  const gfx::Insets control_insets = GetControlInsets();
  const int vertical_spacing = control_insets.top();
  const int horizontal_spacing = control_insets.left();

  title_field->SetProperty(
      views::kMarginsKey,
      gfx::Insets::VH(vertical_spacing, horizontal_spacing));

  return title_field;
}

std::u16string TabGroupEditorBubbleView::GetGroupTitle() {
  return browser_->tab_strip_model()->SupportsTabGroups()
             ? browser_->tab_strip_model()
                   ->group_model()
                   ->GetTabGroup(group_)
                   ->visual_data()
                   ->title()
             : std::u16string();
}

std::unique_ptr<views::LabelButton>
TabGroupEditorBubbleView::BuildNewTabInGroupButton() {
  std::unique_ptr<views::LabelButton> menu_item = CreateMenuItem(
      TAB_GROUP_HEADER_CXMENU_NEW_TAB_IN_GROUP,
      l10n_util::GetStringUTF16(IDS_TAB_GROUP_HEADER_CXMENU_NEW_TAB_IN_GROUP),
      base::BindRepeating(&TabGroupEditorBubbleView::NewTabInGroupPressed,
                          base::Unretained(this)),
      ui::ImageModel::FromVectorIcon(kNewTabInGroupRefreshIcon,
                                     ui::kColorMenuIcon, kDefaultIconSize),
      GetAcceleratorText(IDC_ADD_NEW_TAB_TO_GROUP, browser_));

  menu_item->SetProperty(views::kElementIdentifierKey,
                         kTabGroupEditorBubbleNewTabInGroupButtonId);
  return menu_item;
}

std::unique_ptr<views::LabelButton>
TabGroupEditorBubbleView::BuildUngroupButton() {
  return CreateMenuItem(
      TAB_GROUP_HEADER_CXMENU_UNGROUP,
      l10n_util::GetStringUTF16(IDS_TAB_GROUP_HEADER_CXMENU_UNGROUP),
      base::BindRepeating(&TabGroupEditorBubbleView::UngroupPressed,
                          base::Unretained(this)),
      ui::ImageModel::FromVectorIcon(kUngroupRefreshIcon));
}

std::unique_ptr<views::LabelButton>
TabGroupEditorBubbleView::BuildCloseGroupButton() {
  std::unique_ptr<views::LabelButton> menu_item = CreateMenuItem(
      TAB_GROUP_HEADER_CXMENU_CLOSE_GROUP, GetTextForCloseButton(),
      base::BindRepeating(&TabGroupEditorBubbleView::CloseGroupPressed,
                          base::Unretained(this)),
      ui::ImageModel::FromVectorIcon(kCloseGroupRefreshIcon, ui::kColorMenuIcon,
                                     kDefaultIconSize),
      GetAcceleratorText(IDC_CLOSE_TAB_GROUP, browser_));

  menu_item->SetProperty(views::kElementIdentifierKey,
                         kTabGroupEditorBubbleCloseGroupButtonId);
  return menu_item;
}

std::unique_ptr<views::LabelButton>
TabGroupEditorBubbleView::BuildDeleteGroupButton() {
  std::unique_ptr<views::LabelButton> delete_group_menu_item = CreateMenuItem(
      TAB_GROUP_HEADER_CXMENU_DELETE_GROUP,
      l10n_util::GetStringUTF16(IDS_TAB_GROUP_HEADER_CXMENU_DELETE_GROUP),
      base::BindRepeating(&TabGroupEditorBubbleView::DeleteGroupPressed,
                          base::Unretained(this)),
      ui::ImageModel::FromVectorIcon(kTrashCanRefreshIcon, ui::kColorMenuIcon,
                                     kDefaultIconSize));

  delete_group_menu_item->SetProperty(views::kElementIdentifierKey,
                                      kTabGroupEditorBubbleDeleteGroupButtonId);
  if (ShouldShowSavedFooter()) {
    delete_group_menu_item->SetProperty(
        views::kMarginsKey, gfx::Insets::TLBR(0, 0, kSeparatorPadding, 0));
  }

  return delete_group_menu_item;
}

std::unique_ptr<views::LabelButton>
TabGroupEditorBubbleView::BuildLeaveGroupButton() {
  std::unique_ptr<views::LabelButton> leave_group_menu_item = CreateMenuItem(
      TAB_GROUP_HEADER_CXMENU_LEAVE_GROUP,
      l10n_util::GetStringUTF16(IDS_DATA_SHARING_LEAVE_GROUP),
      base::BindRepeating(&TabGroupEditorBubbleView::LeaveGroupPressed,
                          base::Unretained(this)),
      ui::ImageModel::FromVectorIcon(kTrashCanRefreshIcon, ui::kColorMenuIcon,
                                     kDefaultIconSize));

  leave_group_menu_item->SetProperty(views::kElementIdentifierKey,
                                     kTabGroupEditorBubbleLeaveGroupButtonId);
  if (ShouldShowSavedFooter()) {
    leave_group_menu_item->SetProperty(
        views::kMarginsKey, gfx::Insets::TLBR(0, 0, kSeparatorPadding, 0));
  }

  return leave_group_menu_item;
}

std::unique_ptr<views::LabelButton>
TabGroupEditorBubbleView::BuildMoveGroupToNewWindowButton() {
  return CreateMenuItem(
      TAB_GROUP_HEADER_CXMENU_MOVE_GROUP_TO_NEW_WINDOW,
      l10n_util::GetStringUTF16(
          IDS_TAB_GROUP_HEADER_CXMENU_MOVE_GROUP_TO_NEW_WINDOW),
      base::BindRepeating(
          &TabGroupEditorBubbleView::MoveGroupToNewWindowPressed,
          base::Unretained(this)),
      ui::ImageModel::FromVectorIcon(kMoveGroupToNewWindowRefreshIcon,
                                     ui::kColorMenuIcon, kDefaultIconSize));
}

std::unique_ptr<views::LabelButton>
TabGroupEditorBubbleView::BuildShareGroupButton() {
  auto menu_item = CreateMenuItem(
      TAB_GROUP_HEADER_CXMENU_SHARE,
      l10n_util::GetStringUTF16(IDS_TAB_GROUP_HEADER_CXMENU_SHARE_GROUP),
      base::BindRepeating(&TabGroupEditorBubbleView::ShareOrManagePressed,
                          base::Unretained(this)),
      ui::ImageModel::FromVectorIcon(kTabGroupSharingIcon));
  menu_item->SetProperty(views::kElementIdentifierKey,
                         kTabGroupEditorBubbleShareGroupButtonId);
  return menu_item;
}

std::unique_ptr<views::LabelButton>
TabGroupEditorBubbleView::BuildRecentActivityButton() {
  auto menu_item = CreateMenuItem(
      TAB_GROUP_HEADER_CXMENU_RECENT_ACTIVITY,
      l10n_util::GetStringUTF16(IDS_TAB_GROUP_HEADER_CXMENU_RECENT_ACTIVITY),
      base::BindRepeating(&TabGroupEditorBubbleView::RecentActivityPressed,
                          base::Unretained(this)),
      ui::ImageModel::FromVectorIcon(kHistoryIcon, ui::kColorMenuIcon,
                                     kDefaultIconSize));
  menu_item->SetProperty(views::kElementIdentifierKey,
                         kTabGroupEditorBubbleRecentActivityButtonId);
  return menu_item;
}

TabGroupEditorBubbleView::~TabGroupEditorBubbleView() = default;

tab_groups::TabGroupColorId TabGroupEditorBubbleView::InitColorSet() {
  colors_.clear();
  const tab_groups::ColorLabelMap& color_map =
      tab_groups::GetTabGroupColorLabelMap();

  // TODO(tluk) remove the reliance on the ordering of the color pairs in the
  // vector and use the ColorLabelMap structure instead.
  std::ranges::copy(color_map, std::back_inserter(colors_));

  // Keep track of the current group's color, to be returned as the initial
  // selected value.
  auto* const group_model = browser_->tab_strip_model()->group_model();
  return group_model->GetTabGroup(group_)->visual_data()->color();
}

// TabStripModelObserver:
void TabGroupEditorBubbleView::OnTabGroupChanged(const TabGroupChange& change) {
  if (change.group != group_ ||
      change.type != TabGroupChange::kVisualsChanged ||
      !change.GetVisualsChange()->new_visuals) {
    return;
  }

  const tab_groups::TabGroupVisualData* new_visuals =
      change.GetVisualsChange()->new_visuals;

  const std::optional<int> selected_color =
      color_selector_->GetSelectedElement();

  const bool color_changed =
      !selected_color.has_value() ||
      static_cast<int>(colors_.size()) <= selected_color.value() ||
      colors_[selected_color.value()].first != new_visuals->color();

  if (color_changed) {
    const std::optional<size_t> index = GetIndexOf(color_selector_);
    CHECK(index.has_value());
    RemoveChildViewT<ColorPickerView>(color_selector_);
    color_selector_ = AddChildViewAt(BuildColorPicker(), index.value());
  }

  const bool text_changed = title_field_->GetText() != new_visuals->title();

  if (text_changed) {
    title_field_->SetText(new_visuals->title());
  }
}

void TabGroupEditorBubbleView::UpdateGroup() {
  const std::optional<int> selected_element =
      color_selector_->GetSelectedElement();
  TabGroup* const tab_group =
      browser_->tab_strip_model()->group_model()->GetTabGroup(group_);

  const tab_groups::TabGroupVisualData* current_visual_data =
      tab_group->visual_data();
  const tab_groups::TabGroupColorId updated_color =
      selected_element.has_value() ? colors_[selected_element.value()].first
                                   : current_visual_data->color();

  if (current_visual_data->color() != updated_color) {
    base::RecordAction(
        base::UserMetricsAction("TabGroups_TabGroupBubble_ColorChanged"));
  }

  // If the old close button is being displayed, then update it's text.
  views::LabelButton* const close_or_delete_button =
      views::AsViewClass<views::LabelButton>(
          GetViewByID(TAB_GROUP_HEADER_CXMENU_CLOSE_GROUP));
  if (close_or_delete_button) {
    close_or_delete_button->SetText(GetTextForCloseButton());
  }

  tab_groups::TabGroupVisualData new_data(
      std::u16string(title_field_->GetText()), updated_color,
      current_visual_data->is_collapsed());
  browser_->tab_strip_model()->ChangeTabGroupVisuals(group_, new_data,
                                                     tab_group->IsCustomized());
}

const std::u16string TabGroupEditorBubbleView::GetTextForCloseButton() const {
  tab_groups::TabGroupSyncService* tab_group_service =
      tab_groups::SavedTabGroupUtils::GetServiceForProfile(browser_->profile());

  if (!tab_group_service) {
    return l10n_util::GetStringUTF16(IDS_TAB_GROUP_HEADER_CXMENU_DELETE_GROUP);
  }

  // The UI updates now just name this "Close group" instead of "Delete Group"
  // Since delete group is separate if the group is saved.
  return l10n_util::GetStringUTF16(IDS_TAB_GROUP_HEADER_CXMENU_CLOSE_GROUP);
}

bool TabGroupEditorBubbleView::CanSaveGroups() const {
  return browser_->profile()->IsRegularProfile() &&
         tab_groups::SavedTabGroupUtils::GetServiceForProfile(
             browser_->profile());
}

bool TabGroupEditorBubbleView::CanShareGroups() const {
  return tab_groups::SavedTabGroupUtils::SupportsSharedTabGroups() &&
         CanSaveGroups();
}

bool TabGroupEditorBubbleView::IsAllowedToCreateSharedGroup() const {
  auto* collaboration_service =
      collaboration::CollaborationServiceFactory::GetForProfile(
          browser_->profile());
  return collaboration_service->GetServiceStatus().IsAllowedToCreate();
}

bool TabGroupEditorBubbleView::OwnsGroup() const {
  tab_groups::TabGroupSyncService* tab_group_service =
      tab_groups::SavedTabGroupUtils::GetServiceForProfile(browser_->profile());
  if (!tab_group_service) {
    return true;
  }

  const std::optional<tab_groups::SavedTabGroup> maybe_saved_group =
      tab_group_service->GetGroup(group_);
  if (!maybe_saved_group.has_value()) {
    return true;
  }

  return tab_groups::SavedTabGroupUtils::IsOwnerOfSharedTabGroup(
      browser_->profile(), maybe_saved_group->saved_guid());
}

bool TabGroupEditorBubbleView::IsGroupSaved() const {
  tab_groups::TabGroupSyncService* tab_group_service =
      tab_groups::SavedTabGroupUtils::GetServiceForProfile(browser_->profile());

  if (!tab_group_service) {
    return false;
  }

  const std::optional<tab_groups::SavedTabGroup> maybe_saved_group =
      tab_group_service->GetGroup(group_);
  if (!maybe_saved_group.has_value()) {
    return false;
  }

  return !maybe_saved_group.value().is_shared_tab_group();
}

bool TabGroupEditorBubbleView::IsGroupShared() const {
  tab_groups::TabGroupSyncService* tab_group_service =
      tab_groups::SavedTabGroupUtils::GetServiceForProfile(browser_->profile());

  if (!tab_group_service) {
    return false;
  }

  const std::optional<tab_groups::SavedTabGroup> maybe_saved_group =
      tab_group_service->GetGroup(group_);
  if (!maybe_saved_group.has_value()) {
    return false;
  }

  return maybe_saved_group.value().is_shared_tab_group();
}

bool TabGroupEditorBubbleView::ShouldShowSavedFooter() const {
  PrefService* pref_service = browser_->profile()->GetPrefs();
  return (CanSaveGroups() && pref_service &&
          saved_tab_group_prefs::GetLearnMoreFooterShownCount(pref_service) <
              kFooterDisplayLimit);
}

void TabGroupEditorBubbleView::NewTabInGroupPressed() {
  base::RecordAction(
      base::UserMetricsAction("TabGroups_TabGroupBubble_NewTabInGroup"));
  TabStripModel* const model = browser_->tab_strip_model();
  const auto tabs = model->group_model()->GetTabGroup(group_)->ListTabs();
  model->delegate()->AddTabAt(GURL(), tabs.end(), true, group_);
  // Close the widget to allow users to continue their work in their newly
  // created tab.
  GetWidget()->Close();
}

void TabGroupEditorBubbleView::UngroupPressed() {
  base::RecordAction(
      base::UserMetricsAction("TabGroups_TabGroupBubble_Ungroup"));
  tab_groups::TabGroupSyncService* tab_group_service =
      tab_groups::SavedTabGroupUtils::GetServiceForProfile(browser_->profile());

  if (tab_group_service) {
    const std::optional<tab_groups::SavedTabGroup> saved_group =
        tab_group_service->GetGroup(group_);
    if (saved_group.has_value()) {
      tab_groups::SavedTabGroupUtils::UngroupSavedGroup(
          browser_, saved_group->saved_guid());
    } else {
      Ungroup(browser_, group_);
    }
  } else {
    Ungroup(browser_, group_);
  }
  GetWidget()->Close();
}

void TabGroupEditorBubbleView::ShareOrManagePressed() {
  collaboration::CollaborationService* service =
      collaboration::CollaborationServiceFactory::GetForProfile(
          browser_->profile());
  auto delegate = std::make_unique<CollaborationControllerDelegateDesktop>(
      const_cast<Browser*>(browser_.get()));
  service->StartShareOrManageFlow(
      std::move(delegate), group_,
      collaboration::CollaborationServiceShareOrManageEntryPoint::
          kDesktopGroupEditorShareOrManageButton);

  bool is_group_shared = IsGroupShared();
  if (!is_group_shared) {
    shared_tab_group_metrics::RecordSharedTabGroupManageType(
        shared_tab_group_metrics::SharedTabGroupManageTypeDesktop::kShareGroup);
  } else {
    shared_tab_group_metrics::RecordSharedTabGroupManageType(
        shared_tab_group_metrics::SharedTabGroupManageTypeDesktop::
            kManageGroup);
  }
}

void TabGroupEditorBubbleView::RecentActivityPressed() {
  auto* tab_group_header =
      BrowserView::GetBrowserViewForBrowser(browser_)->tabstrip()->group_header(
          group_);

  RecentActivityBubbleCoordinator bubble_coordinator;
  bubble_coordinator.Show(tab_group_header,
                          browser_->tab_strip_model()->GetActiveWebContents(),
                          tab_groups::SavedTabGroupUtils::GetRecentActivity(
                              browser_->profile(), group_),
                          browser_->profile());
}

// static
void TabGroupEditorBubbleView::Ungroup(const Browser* browser,
                                       tab_groups::TabGroupId group) {
  TabStripModel* const model = browser->tab_strip_model();
  const gfx::Range tab_range =
      model->group_model()->GetTabGroup(group)->ListTabs();

  std::vector<int> tabs;
  tabs.reserve(tab_range.length());
  for (auto i = tab_range.start(); i < tab_range.end(); ++i) {
    tabs.push_back(i);
  }

  model->RemoveFromGroup(tabs);
}

void TabGroupEditorBubbleView::CloseGroupPressed() {
  base::RecordAction(
      base::UserMetricsAction("TabGroups_TabGroupBubble_CloseGroup"));

  bool is_group_shared = IsGroupShared();

  DeleteGroupFromTabstrip();
  GetWidget()->Close();

  if (is_group_shared) {
    shared_tab_group_metrics::RecordSharedTabGroupRecallType(
        shared_tab_group_metrics::SharedTabGroupRecallTypeDesktop::kClosed);
  }
}

void TabGroupEditorBubbleView::DeleteGroupPressed() {
  base::RecordAction(
      base::UserMetricsAction("TabGroups_TabGroupBubble_DeleteGroup"));
  tab_groups::TabGroupSyncService* tab_group_service =
      tab_groups::SavedTabGroupUtils::GetServiceForProfile(browser_->profile());
  if (!tab_group_service) {
    return CloseGroupPressed();
  }

  std::optional<tab_groups::SavedTabGroup> saved_group =
      tab_group_service->GetGroup(group_);
  if (!saved_group) {
    return CloseGroupPressed();
  }

  bool is_group_shared = saved_group->is_shared_tab_group();
  tab_groups::SavedTabGroupUtils::DeleteSavedGroup(browser_,
                                                   saved_group->saved_guid());
  if (is_group_shared) {
    shared_tab_group_metrics::RecordSharedTabGroupManageType(
        shared_tab_group_metrics::SharedTabGroupManageTypeDesktop::
            kDeleteGroup);
  }

  GetWidget()->Close();
}

void TabGroupEditorBubbleView::LeaveGroupPressed() {
  tab_groups::TabGroupSyncService* tab_group_service =
      tab_groups::SavedTabGroupUtils::GetServiceForProfile(browser_->profile());
  if (!tab_group_service) {
    return;
  }

  std::optional<tab_groups::SavedTabGroup> saved_group =
      tab_group_service->GetGroup(group_);
  if (!saved_group) {
    return;
  }

  tab_groups::SavedTabGroupUtils::LeaveSharedGroup(browser_,
                                                   saved_group->saved_guid());
  GetWidget()->Close();
}

void TabGroupEditorBubbleView::DeleteGroupFromTabstrip() {
  TabStripModel* const model = browser_->tab_strip_model();
  const int num_tabs_in_group =
      model->group_model()->GetTabGroup(group_)->tab_count();
  if (model->count() == num_tabs_in_group) {
    // If the group about to be closed has all of the tabs in the browser, add a
    // new tab outside the group to prevent the browser from closing.
    model->delegate()->AddTabAt(GURL(), -1, true);
  }

  model->CloseAllTabsInGroup(group_);
}

void TabGroupEditorBubbleView::MoveGroupToNewWindowPressed() {
  browser_->tab_strip_model()->delegate()->MoveGroupToNewWindow(group_);
  GetWidget()->Close();
}

bool TabGroupEditorBubbleView::CanMoveGroupToNewWindow() {
  return browser_->tab_strip_model()->count() != browser_->tab_strip_model()
                                                     ->group_model()
                                                     ->GetTabGroup(group_)
                                                     ->tab_count();
}

std::unique_ptr<ManageSharingRow>
TabGroupEditorBubbleView::BuildManageSharingButton() {
  tab_groups::TabGroupSyncService* tab_group_service =
      tab_groups::SavedTabGroupUtils::GetServiceForProfile(browser_->profile());

  std::optional<tab_groups::SavedTabGroup> saved_group =
      tab_group_service->GetGroup(group_);
  CHECK(saved_group.has_value());
  CHECK(saved_group->collaboration_id().has_value());
  return std::make_unique<ManageSharingRow>(
      browser_->profile(), saved_group->collaboration_id().value(),
      base::BindRepeating(&TabGroupEditorBubbleView::ShareOrManagePressed,
                          base::Unretained(this)));
}

void TabGroupEditorBubbleView::OnBubbleClose() {
  if (title_at_opening_ != title_field_->GetText()) {
    base::RecordAction(
        base::UserMetricsAction("TabGroups_TabGroupBubble_NameChanged"));
  }

  if (browser_->tab_strip_model()->group_model()->ContainsTabGroup(group_)) {
    const int tab_count = browser_->tab_strip_model()
                              ->group_model()
                              ->GetTabGroup(group_)
                              ->tab_count();
    if (tab_count > 0) {
      base::UmaHistogramCounts100("TabGroups.TabGroupBubble.TabCount",
                                  tab_count);
    }
  }
}

BEGIN_METADATA(TabGroupEditorBubbleView)
END_METADATA

void TabGroupEditorBubbleView::TitleFieldController::ContentsChanged(
    views::Textfield* sender,
    const std::u16string& new_contents) {
  DCHECK_EQ(sender, parent_->title_field_);
  parent_->UpdateGroup();
}

bool TabGroupEditorBubbleView::TitleFieldController::HandleKeyEvent(
    views::Textfield* sender,
    const ui::KeyEvent& key_event) {
  DCHECK_EQ(sender, parent_->title_field_);

  // For special actions, only respond to key pressed events, to be consistent
  // with other views like buttons and dialogs.
  if (key_event.type() == ui::EventType::kKeyPressed) {
    const ui::KeyboardCode key_code = key_event.key_code();
    if (key_code == ui::VKEY_ESCAPE) {
      parent_->GetWidget()->CloseWithReason(
          views::Widget::ClosedReason::kEscKeyPressed);
      return true;
    }
    if (key_code == ui::VKEY_RETURN) {
      parent_->GetWidget()->CloseWithReason(
          views::Widget::ClosedReason::kUnspecified);
      return true;
    }
  }

  return false;
}

void TabGroupEditorBubbleView::TitleField::ShowContextMenu(
    const gfx::Point& p,
    ui::mojom::MenuSourceType source_type) {
  // There is no easy way to stop the propagation of a ShowContextMenu event,
  // which is sometimes used to open the bubble itself. So when the bubble is
  // opened this way, we manually hide the textfield's context menu the first
  // time. Otherwise, the textfield, which is automatically focused, would show
  // an extra context menu when the bubble first opens.
  if (stop_context_menu_propagation_) {
    stop_context_menu_propagation_ = false;
    return;
  }
  views::Textfield::ShowContextMenu(p, source_type);
}

BEGIN_METADATA(TabGroupEditorBubbleView, TitleField)
END_METADATA

TabGroupEditorBubbleView::Footer::Footer(const Browser* browser) {
  views::FlexLayout* flex_layout =
      views::View::SetLayoutManager(std::make_unique<views::FlexLayout>());
  flex_layout->SetOrientation(views::LayoutOrientation::kVertical)
      .SetMainAxisAlignment(views::LayoutAlignment::kStart)
      .SetCollapseMargins(true);

  SetBackground(views::CreateSolidBackground(ui::kColorBubbleFooterBackground));

  // Get the keyed service and check if saved.
  views::StyledLabel* footer_label =
      AddChildView(std::make_unique<views::StyledLabel>());
  footer_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  std::vector<std::u16string> footer_text_substr;

  // Strings for the footer are different if the user has sync enabled.
  footer_text_substr.push_back(l10n_util::GetStringUTF16(
      tab_groups::SavedTabGroupUtils::AreSavedTabGroupsSyncedForProfile(
          browser->profile())
          ? IDS_TAB_GROUP_EDITOR_BUBBLE_FOOTER_SYNC_ENABLED
          : IDS_TAB_GROUP_EDITOR_BUBBLE_FOOTER_SYNC_DISABLED));

  // Learn more link for the footer.
  footer_text_substr.push_back(
      l10n_util::GetStringUTF16(IDS_TAB_GROUP_EDITOR_BUBBLE_FOOTER_LEARN_MORE));

  std::vector<size_t> offsets;
  std::u16string styled_text =
      base::ReplaceStringPlaceholders(u"$1 $2", footer_text_substr, &offsets);
  footer_label->SetText(styled_text);
  footer_label->SetDefaultEnabledColorId(ui::kColorLabelForegroundSecondary);

  gfx::Range details_range(offsets[1], styled_text.length());

  views::StyledLabel::RangeStyleInfo link_style =
      views::StyledLabel::RangeStyleInfo::CreateForLink(base::BindRepeating(
          &TabGroupEditorBubbleView::Footer::OpenLearnMorePage, browser));

  footer_label->AddStyleRange(details_range, link_style);

  ChromeLayoutProvider* layout_provider = ChromeLayoutProvider::Get();
  const int horizontal_spacing = layout_provider->GetDistanceMetric(
      views::DISTANCE_RELATED_CONTROL_HORIZONTAL);
  const int vertical_spacing = layout_provider->GetDistanceMetric(
      views::DISTANCE_RELATED_CONTROL_VERTICAL);
  const gfx::Insets control_insets =
      ui::TouchUiController::Get()->touch_ui()
          ? gfx::Insets::VH(5 * vertical_spacing / 4, horizontal_spacing)
          : gfx::Insets::VH(2 * vertical_spacing, horizontal_spacing);
  footer_label->SizeToFit(kDialogWidth - control_insets.right() -
                          control_insets.left());
  SetSize({kDialogWidth, height()});
  SetBorder(views::CreateEmptyBorder(control_insets));
}

// static
void TabGroupEditorBubbleView::Footer::OpenLearnMorePage(
    const Browser* browser) {
  browser->tab_strip_model()->delegate()->AddTabAt(
      GURL(chrome::kTabGroupsLearnMoreURL), -1, true);
}

BEGIN_METADATA(TabGroupEditorBubbleView, Footer)
END_METADATA
