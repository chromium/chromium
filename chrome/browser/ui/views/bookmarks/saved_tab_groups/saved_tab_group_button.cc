// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bookmarks/saved_tab_groups/saved_tab_group_button.h"

#include <memory>
#include <string>
#include <vector>

#include "base/check.h"
#include "base/cxx20_to_address.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "cc/paint/paint_flags.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/favicon/favicon_utils.h"
#include "chrome/browser/ui/bookmarks/bookmark_utils_desktop.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_keyed_service.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_service_factory.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_utils.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_group_theme.h"
#include "chrome/browser/ui/tabs/tab_strip_model_delegate.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_button_util.h"
#include "chrome/browser/ui/views/bookmarks/saved_tab_groups/saved_tab_group_drag_data.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/toolbar/toolbar_ink_drop_util.h"
#include "chrome/grit/generated_resources.h"
#include "components/saved_tab_groups/saved_tab_group.h"
#include "components/tab_groups/tab_group_id.h"
#include "content/public/browser/page_navigator.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/dialog_model.h"
#include "ui/base/models/dialog_model_field.h"
#include "ui/base/models/dialog_model_menu_model_adapter.h"
#include "ui/base/models/image_model.h"
#include "ui/base/theme_provider.h"
#include "ui/base/ui_base_types.h"
#include "ui/color/color_id.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/label_button_border.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_utils.h"

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(SavedTabGroupButton,
                                      kDeleteGroupMenuItem);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(SavedTabGroupButton,
                                      kMoveGroupToNewWindowMenuItem);

namespace {
// The max height of the button and the max width of a button with no title.
constexpr int kButtonSize = 24;
// The corner radius for the button.
constexpr float kButtonRadius = 4.0f;
// The amount of insets from the buttons border.
constexpr float kInsets = 5.0f;
// The width of the outline of the button when open in the Tab Strip.
constexpr float kBorderThickness = 1.0f;
// The radius for the circle that is displayed for buttons with no title.
constexpr float kCircleRadius = 7.0f;
}  // namespace

SavedTabGroupButton::SavedTabGroupButton(
    const SavedTabGroup& group,
    base::RepeatingCallback<content::PageNavigator*()> page_navigator,
    PressedCallback callback,
    Browser* browser,
    bool animations_enabled)
    : MenuButton(std::move(callback), group.title()),
      tab_group_color_id_(group.color()),
      guid_(group.saved_guid()),
      local_group_id_(group.local_group_id()),
      tabs_(group.saved_tabs()),
      browser_(*browser),
      service_(
          *SavedTabGroupServiceFactory::GetForProfile(browser_->profile())),
      page_navigator_callback_(std::move(page_navigator)),
      context_menu_controller_(
          this,
          base::BindRepeating(
              &SavedTabGroupButton::CreateDialogModelForContextMenu,
              base::Unretained(this)),
          views::MenuRunner::CONTEXT_MENU | views::MenuRunner::IS_NESTED) {
  SetAccessibilityProperties(
      ax::mojom::Role::kPopUpButton, group.title(),
      /*description*/ absl::nullopt,
      l10n_util::GetStringUTF16(
          IDS_ACCNAME_SAVED_TAB_GROUP_BUTTON_ROLE_DESCRIPTION));
  SetText(group.title());
  SetTooltipText(group.title());
  SetID(VIEW_ID_BOOKMARK_BAR_ELEMENT);
  SetProperty(views::kElementIdentifierKey, kSavedTabGroupButtonElementId);
  SetMaxSize(gfx::Size(bookmark_button_util::kMaxButtonWidth, kButtonSize));

  show_animation_ = std::make_unique<gfx::SlideAnimation>(this);
  if (!animations_enabled) {
    // For some reason during testing the events generated by animating
    // throw off the test. So, don't animate while testing.
    show_animation_->Reset(1);
  } else {
    show_animation_->Show();
  }

  ConfigureInkDropForToolbar(this);
  SetImageLabelSpacing(ChromeLayoutProvider::Get()->GetDistanceMetric(
      ChromeDistanceMetric::DISTANCE_RELATED_LABEL_HORIZONTAL_LIST));
  views::InstallRoundRectHighlightPathGenerator(this, gfx::Insets(0),
                                                kButtonRadius);

  set_drag_controller(this);
}

SavedTabGroupButton::~SavedTabGroupButton() = default;

