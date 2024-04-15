// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_group_editor_bubble_view.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

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
#include "base/ranges/algorithm.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_keyed_service.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_service_factory.h"
#include "chrome/browser/ui/tabs/tab_group.h"
#include "chrome/browser/ui/tabs/tab_group_deletion_dialog_controller.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_delegate.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/bubble_menu_item_factory.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/controls/hover_button.h"
#include "chrome/browser/ui/views/tabs/color_picker_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_ink_drop_util.h"
#include "chrome/browser/user_education/user_education_service.h"
#include "chrome/browser/user_education/user_education_service_factory.h"
#include "chrome/grit/generated_resources.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/saved_tab_groups/features.h"
#include "components/tab_groups/tab_group_color.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/dialog_model_field.h"
#include "ui/base/models/image_model.h"
#include "ui/base/pointer/touch_ui_controller.h"
#include "ui/base/ui_base_features.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/range/range.h"
#include "ui/gfx/text_constants.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/toggle_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/separator.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_utils.h"

namespace {

constexpr base::TimeDelta kTemporaryBookmarkBarDuration = base::Seconds(15);

std::unique_ptr<views::LabelButton> CreateMenuItem(
    int button_id,
    const std::u16string& name,
    views::Button::PressedCallback callback,
    const ui::ImageModel& icon = ui::ImageModel()) {
  const auto* layout_provider = ChromeLayoutProvider::Get();
  const int horizontal_spacing = layout_provider->GetDistanceMetric(
      views::DISTANCE_RELATED_CONTROL_HORIZONTAL);
  const int vertical_spacing = layout_provider->GetDistanceMetric(
      views::DISTANCE_RELATED_CONTROL_VERTICAL);

  const gfx::Insets control_insets =
      ui::TouchUiController::Get()->touch_ui()
          ? gfx::Insets::VH(5 * vertical_spacing / 4, horizontal_spacing)
          : gfx::Insets::VH(vertical_spacing, horizontal_spacing);

  auto button =
      CreateBubbleMenuItem(button_id, name, std::move(callback), icon);
  button->SetBorder(views::CreateEmptyBorder(control_insets));
  if (features::IsChromeRefresh2023()) {
    button->SetLabelStyle(views::style::STYLE_BODY_3_EMPHASIS);
  }

  return button;
}

}  // namespace

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

  // If |header_view| is not null, use |header_view| as the |anchor_view|.
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
  tab_group_editor_bubble_view->SizeToContents();
  widget->Show();
  return widget;
}

views::View* TabGroupEditorBubbleView::GetInitiallyFocusedView() {
  return title_field_;
}

gfx::Rect TabGroupEditorBubbleView::GetAnchorRect() const {
  // We want to avoid calling BubbleDialogDelegateView::GetAnchorRect() if
  // |anchor_rect_| has been set. This is because the default behavior uses the
  // anchor view's bounds and also updates |anchor_rect_| to the views bounds.
  // It does this so that the bubble does not jump when the anchoring view is
  // deleted.
  if (use_set_anchor_rect_) {
    return anchor_rect().value();
  }
  return BubbleDialogDelegateView::GetAnchorRect();
}