void SavedTabGroupButton::UpdateButtonData(const SavedTabGroup& group) {
  SetText(group.title());
  SetTooltipText(group.title());
  SetAccessibleName(group.title());
  tab_group_color_id_ = group.color();
  local_group_id_ = group.local_group_id();
  guid_ = group.saved_guid();
  tabs_.clear();
  tabs_ = group.saved_tabs();

  UpdateButtonLayout();
}

std::u16string SavedTabGroupButton::GetTooltipText(const gfx::Point& p) const {
  return label()->GetPreferredSize().width() > label()->size().width()
             ? GetText()
             : std::u16string();
}

void SavedTabGroupButton::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  views::MenuButton::GetAccessibleNodeData(node_data);

  // TODO(crbug.com/1411342): Under what circumstances would there be no
  // name? Please read the bug description and update accordingly.
  // If the button would have no name, avoid crashing by setting the name
  // explicitly empty.
  if (GetAccessibleName().empty()) {
    node_data->SetNameExplicitlyEmpty();
  }
}

void SavedTabGroupButton::PaintButtonContents(gfx::Canvas* canvas) {
  if (!GetText().empty()) {
    return;
  }

  // When the title is empty, we draw a circle similar to the tab group
  // header when there is no title.
  const ui::ColorProvider* const cp = GetColorProvider();
  SkColor text_and_outline_color =
      cp->GetColor(GetTabGroupDialogColorId(tab_group_color_id_));

  // Draw circle.
  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setStyle(cc::PaintFlags::kFill_Style);
  flags.setColor(text_and_outline_color);

  const gfx::PointF center_point_f = gfx::PointF(width() / 2, height() / 2);
  canvas->DrawCircle(center_point_f, kCircleRadius, flags);
}

void SavedTabGroupButton::UpdateButtonLayout() {
  if (GetText().empty()) {
    // When the text is empty force the button to have square dimensions.
    SetPreferredSize(gfx::Size(kButtonSize, kButtonSize));
  } else {
    SetPreferredSize(CalculatePreferredSize());
  }

  // Relies on logic in theme_helper.cc to determine dark/light palette.
  ui::ColorId text_and_outline_color =
      GetTabGroupDialogColorId(tab_group_color_id_);
  ui::ColorId background_color =
      GetTabGroupBookmarkColorId(tab_group_color_id_);

  SetEnabledTextColorIds(GetTabGroupDialogColorId(tab_group_color_id_));
  SetBackground(views::CreateThemedRoundedRectBackground(background_color,
                                                         kButtonRadius));

  // Only draw a border if the group is open in the tab strip.
  if (!local_group_id_.has_value()) {
    SetBorder(views::CreateEmptyBorder(gfx::Insets(kInsets)));
  } else {
    std::unique_ptr<views::Border> border =
        views::CreateThemedRoundedRectBorder(kBorderThickness, kButtonRadius,
                                             text_and_outline_color);
    SetBorder(
        views::CreatePaddedBorder(std::move(border), gfx::Insets(kInsets)));
  }
}

std::unique_ptr<views::LabelButtonBorder>
SavedTabGroupButton::CreateDefaultBorder() const {
  auto border = std::make_unique<views::LabelButtonBorder>();
  border->set_insets(ChromeLayoutProvider::Get()->GetInsetsMetric(
      INSETS_BOOKMARKS_BAR_BUTTON));
  return border;
}

void SavedTabGroupButton::OnThemeChanged() {
  views::MenuButton::OnThemeChanged();
  UpdateButtonLayout();
}

void SavedTabGroupButton::WriteDragDataForView(View* sender,
                                               const gfx::Point& press_pt,
                                               ui::OSExchangeData* data) {
  SavedTabGroupButton* const button =
      views::AsViewClass<SavedTabGroupButton>(sender);
  CHECK(button);
  CHECK(button == this);

  // Write the image and MIME type to the OSExchangeData.
  SavedTabGroupDragData::WriteToOSExchangeData(this, press_pt,
                                               GetThemeProvider(), data);
}

int SavedTabGroupButton::GetDragOperationsForView(View* sender,
                                                  const gfx::Point& p) {
  // This may need to become more complicated
  return ui::DragDropTypes::DRAG_MOVE;
}

bool SavedTabGroupButton::CanStartDragForView(View* sender,
                                              const gfx::Point& press_pt,
                                              const gfx::Point& p) {
  // Check if we have not moved enough horizontally but we have moved downward
  // vertically - downward drag.
  gfx::Vector2d move_offset = p - press_pt;
  return View::ExceededDragThreshold(move_offset);
}

void SavedTabGroupButton::TabMenuItemPressed(const GURL& url, int event_flags) {
  CHECK(page_navigator_callback_);

  content::OpenURLParams params(url, content::Referrer(),
                                WindowOpenDisposition::NEW_FOREGROUND_TAB,
                                ui::PAGE_TRANSITION_AUTO_BOOKMARK,
                                /*is_renderer_initiated=*/false,
                                /*started_from_context_menu=*/true);
  page_navigator_callback_.Run()->OpenURL(params);
}

void SavedTabGroupButton::MoveGroupToNewWindowPressed(int event_flags) {
  Browser* const browser_with_local_group_id =
      local_group_id_.has_value()
          ? SavedTabGroupUtils::GetBrowserWithTabGroupId(
                local_group_id_.value())
          : base::to_address(browser_);

  if (!local_group_id_.has_value()) {
    // Open the group in the browser the button was pressed.
    service_->OpenSavedTabGroupInBrowser(browser_with_local_group_id, guid_);
  }

  // Move the open group to a new browser window.
  const SavedTabGroup* group = service_->model()->Get(guid_);
  browser_with_local_group_id->tab_strip_model()
      ->delegate()
      ->MoveGroupToNewWindow(group->local_group_id().value());
}

void SavedTabGroupButton::DeleteGroupPressed(int event_flags) {
  if (local_group_id_.has_value()) {
    const Browser* const browser_with_local_group_id =
        SavedTabGroupUtils::GetBrowserWithTabGroupId(local_group_id_.value());

    // Keep the opened tab group in the tabstrip but remove the SavedTabGroup
    // data from the model.
    TabGroup* tab_group = browser_with_local_group_id->tab_strip_model()
                              ->group_model()
                              ->GetTabGroup(local_group_id_.value());

    service_->UnsaveGroup(local_group_id_.value());

    // Notify observers to update the tab group header.
    // TODO(dljames): Find a way to move this into
    // SavedTabGroupKeyedService::DisconnectLocalTabGroup. The goal is to
    // abstract this logic from the button in case we need to do similar
    // functionality elsewhere in the future. Ensure this change works when
    // dragging a Saved group out of the window.
    tab_group->SetVisualData(*tab_group->visual_data());

  } else {
    // Remove the SavedTabGroup from the model. No need to worry about updating
    // tabstrip, since this group is not open.
    service_->model()->Remove(guid_);
  }
}

std::unique_ptr<ui::DialogModel>
SavedTabGroupButton::CreateDialogModelForContextMenu() {
  ui::DialogModel::Builder dialog_model = ui::DialogModel::Builder();

  const std::u16string move_or_open_group_text =
      local_group_id_.has_value()
          ? l10n_util::GetStringUTF16(
                IDS_TAB_GROUP_HEADER_CXMENU_MOVE_GROUP_TO_NEW_WINDOW)
          : l10n_util::GetStringUTF16(
                IDS_TAB_GROUP_HEADER_CXMENU_OPEN_GROUP_IN_NEW_WINDOW);

  dialog_model
      .AddMenuItem(
          ui::ImageModel::FromVectorIcon(kMoveGroupToNewWindowIcon),
          move_or_open_group_text,
          base::BindRepeating(&SavedTabGroupButton::MoveGroupToNewWindowPressed,
                              base::Unretained(this)),
          ui::DialogModelMenuItem::Params().SetId(
              kMoveGroupToNewWindowMenuItem))
      .AddMenuItem(
          ui::ImageModel::FromVectorIcon(kCloseGroupIcon),
          l10n_util::GetStringUTF16(IDS_TAB_GROUP_HEADER_CXMENU_DELETE_GROUP),
          base::BindRepeating(&SavedTabGroupButton::DeleteGroupPressed,
                              base::Unretained(this)),
          ui::DialogModelMenuItem::Params().SetId(kDeleteGroupMenuItem))
      .AddSeparator();

  for (const SavedTabGroupTab& tab : tabs_) {
    const ui::ImageModel& image =
        tab.favicon().has_value()
            ? ui::ImageModel::FromImage(tab.favicon().value())
            : favicon::GetDefaultFaviconModel(
                  GetTabGroupBookmarkColorId(tab_group_color_id_));
    const std::u16string title =
        tab.title().empty() ? base::UTF8ToUTF16(tab.url().spec()) : tab.title();
    dialog_model.AddMenuItem(
        image, title,
        base::BindRepeating(&SavedTabGroupButton::TabMenuItemPressed,
                            base::Unretained(this), tab.url()));
  }

  return dialog_model.Build();
}

BEGIN_METADATA(SavedTabGroupButton, MenuButton)
END_METADATA