void TabGroupEditorBubbleView::AddedToWidget() {
  const auto* const color_provider = GetColorProvider();

  for (views::LabelButton* menu_item : menu_items_) {
    const bool enabled = menu_item->GetEnabled();
    views::Button::ButtonState button_state =
        enabled ? views::Button::STATE_NORMAL : views::Button::STATE_DISABLED;

    const SkColor text_color = menu_item->GetCurrentTextColor();

    const SkColor enabled_icon_color =
        features::IsChromeRefresh2023()
            ? color_provider->GetColor(kColorTabGroupDialogIconEnabled)
            : color_utils::DeriveDefaultIconColor(text_color);
    const SkColor icon_color = enabled ? enabled_icon_color : text_color;

    const std::optional<ui::ImageModel>& old_image_model =
        menu_item->GetImageModel(button_state);
    if (old_image_model.has_value() && !old_image_model->IsEmpty() &&
        old_image_model->IsVectorIcon()) {
      ui::VectorIconModel vector_icon_model = old_image_model->GetVectorIcon();
      const gfx::VectorIcon* icon = vector_icon_model.vector_icon();
      const ui::ImageModel new_image_model =
          ui::ImageModel::FromVectorIcon(*icon, icon_color);
      menu_item->SetImageModel(button_state, new_image_model);
    }
  }

  if (save_group_icon_) {
    DCHECK(save_group_label_);

    const bool enabled = save_group_icon_->GetEnabled();
    const SkColor text_color = save_group_label_->GetEnabledColor();
    const SkColor enabled_icon_color =
        features::IsChromeRefresh2023()
            ? color_provider->GetColor(kColorTabGroupDialogIconEnabled)
            : color_utils::DeriveDefaultIconColor(text_color);
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
    : browser_(browser),
      group_(group),
      title_field_controller_(this),
      use_set_anchor_rect_(anchor_rect) {
  // |anchor_view| should always be defined as it will be used to source the
  // |anchor_widget_|.
  DCHECK(anchor_view);
  SetAnchorView(anchor_view);
  if (anchor_rect) {
    SetAnchorRect(anchor_rect.value());
  }

  set_margins(gfx::Insets());

  SetButtons(ui::DIALOG_BUTTON_NONE);
  SetModalType(ui::MODAL_TYPE_NONE);

  TabStripModel* const tab_strip_model = browser_->tab_strip_model();
  DCHECK(tab_strip_model->group_model());

  const std::u16string title = tab_strip_model->group_model()
                                   ->GetTabGroup(group_)
                                   ->visual_data()
                                   ->title();
  title_at_opening_ = title;
  SetCloseCallback(base::BindOnce(&TabGroupEditorBubbleView::OnBubbleClose,
                                  base::Unretained(this)));

  std::unique_ptr<views::LabelButton> move_menu_item = CreateMenuItem(
      TAB_GROUP_HEADER_CXMENU_MOVE_GROUP_TO_NEW_WINDOW,
      l10n_util::GetStringUTF16(
          IDS_TAB_GROUP_HEADER_CXMENU_MOVE_GROUP_TO_NEW_WINDOW),
      base::BindRepeating(
          &TabGroupEditorBubbleView::MoveGroupToNewWindowPressed,
          base::Unretained(this)),
      ui::ImageModel::FromVectorIcon(features::IsChromeRefresh2023()
                                         ? kMoveGroupToNewWindowRefreshIcon
                                         : kMoveGroupToNewWindowIcon));

  // Create view hierarchy.
  title_field_ =
      AddChildView(std::make_unique<TitleField>(stop_context_menu_propagation));
  title_field_->SetText(title);
  title_field_->SetAccessibleName(l10n_util::GetStringUTF16(
      IDS_TAB_GROUP_HEADER_CXMENU_TAB_GROUP_TITLE_ACCESSIBLE_NAME));
  title_field_->SetPlaceholderText(
      l10n_util::GetStringUTF16(IDS_TAB_GROUP_HEADER_BUBBLE_TITLE_PLACEHOLDER));
  title_field_->set_controller(&title_field_controller_);
  title_field_->SetProperty(views::kElementIdentifierKey,
                            kTabGroupEditorBubbleId);

  const tab_groups::TabGroupColorId initial_color_id = InitColorSet();
  color_selector_ = AddChildView(std::make_unique<ColorPickerView>(
      this, colors_, initial_color_id,
      base::BindRepeating(&TabGroupEditorBubbleView::UpdateGroup,
                          base::Unretained(this))));

  auto* const visual_data_separator =
      AddChildView(std::make_unique<views::Separator>());

  views::View* save_group_line_container = nullptr;

  if (browser_->profile()->IsRegularProfile()) {
    save_group_line_container = AddChildView(std::make_unique<views::View>());

    // The `save_group_icon_` is put in differently than the rest because it
    // utilizes a different view (view::Label) that does not have an option to
    // take in an image like the other line items do.
    save_group_icon_ = save_group_line_container->AddChildView(
        std::make_unique<views::ImageView>(ui::ImageModel::FromVectorIcon(
            features::IsChromeRefresh2023() ? kSaveGroupRefreshIcon
                                            : kSaveGroupIcon)));

    save_group_label_ =
        save_group_line_container->AddChildView(std::make_unique<views::Label>(
            l10n_util::GetStringUTF16(IDS_TAB_GROUP_HEADER_CXMENU_SAVE_GROUP)));
    save_group_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    if (features::IsChromeRefresh2023()) {
      save_group_label_->SetTextStyle(views::style::STYLE_BODY_3_EMPHASIS);
    }

    save_group_toggle_ = save_group_line_container->AddChildView(
        std::make_unique<views::ToggleButton>(
            base::BindRepeating(&TabGroupEditorBubbleView::OnSaveTogglePressed,
                                base::Unretained(this))));
    save_group_toggle_->SetID(TAB_GROUP_HEADER_CXMENU_SAVE_GROUP);

    const tab_groups::SavedTabGroupKeyedService* const saved_tab_group_service =
        tab_groups::SavedTabGroupServiceFactory::GetForProfile(
            browser_->profile());
    CHECK(saved_tab_group_service);

    save_group_toggle_->SetIsOn(
        saved_tab_group_service->model()->Contains(group_));
    save_group_toggle_->SetAccessibleName(GetSaveToggleAccessibleName());
    save_group_toggle_->SetProperty(views::kElementIdentifierKey,
                                    kTabGroupEditorBubbleSaveToggleId);
  }

  views::LabelButton* const new_tab_menu_item = AddChildView(CreateMenuItem(
      TAB_GROUP_HEADER_CXMENU_NEW_TAB_IN_GROUP,
      l10n_util::GetStringUTF16(IDS_TAB_GROUP_HEADER_CXMENU_NEW_TAB_IN_GROUP),
      base::BindRepeating(&TabGroupEditorBubbleView::NewTabInGroupPressed,
                          base::Unretained(this)),
      ui::ImageModel::FromVectorIcon(features::IsChromeRefresh2023()
                                         ? kNewTabInGroupRefreshIcon
                                         : kNewTabInGroupIcon)));
  menu_items_.push_back(new_tab_menu_item);

  views::LabelButton* move_menu_item_ptr;
  if (tab_groups::IsTabGroupsSaveV2Enabled()) {
    move_menu_item_ptr = AddChildView(std::move(move_menu_item));
  }

  menu_items_.push_back(AddChildView(CreateMenuItem(
      TAB_GROUP_HEADER_CXMENU_UNGROUP,
      l10n_util::GetStringUTF16(IDS_TAB_GROUP_HEADER_CXMENU_UNGROUP),
      base::BindRepeating(&TabGroupEditorBubbleView::UngroupPressed,
                          base::Unretained(this)),
      ui::ImageModel::FromVectorIcon(features::IsChromeRefresh2023()
                                         ? kUngroupRefreshIcon
                                         : kUngroupIcon))));

  views::LabelButton* close_group_menu_item = AddChildView(CreateMenuItem(
      TAB_GROUP_HEADER_CXMENU_CLOSE_GROUP, GetTextForCloseButton(),
      base::BindRepeating(&TabGroupEditorBubbleView::CloseGroupPressed,
                          base::Unretained(this)),
      ui::ImageModel::FromVectorIcon(features::IsChromeRefresh2023()
                                         ? kCloseGroupRefreshIcon
                                         : kCloseGroupIcon)));
  close_group_menu_item->SetProperty(views::kElementIdentifierKey,
                                     kTabGroupEditorBubbleCloseGroupButtonId);
  menu_items_.push_back(close_group_menu_item);

  if (!tab_groups::IsTabGroupsSaveV2Enabled()) {
    // The move menu item must not be added to the menu by this point.
    CHECK(move_menu_item);
    move_menu_item_ptr = AddChildView(std::move(move_menu_item));
  }

  // The move menu item must be added to the menu by this point.
  CHECK(!move_menu_item);
  move_menu_item_ptr->SetEnabled(
      tab_strip_model->count() !=
      tab_strip_model->group_model()->GetTabGroup(group_)->tab_count());
  menu_items_.push_back(move_menu_item_ptr);

  // Setting up the layout.
  const gfx::Insets control_insets = new_tab_menu_item->GetInsets();
  const int vertical_spacing = control_insets.top();
  const int horizontal_spacing = control_insets.left();

  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical)
      .SetInteriorMargin(gfx::Insets::VH(vertical_spacing, 0));

  title_field_->SetProperty(
      views::kMarginsKey,
      gfx::Insets::VH(vertical_spacing, horizontal_spacing));

  color_selector_->SetProperty(views::kMarginsKey,
                               gfx::Insets::VH(0, horizontal_spacing));

  visual_data_separator->SetProperty(views::kMarginsKey,
                                     gfx::Insets::VH(vertical_spacing, 0));

  if (save_group_line_container) {
    gfx::Insets save_group_margins = control_insets;
    const int label_height = new_tab_menu_item->GetPreferredSize().height();
    const int control_height =
        std::max(save_group_label_
                     ->GetPreferredSize(
                         views::SizeBounds(save_group_label_->width(), {}))
                     .height(),
                 save_group_toggle_->GetPreferredSize().height());
    save_group_margins.set_top((label_height - control_height) / 2);
    save_group_margins.set_bottom(save_group_margins.top());

    save_group_icon_->SetProperty(
        views::kMarginsKey,
        gfx::Insets::TLBR(0, 0, 0, new_tab_menu_item->GetImageLabelSpacing()));

    save_group_line_container
        ->SetLayoutManager(std::make_unique<views::FlexLayout>())
        ->SetOrientation(views::LayoutOrientation::kHorizontal)
        .SetCrossAxisAlignment(views::LayoutAlignment::kCenter)
        .SetInteriorMargin(save_group_margins);

    save_group_label_->SetProperty(
        views::kFlexBehaviorKey,
        views::FlexSpecification(views::MinimumFlexSizeRule::kPreferred,
                                 views::MaximumFlexSizeRule::kUnbounded));
  }
}

TabGroupEditorBubbleView::~TabGroupEditorBubbleView() = default;

tab_groups::TabGroupColorId TabGroupEditorBubbleView::InitColorSet() {
  const tab_groups::ColorLabelMap& color_map =
      tab_groups::GetTabGroupColorLabelMap();

  // TODO(tluk) remove the reliance on the ordering of the color pairs in the
  // vector and use the ColorLabelMap structure instead.
  base::ranges::copy(color_map, std::back_inserter(colors_));

  // Keep track of the current group's color, to be returned as the initial
  // selected value.
  auto* const group_model = browser_->tab_strip_model()->group_model();
  return group_model->GetTabGroup(group_)->visual_data()->color();
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

  views::LabelButton* const close_or_delete_button =
      views::AsViewClass<views::LabelButton>(
          GetViewByID(TAB_GROUP_HEADER_CXMENU_CLOSE_GROUP));
  CHECK(close_or_delete_button);
  close_or_delete_button->SetText(GetTextForCloseButton());

  tab_groups::TabGroupVisualData new_data(title_field_->GetText(),
                                          updated_color,
                                          current_visual_data->is_collapsed());
  tab_group->SetVisualData(new_data, tab_group->IsCustomized());
}

const std::u16string TabGroupEditorBubbleView::GetTextForCloseButton() {
  tab_groups::SavedTabGroupKeyedService* const saved_tab_group_service =
      tab_groups::SavedTabGroupServiceFactory::GetForProfile(
          browser_->profile());

  if (!saved_tab_group_service) {
    return l10n_util::GetStringUTF16(IDS_TAB_GROUP_HEADER_CXMENU_DELETE_GROUP);
  }

  // The UI updates now just name this "Close group" instead of "Delete Group"
  // Since delete group is separate if the group is saved.
  if (tab_groups::IsTabGroupsSaveV2Enabled()) {
    return l10n_util::GetStringUTF16(IDS_TAB_GROUP_HEADER_CXMENU_CLOSE_GROUP);
  } else {
    return saved_tab_group_service->model()->Contains(group_)
               ? l10n_util::GetStringUTF16(
                     IDS_TAB_GROUP_HEADER_CXMENU_HIDE_GROUP)
               : l10n_util::GetStringUTF16(
                     IDS_TAB_GROUP_HEADER_CXMENU_DELETE_GROUP);
  }
}

const std::u16string TabGroupEditorBubbleView::GetSaveToggleAccessibleName() {
  return l10n_util::GetStringUTF16(
      save_group_toggle_->GetIsOn() ? IDS_TAB_GROUP_HEADER_CXMENU_UNSAVE_GROUP
                                    : IDS_TAB_GROUP_HEADER_CXMENU_SAVE_GROUP);
}

void TabGroupEditorBubbleView::OnSaveTogglePressed() {
  tab_groups::SavedTabGroupKeyedService* const saved_tab_group_service =
      tab_groups::SavedTabGroupServiceFactory::GetForProfile(
          browser_->profile());
  CHECK(saved_tab_group_service);

  if (save_group_toggle_->GetIsOn()) {
    base::RecordAction(
        base::UserMetricsAction("TabGroups_TabGroupBubble_GroupSaved"));

    saved_tab_group_service->SaveGroup(
        group_,
        /*is_pinned=*/tab_groups::IsTabGroupsSaveUIUpdateEnabled());

    views::ElementTrackerViews::GetInstance()->NotifyCustomEvent(
        kTabGroupSavedCustomEventId, save_group_toggle_);

    auto* const service =
        UserEducationServiceFactory::GetForBrowserContext(browser_->profile());
    if (service && !service->tutorial_service().IsRunningTutorial(
                       kSavedTabGroupTutorialId)) {
      browser_->window()->TemporarilyShowBookmarkBar(
          kTemporaryBookmarkBarDuration);
    }
  } else {
    base::RecordAction(
        base::UserMetricsAction("TabGroups_TabGroupBubble_GroupUnsaved"));
    saved_tab_group_service->UnsaveGroup(group_);
  }

  save_group_toggle_->SetAccessibleName(GetSaveToggleAccessibleName());
  UpdateGroup();
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
  if (tab_groups::IsTabGroupsSaveUIUpdateEnabled() &&
      save_group_toggle_->GetIsOn()) {
    browser_->tab_group_deletion_dialog_controller()->MaybeShowDialog(
        tab_groups::DeletionDialogController::DialogType::UngroupSingle,
        base::BindOnce(&TabGroupEditorBubbleView::Ungroup, browser_, group_));
  } else {
    Ungroup(browser_, group_);
  }
  GetWidget()->Close();
}

// static
void TabGroupEditorBubbleView::Ungroup(const Browser* browser,
                                       tab_groups::TabGroupId group) {
  base::RecordAction(
      base::UserMetricsAction("TabGroups_TabGroupBubble_Ungroup"));

  tab_groups::SavedTabGroupKeyedService* saved_tab_group_service =
      tab_groups::SavedTabGroupServiceFactory::GetForProfile(
          browser->profile());
  if (saved_tab_group_service) {
    saved_tab_group_service->DisconnectLocalTabGroup(group);
  }

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
  TabStripModel* const model = browser_->tab_strip_model();
  const int num_tabs_in_group =
      model->group_model()->GetTabGroup(group_)->tab_count();
  if (model->count() == num_tabs_in_group) {
    // If the group about to be closed has all of the tabs in the browser, add a
    // new tab outside the group to prevent the browser from closing.
    model->delegate()->AddTabAt(GURL(), -1, true);
  }

  model->CloseAllTabsInGroup(group_);
  // Close the widget because it is no longer applicable.
  GetWidget()->Close();
}

void TabGroupEditorBubbleView::MoveGroupToNewWindowPressed() {
  browser_->tab_strip_model()->delegate()->MoveGroupToNewWindow(group_);
  GetWidget()->CloseWithReason(views::Widget::ClosedReason::kUnspecified);
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
  if (key_event.type() == ui::EventType::ET_KEY_PRESSED) {
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
    ui::MenuSourceType source_type) {
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
