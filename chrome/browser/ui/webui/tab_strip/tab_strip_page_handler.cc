// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/tab_strip/tab_strip_page_handler.h"

#include <algorithm>
#include <memory>
#include <optional>

#include "base/containers/fixed_flat_map.h"
#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/trace_event/trace_event.h"
#include "build/chromeos_buildflags.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/favicon/favicon_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/tabs/tab_group.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_group_theme.h"
#include "chrome/browser/ui/tabs/tab_menu_model.h"
#include "chrome/browser/ui/tabs/tab_renderer_data.h"
#include "chrome/browser/ui/tabs/tab_utils.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/webui/tab_strip/tab_strip_ui.h"
#include "chrome/browser/ui/webui/tab_strip/tab_strip_ui_embedder.h"
#include "chrome/browser/ui/webui/tab_strip/tab_strip_ui_metrics.h"
#include "chrome/browser/ui/webui/tab_strip/tab_strip_ui_util.h"
#include "chrome/browser/ui/webui/theme_source.h"
#include "chrome/browser/ui/webui/util/image_util.h"
#include "chrome/browser/ui/webui/webui_util_desktop.h"
#include "chrome/grit/generated_resources.h"
#include "components/tab_groups/tab_group_color.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "content/public/common/drop_data.h"
#include "third_party/blink/public/common/input/web_gesture_event.h"
#include "ui/aura/window_delegate.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/list_selection_model.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/base/theme_provider.h"
#include "ui/color/color_id.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/events/gesture_event_details.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/range/range.h"
#include "url/gurl.h"

// This should be after all other #includes.
#if defined(_WINDOWS_)  // Detect whether windows.h was included.
#include "base/win/windows_h_disallowed.h"
#endif  // defined(_WINDOWS_)

namespace {

// Delay in milliseconds of when the dragging UI should be shown for touch drag.
// Note: For better user experience, this is made shorter than
// EventType::kGestureLongPress delay, which is too long for this case, e.g.,
// about 650ms.
constexpr base::TimeDelta kTouchLongpressDelay = base::Milliseconds(300);

class WebUIBackgroundMenuModel : public ui::SimpleMenuModel {
 public:
  explicit WebUIBackgroundMenuModel(ui::SimpleMenuModel::Delegate* delegate)
      : ui::SimpleMenuModel(delegate) {
    AddItemWithStringId(IDC_NEW_TAB, IDS_NEW_TAB);
    AddItemWithStringId(IDC_RESTORE_TAB, IDS_RESTORE_TAB);
    AddItemWithStringId(IDC_BOOKMARK_ALL_TABS, IDS_BOOKMARK_ALL_TABS);
  }
};

class WebUIBackgroundContextMenu : public ui::SimpleMenuModel::Delegate,
                                   public WebUIBackgroundMenuModel {
 public:
  WebUIBackgroundContextMenu(
      Browser* browser,
      const ui::AcceleratorProvider* accelerator_provider)
      : WebUIBackgroundMenuModel(this),
        browser_(browser),
        accelerator_provider_(accelerator_provider) {}
  ~WebUIBackgroundContextMenu() override = default;

  void ExecuteCommand(int command_id, int event_flags) override {
    chrome::ExecuteCommand(browser_, command_id);
  }

  bool GetAcceleratorForCommandId(int command_id,
                                  ui::Accelerator* accelerator) const override {
    return accelerator_provider_->GetAcceleratorForCommandId(command_id,
                                                             accelerator);
  }

 private:
  const raw_ptr<Browser> browser_;
  const raw_ptr<const ui::AcceleratorProvider> accelerator_provider_;
};

class WebUITabContextMenu : public ui::SimpleMenuModel::Delegate,
                            public TabMenuModel {
 public:
  WebUITabContextMenu(Browser* browser,
                      const ui::AcceleratorProvider* accelerator_provider,
                      int tab_index)
      : TabMenuModel(this,
                     browser->tab_menu_model_delegate(),
                     browser->tab_strip_model(),
                     tab_index),
        browser_(browser),
        accelerator_provider_(accelerator_provider),
        tab_index_(tab_index) {}
  ~WebUITabContextMenu() override = default;

  void ExecuteCommand(int command_id, int event_flags) override {
    DCHECK_LT(tab_index_, browser_->tab_strip_model()->count());
    browser_->tab_strip_model()->ExecuteContextMenuCommand(
        tab_index_, static_cast<TabStripModel::ContextMenuCommand>(command_id));
  }

  bool GetAcceleratorForCommandId(int command_id,
                                  ui::Accelerator* accelerator) const override {
    int real_command = -1;
    TabStripModel::ContextMenuCommandToBrowserCommand(command_id,
                                                      &real_command);

    if (real_command != -1) {
      return accelerator_provider_->GetAcceleratorForCommandId(real_command,
                                                               accelerator);
    } else {
      return false;
    }
  }

 private:
  const raw_ptr<Browser> browser_;
  const raw_ptr<const ui::AcceleratorProvider> accelerator_provider_;
  const int tab_index_;
};

}  // namespace

TabStripPageHandler::~TabStripPageHandler() {
  ThemeServiceFactory::GetForProfile(browser_->profile())->RemoveObserver(this);
  theme_observation_.Reset();
}

TabStripPageHandler::TabStripPageHandler(
    mojo::PendingReceiver<tab_strip::mojom::PageHandler> receiver,
    mojo::PendingRemote<tab_strip::mojom::Page> page,
    content::WebUI* web_ui,
    Browser* browser,
    TabStripUIEmbedder* embedder)
    : receiver_(this, std::move(receiver)),
      page_(std::move(page)),
      web_ui_(web_ui),
      browser_(browser),
      embedder_(embedder),
      thumbnail_tracker_(
          base::BindRepeating(&TabStripPageHandler::HandleThumbnailUpdate,
                              base::Unretained(this))),
      tab_before_unload_tracker_(
          base::BindRepeating(&TabStripPageHandler::OnTabCloseCancelled,
                              base::Unretained(this))),
      context_menu_after_tap_(base::FeatureList::IsEnabled(
          features::kWebUITabStripContextMenuAfterTap)),
      long_press_timer_(std::make_unique<base::RetainingOneShotTimer>(
          FROM_HERE,
          kTouchLongpressDelay,
          base::BindRepeating(&TabStripPageHandler::OnLongPressTimer,
                              base::Unretained(this)))) {
  DCHECK(browser_);
  DCHECK(embedder_);
  web_ui_->GetWebContents()->SetDelegate(this);
  browser_->tab_strip_model()->AddObserver(this);

  // Listen for theme installation.
  ThemeServiceFactory::GetForProfile(browser_->profile())->AddObserver(this);

  // Or native theme change.
  theme_observation_.Observe(
      webui::GetNativeThemeDeprecated(web_ui_->GetWebContents()));
}

void TabStripPageHandler::NotifyLayoutChanged() {
  TRACE_EVENT0("browser", "TabStripPageHandler:NotifyLayoutChanged");
  page_->LayoutChanged(embedder_->GetLayout().AsDictionary());
}

void TabStripPageHandler::NotifyReceivedKeyboardFocus() {
  page_->ReceivedKeyboardFocus();
}

void TabStripPageHandler::NotifyContextMenuClosed() {
  page_->ContextMenuClosed();
}

// TabStripModelObserver:
void TabStripPageHandler::OnTabGroupChanged(const TabGroupChange& change) {
  TRACE_EVENT0("browser", "TabStripPageHandler:OnTabGroupChanged");
  switch (change.type) {
    case TabGroupChange::kCreated:
    case TabGroupChange::kEditorOpened:
    case TabGroupChange::kContentsChanged: {
      // TabGroupChange::kCreated events are unnecessary as the front-end will
      // assume a group was created if there is a tab-group-state-changed event
      // with a new group ID. TabGroupChange::kContentsChanged events are
      // handled by TabGroupStateChanged.
      break;
    }

    case TabGroupChange::kVisualsChanged: {
      TabGroupModel* group_model = browser_->tab_strip_model()->group_model();
      if (group_model) {
        page_->TabGroupVisualsChanged(
            change.group.ToString(),
            GetTabGroupData(group_model->GetTabGroup(change.group)));
      }
      break;
    }

    case TabGroupChange::kMoved: {
      DCHECK(browser_->tab_strip_model()->SupportsTabGroups());
      TabGroupModel* group_model = browser_->tab_strip_model()->group_model();
      const int start_tab =
          group_model->GetTabGroup(change.group)->ListTabs().start();
      page_->TabGroupMoved(change.group.ToString(), start_tab);
      break;
    }

    case TabGroupChange::kClosed: {
      embedder_->HideEditDialogForGroup();
      page_->TabGroupClosed(change.group.ToString());
      break;
    }
  }
}

void TabStripPageHandler::TabGroupedStateChanged(
    std::optional<tab_groups::TabGroupId> group,
    tabs::TabModel* tab,
    int index) {
  TRACE_EVENT0("browser", "TabStripPageHandler:TabGroupedStateChanged");
  const SessionID::id_type tab_id =
      extensions::ExtensionTabUtil::GetTabId(tab->contents());
  if (group.has_value()) {
    page_->TabGroupStateChanged(tab_id, index, group.value().ToString());
  } else {
    page_->TabGroupStateChanged(tab_id, index, std::optional<std::string>());
  }
}

void TabStripPageHandler::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  TRACE_EVENT0("browser", "TabStripPageHandler:OnTabStripModelChanged");
  if (tab_strip_model->empty())
    return;

  // The context menu model is created when the menu is first shown. However, if
  // the tab strip model changes, the context menu model may not longer reflect
  // the current state of the tab strip. Actions then taken from the context
  // menu may leave the tab strip in an inconsistent state, or result in DCHECK
  // crashes. To ensure this does not occur close the context menu on a tab
  // strip model change.
  embedder_->CloseContextMenu();

  switch (change.type()) {
    case TabStripModelChange::kInserted: {
      for (const auto& contents : change.GetInsert()->contents) {
        page_->TabCreated(GetTabData(contents.contents, contents.index));
      }
      break;
    }
    case TabStripModelChange::kRemoved: {
      for (const auto& contents : change.GetRemove()->contents) {
        page_->TabRemoved(
            extensions::ExtensionTabUtil::GetTabId(contents.contents));
      }
      break;
    }
    case TabStripModelChange::kMoved: {
      auto* move = change.GetMove();
      page_->TabMoved(extensions::ExtensionTabUtil::GetTabId(move->contents),
                      move->to_index,
                      tab_strip_model->IsTabPinned(move->to_index));
      break;
    }
    case TabStripModelChange::kReplaced: {
      auto* replace = change.GetReplace();
      page_->TabReplaced(
          extensions::ExtensionTabUtil::GetTabId(replace->old_contents),
          extensions::ExtensionTabUtil::GetTabId(replace->new_contents));
      break;
    }
    case TabStripModelChange::kSelectionOnly:
      // Multi-selection is not supported for touch.
      break;
  }

  if (selection.active_tab_changed()) {
    content::WebContents* new_contents = selection.new_contents;
    if (new_contents && selection.new_model.active().has_value()) {
      page_->TabActiveChanged(
          extensions::ExtensionTabUtil::GetTabId(new_contents));
    }
  }
}

void TabStripPageHandler::TabChangedAt(content::WebContents* contents,
                                       int index,
                                       TabChangeType change_type) {
  TRACE_EVENT0("browser", "TabStripPageHandler:TabChangedAt");
  page_->TabUpdated(GetTabData(contents, index));
}

void TabStripPageHandler::TabPinnedStateChanged(TabStripModel* tab_strip_model,
                                                content::WebContents* contents,
                                                int index) {
  page_->TabUpdated(GetTabData(contents, index));
}

void TabStripPageHandler::TabBlockedStateChanged(content::WebContents* contents,
                                                 int index) {
  page_->TabUpdated(GetTabData(contents, index));
}

bool TabStripPageHandler::PreHandleGestureEvent(
    content::WebContents* source,
    const blink::WebGestureEvent& event) {
  switch (event.GetType()) {
    case blink::WebInputEvent::Type::kGestureScrollBegin:
      // Drag and drop for the WebUI tab strip is currently only supported for
      // Aura platforms.
#if defined(USE_AURA)
      // If we are passed the `kTouchLongpressDelay` threshold since the initial
      // tap down initiate a drag on scroll start.
      if (should_drag_on_gesture_scroll_ && !long_press_timer_->IsRunning()) {
        handling_gesture_scroll_ = true;

        // If we are about to start a drag ensure the context menu is closed.
        embedder_->CloseContextMenu();

        // Synthesize a long press event to start the drag and drop session.
        // TODO(tluk): Replace this with a better drag and drop trigger when
        // available.
        ui::GestureEventDetails press_details(ui::EventType::kGestureLongPress);
        press_details.set_device_type(
            ui::GestureDeviceType::DEVICE_TOUCHSCREEN);
        ui::GestureEvent press_event(
            touch_drag_start_point_.x(), touch_drag_start_point_.y(),
            ui::EF_IS_SYNTHESIZED, base::TimeTicks::Now(), press_details);

        auto* window = web_ui_->GetWebContents()->GetContentNativeView();
        window->delegate()->OnGestureEvent(&press_event);

        // Following the long press we need to dispatch a scroll end event to
        // ensure the gesture stream is not left in an inconsistent state.
        ui::GestureEventDetails scroll_end_details(
            ui::EventType::kGestureScrollEnd);
        scroll_end_details.set_device_type(
            ui::GestureDeviceType::DEVICE_TOUCHSCREEN);
        ui::GestureEvent scroll_end_event(
            touch_drag_start_point_.x(), touch_drag_start_point_.y(),
            ui::EF_IS_SYNTHESIZED, base::TimeTicks::Now(), scroll_end_details);
        window->delegate()->OnGestureEvent(&scroll_end_event);
        return true;
      }
      long_press_timer_->Stop();
#endif  // defined(USE_AURA)
      return false;
    case blink::WebInputEvent::Type::kGestureScrollEnd:
      should_drag_on_gesture_scroll_ = false;
      handling_gesture_scroll_ = false;
      return false;
    case blink::WebInputEvent::Type::kGestureTapDown:
      // We should only trigger a drag as part of the gesture event stream if
      // the stream begins with a tap down gesture event.
      should_drag_on_gesture_scroll_ = true;
      touch_drag_start_point_ =
          gfx::ToRoundedPoint(event.PositionInRootFrame());
      long_press_timer_->Reset();
      return false;
    case blink::WebInputEvent::Type::kGestureLongPress:
      // Do not block the long press if handling a scroll gesture. This ensures
      // the long press gesture event emitted during a scroll begin event
      // reaches the WebContents and triggers a drag session.
      if (handling_gesture_scroll_) {
        should_drag_on_gesture_scroll_ = false;
        return false;
      }
      if (!context_menu_after_tap_)
        page_->ShowContextMenu();
      return true;
    case blink::WebInputEvent::Type::kGestureTwoFingerTap:
      page_->ShowContextMenu();
      return true;
    case blink::WebInputEvent::Type::kGestureLongTap:
      if (context_menu_after_tap_)
        page_->ShowContextMenu();

      should_drag_on_gesture_scroll_ = false;
      long_press_timer_->Stop();
      return true;
    case blink::WebInputEvent::Type::kGestureTap:
      // Ensure that we reset `should_drag_on_gesture_scroll_` when we encounter
      // a gesture tap event (i.e. an event triggered after the user lifts their
      // finger following a press or long press).
      should_drag_on_gesture_scroll_ = false;
      long_press_timer_->Stop();
      return false;
    default:
      break;
  }
  return false;
}

bool TabStripPageHandler::CanDragEnter(
    content::WebContents* source,
    const content::DropData& data,
    blink::DragOperationsMask operations_allowed) {
  // TODO(crbug.com/40110968): Prevent dragging across Chromium instances.
  if (auto it = data.custom_data.find(kWebUITabIdDataType);
      it != data.custom_data.end()) {
    int tab_id;
    bool found_tab_id = base::StringToInt(it->second, &tab_id);
    return found_tab_id && extensions::ExtensionTabUtil::GetTabById(
                               tab_id, browser_->profile(), false, nullptr);
  }

  if (auto it = data.custom_data.find(kWebUITabGroupIdDataType);
      it != data.custom_data.end()) {
    std::string group_id = base::UTF16ToUTF8(it->second);
    Browser* found_browser = tab_strip_ui::GetBrowserWithGroupId(
        Profile::FromBrowserContext(browser_->profile()), group_id);
    return found_browser != nullptr;
  }

  return false;
}

bool TabStripPageHandler::IsPrivileged() {
  return true;
}

void TabStripPageHandler::OnLongPressTimer() {
  page_->LongPress();
}

tab_strip::mojom::TabPtr TabStripPageHandler::GetTabData(
    content::WebContents* contents,
    int index) {
  DCHECK(index >= 0);
  auto tab_data = tab_strip::mojom::Tab::New();

  tab_data->active = browser_->tab_strip_model()->active_index() == index;
  tab_data->id = extensions::ExtensionTabUtil::GetTabId(contents);
  DCHECK(tab_data->id > 0);
  tab_data->index = index;

  const std::optional<tab_groups::TabGroupId> group_id =
      browser_->tab_strip_model()->GetTabGroupForTab(index);
  if (group_id.has_value()) {
    tab_data->group_id = group_id.value().ToString();
  }

  TabRendererData tab_renderer_data =
      TabRendererData::FromTabInModel(browser_->tab_strip_model(), index);
  tab_data->pinned = tab_renderer_data.pinned;
  tab_data->title = base::UTF16ToUTF8(tab_renderer_data.title);
  tab_data->url = tab_renderer_data.visible_url;

  const ui::ColorProvider& provider =
      web_ui_->GetWebContents()->GetColorProvider();
  const gfx::ImageSkia default_favicon =
      favicon::GetDefaultFaviconModel().Rasterize(&provider);
  const gfx::ImageSkia raster_favicon =
      tab_renderer_data.favicon.Rasterize(&provider);

  if (!tab_renderer_data.favicon.IsEmpty()) {
    // Themified icons only apply to a few select chrome URLs.
    if (tab_renderer_data.should_themify_favicon) {
      tab_data->favicon_url = GURL(
          webui::EncodePNGAndMakeDataURI(ThemeFavicon(raster_favicon, false),
                                         web_ui_->GetDeviceScaleFactor()));
      tab_data->active_favicon_url = GURL(webui::EncodePNGAndMakeDataURI(
          ThemeFavicon(raster_favicon, true), web_ui_->GetDeviceScaleFactor()));
    } else {
      tab_data->favicon_url = GURL(webui::EncodePNGAndMakeDataURI(
          tab_renderer_data.favicon.Rasterize(&provider),
          web_ui_->GetDeviceScaleFactor()));
    }

    tab_data->is_default_favicon =
        raster_favicon.BackedBySameObjectAs(default_favicon);
  } else {
    tab_data->is_default_favicon = true;
  }
  tab_data->show_icon = tab_renderer_data.show_icon;
  tab_data->network_state = tab_renderer_data.network_state;
  tab_data->should_hide_throbber = tab_renderer_data.should_hide_throbber;
  tab_data->blocked = tab_renderer_data.blocked;
  tab_data->crashed = tab_renderer_data.IsCrashed();
  // TODO(johntlee): Add the rest of TabRendererData

  for (const auto alert_state : GetTabAlertStatesForContents(contents)) {
    tab_data->alert_states.push_back(alert_state);
  }

  return tab_data;
}

tab_strip::mojom::TabGroupVisualDataPtr TabStripPageHandler::GetTabGroupData(
    TabGroup* group) {
  const tab_groups::TabGroupVisualData* visual_data = group->visual_data();

  auto tab_group = tab_strip::mojom::TabGroupVisualData::New();
  tab_group->title = base::UTF16ToUTF8(visual_data->title());

  // TODO the tab strip should support toggles between inactive and active frame
  // states. Currently the webui tab strip only uses active frame colors
  // (https://crbug.com/1060398).
  const int group_color_id =
      GetThumbnailTabStripTabGroupColorId(visual_data->color(), true);
  const SkColor group_color = embedder_->GetColorProviderColor(group_color_id);
  tab_group->color = color_utils::SkColorToRgbString(group_color);
  // TODO(tluk): Incorporate the text color into the ColorProvider.
  tab_group->text_color = color_utils::SkColorToRgbString(
      color_utils::GetColorWithMaxContrast(group_color));
  return tab_group;
}

void TabStripPageHandler::GetTabs(GetTabsCallback callback) {
  TRACE_EVENT0("browser", "TabStripPageHandler:HandleGetTabs");
  std::vector<tab_strip::mojom::TabPtr> tabs;
  TabStripModel* tab_strip_model = browser_->tab_strip_model();
  for (int i = 0; i < tab_strip_model->count(); ++i) {
    tabs.push_back(GetTabData(tab_strip_model->GetWebContentsAt(i), i));
  }
  std::move(callback).Run(std::move(tabs));
}

void TabStripPageHandler::GetGroupVisualData(
    GetGroupVisualDataCallback callback) {
  TRACE_EVENT0("browser", "TabStripPageHandler:HandleGetGroupVisualData");
  base::flat_map<std::string, tab_strip::mojom::TabGroupVisualDataPtr>
      group_visual_datas;
  std::vector<tab_groups::TabGroupId> groups =
      browser_->tab_strip_model()->group_model()->ListTabGroups();
  for (const tab_groups::TabGroupId& group : groups) {
    group_visual_datas[group.ToString()] = GetTabGroupData(
        browser_->tab_strip_model()->group_model()->GetTabGroup(group));
  }
  std::move(callback).Run(std::move(group_visual_datas));
}

void TabStripPageHandler::GroupTab(int32_t tab_id,
                                   const std::string& group_id_string) {
  int tab_index = -1;
  if (!extensions::ExtensionTabUtil::GetTabById(
          tab_id, browser_->profile(), /*include_incognito=*/true, nullptr,
          nullptr, nullptr, &tab_index)) {
    return;
  }

  std::optional<tab_groups::TabGroupId> group_id =
      tab_strip_ui::GetTabGroupIdFromString(
          browser_->tab_strip_model()->group_model(), group_id_string);
  if (group_id.has_value()) {
    browser_->tab_strip_model()->AddToExistingGroup({tab_index},
                                                    group_id.value());
  }
}

void TabStripPageHandler::UngroupTab(int32_t tab_id) {
  int tab_index = -1;
  if (!extensions::ExtensionTabUtil::GetTabById(
          tab_id, browser_->profile(), /*include_incognito=*/true, nullptr,
          nullptr, nullptr, &tab_index)) {
    return;
  }

  browser_->tab_strip_model()->RemoveFromGroup({tab_index});
}

void TabStripPageHandler::MoveGroup(const std::string& group_id_string,
                                    int32_t to_index) {
  if (to_index == -1) {
    to_index = browser_->tab_strip_model()->count();
  }

  auto* target_browser = browser_.get();
  Browser* source_browser =
      tab_strip_ui::GetBrowserWithGroupId(browser_->profile(), group_id_string);
  if (!source_browser) {
    return;
  }

  std::optional<tab_groups::TabGroupId> group_id =
      tab_strip_ui::GetTabGroupIdFromString(
          source_browser->tab_strip_model()->group_model(), group_id_string);
  TabGroup* group =
      source_browser->tab_strip_model()->group_model()->GetTabGroup(
          group_id.value());
  const gfx::Range tabs_in_group = group->ListTabs();

  if (source_browser == target_browser) {
    if (static_cast<int>(tabs_in_group.start()) == to_index) {
      // If the group is already in place, don't move it. This may happen
      // if multiple drag events happen while the tab group is still
      // being moved.
      return;
    }

    // When a group is moved, all the tabs in it need to be selected at the same
    // time. This mimics the way the native tab strip works and also allows
    // this handler to ignore the events for each individual tab moving.
    ui::ListSelectionModel group_selection;
    group_selection.SetSelectedIndex(tabs_in_group.start());
    group_selection.SetSelectionFromAnchorTo(tabs_in_group.end() - 1);
    group_selection.set_active(
        target_browser->tab_strip_model()->selection_model().active());
    target_browser->tab_strip_model()->SetSelectionFromModel(group_selection);

    target_browser->tab_strip_model()->MoveGroupTo(group_id.value(), to_index);
    return;
  }

  target_browser->tab_strip_model()->group_model()->AddTabGroup(
      group_id.value(),
      std::optional<tab_groups::TabGroupVisualData>{*group->visual_data()});

  gfx::Range source_tab_indices = group->ListTabs();
  const int tab_count = source_tab_indices.length();
  const int from_index = source_tab_indices.start();
  for (int i = 0; i < tab_count; i++) {
    tab_strip_ui::MoveTabAcrossWindows(source_browser, from_index,
                                       target_browser, to_index + i, group_id);
  }
}

void TabStripPageHandler::MoveTab(int32_t tab_id, int32_t to_index) {
  if (to_index == -1) {
    to_index = browser_->tab_strip_model()->count();
  }

  Browser* source_browser;
  int from_index = -1;
  if (!extensions::ExtensionTabUtil::GetTabById(tab_id, browser_->profile(),
                                                true, &source_browser, nullptr,
                                                nullptr, &from_index)) {
    return;
  }

  if (source_browser->profile() != browser_->profile()) {
    return;
  }

  if (source_browser == browser_) {
    browser_->tab_strip_model()->MoveWebContentsAt(from_index, to_index, false);
    return;
  }

  tab_strip_ui::MoveTabAcrossWindows(source_browser, from_index, browser_,
                                     to_index);
}

void TabStripPageHandler::CloseContainer() {
  // We only autoclose for tab selection.
  RecordTabStripUICloseHistogram(TabStripUICloseAction::kTabSelected);
  DCHECK(embedder_);
  embedder_->CloseContainer();
}

void TabStripPageHandler::CloseTab(int32_t tab_id, bool tab_was_swiped) {
  content::WebContents* tab = nullptr;
  if (!extensions::ExtensionTabUtil::GetTabById(tab_id, browser_->profile(),
                                                true, &tab)) {
    // ID didn't refer to a valid tab.
    DVLOG(1) << "Invalid tab ID";
    return;
  }

  if (tab_was_swiped) {
    // The unload tracker will automatically unobserve the tab when it
    // successfully closes.
    tab_before_unload_tracker_.Observe(tab);
  }
  tab->Close();
}

void TabStripPageHandler::ShowBackgroundContextMenu(int32_t location_x,
                                                    int32_t location_y) {
  gfx::PointF point(location_x, location_y);
  DCHECK(embedder_);
  embedder_->ShowContextMenuAtPoint(
      gfx::ToRoundedPoint(point),
      std::make_unique<WebUIBackgroundContextMenu>(
          browser_, embedder_->GetAcceleratorProvider()),
      base::BindRepeating(&TabStripPageHandler::NotifyContextMenuClosed,
                          weak_ptr_factory_.GetWeakPtr()));
}

void TabStripPageHandler::ShowEditDialogForGroup(
    const std::string& group_id_string,
    int32_t location_x,
    int32_t location_y,
    int32_t width,
    int32_t height) {
  std::optional<tab_groups::TabGroupId> group_id =
      tab_strip_ui::GetTabGroupIdFromString(
          browser_->tab_strip_model()->group_model(), group_id_string);
  if (!group_id.has_value()) {
    return;
  }

  gfx::Point point(location_x, location_y);
  gfx::Rect rect(width, height);
  DCHECK(embedder_);
  embedder_->ShowEditDialogForGroupAtPoint(point, rect, group_id.value());
}

void TabStripPageHandler::ShowTabContextMenu(int32_t tab_id,
                                             int32_t location_x,
                                             int32_t location_y) {
  gfx::PointF point(location_x, location_y);
  Browser* browser = nullptr;
  int tab_index = -1;
  if (!extensions::ExtensionTabUtil::GetTabById(
          tab_id, browser_->profile(), true /* include_incognito */, &browser,
          nullptr, nullptr, &tab_index)) {
    return;
  }

  if (browser != browser_) {
    // TODO(crbug.com/40727240): Investigate how a context menu is being opened
    // for a tab that is no longer in the tab strip. Until then, fire a
    // tab-removed event so the tab is removed from this tab strip.
    page_->TabRemoved(tab_id);
    return;
  }

  DCHECK(embedder_);
  embedder_->ShowContextMenuAtPoint(
      gfx::ToRoundedPoint(point),
      std::make_unique<WebUITabContextMenu>(
          browser, embedder_->GetAcceleratorProvider(), tab_index),
      base::BindRepeating(&TabStripPageHandler::NotifyContextMenuClosed,
                          weak_ptr_factory_.GetWeakPtr()));
  base::UmaHistogramEnumeration("TabStrip.Tab.WebUI.ActivationAction",
                                TabActivationTypes::kContextMenu);
}

void TabStripPageHandler::GetLayout(GetLayoutCallback callback) {
  TRACE_EVENT0("browser", "TabStripPageHandler:HandleGetLayout");
  std::move(callback).Run(std::move(embedder_->GetLayout().AsDictionary()));
}

void TabStripPageHandler::SetThumbnailTracked(int32_t tab_id,
                                              bool thumbnail_tracked) {
  TRACE_EVENT0("browser", "TabStripPageHandler:HandleSetThumbnailTracked");
  content::WebContents* tab = nullptr;
  if (!extensions::ExtensionTabUtil::GetTabById(tab_id, browser_->profile(),
                                                true, &tab)) {
    // ID didn't refer to a valid tab.
    DVLOG(1) << "Invalid tab ID";
    return;
  }

  if (thumbnail_tracked)
    thumbnail_tracker_.AddTab(tab);
  else
    thumbnail_tracker_.RemoveTab(tab);
}

void TabStripPageHandler::ReportTabActivationDuration(uint32_t duration_ms) {
  UMA_HISTOGRAM_TIMES("WebUITabStrip.TabActivation",
                      base::Milliseconds(duration_ms));
  base::UmaHistogramEnumeration("TabStrip.Tab.WebUI.ActivationAction",
                                TabActivationTypes::kTab);
}

void TabStripPageHandler::ReportTabDataReceivedDuration(uint32_t tab_count,
                                                        uint32_t duration_ms) {
  ReportTabDurationHistogram("TabDataReceived", tab_count,
                             base::Milliseconds(duration_ms));
}

void TabStripPageHandler::ReportTabCreationDuration(uint32_t tab_count,
                                                    uint32_t duration_ms) {
  ReportTabDurationHistogram("TabCreation", tab_count,
                             base::Milliseconds(duration_ms));
}

// Callback passed to |thumbnail_tracker_|. Called when a tab's thumbnail
// changes, or when we start watching the tab.
void TabStripPageHandler::HandleThumbnailUpdate(
    content::WebContents* tab,
    ThumbnailTracker::CompressedThumbnailData image) {
  // Send base-64 encoded image to JS side. If |image| is blank (i.e.
  // there is no data), send a blank URI.
  TRACE_EVENT0("browser", "TabStripPageHandler:HandleThumbnailUpdate");
  std::string data_uri;
  if (image)
    data_uri = webui::MakeDataURIForImage(base::make_span(image->data), "jpeg");

  const SessionID::id_type tab_id = extensions::ExtensionTabUtil::GetTabId(tab);
  page_->TabThumbnailUpdated(tab_id, data_uri);
}

void TabStripPageHandler::OnTabCloseCancelled(content::WebContents* tab) {
  tab_before_unload_tracker_.Unobserve(tab);
  const SessionID::id_type tab_id = extensions::ExtensionTabUtil::GetTabId(tab);
  page_->TabCloseCancelled(tab_id);
}

// Reports a histogram using the format
// WebUITabStrip.|histogram_fragment|.[tab count bucket].
void TabStripPageHandler::ReportTabDurationHistogram(
    const char* histogram_fragment,
    int tab_count,
    base::TimeDelta duration) {
  if (tab_count <= 0)
    return;

  // It isn't possible to report both a number of tabs and duration datapoint
  // together in a histogram or to correlate two histograms together. As a
  // result the histogram is manually bucketed.
  const char* tab_count_bucket = "01_05";
  if (6 <= tab_count && tab_count <= 20) {
    tab_count_bucket = "06_20";
  } else if (20 < tab_count) {
    tab_count_bucket = "21_";
  }

  std::string histogram_name = base::JoinString(
      {"WebUITabStrip", histogram_fragment, tab_count_bucket}, ".");
  base::UmaHistogramTimes(histogram_name, duration);
}

gfx::ImageSkia TabStripPageHandler::ThemeFavicon(const gfx::ImageSkia& source,
                                                 bool active_tab_icon) {
  if (active_tab_icon) {
    return favicon::ThemeFavicon(
        source, embedder_->GetColorProviderColor(kColorThumbnailTabForeground),
        embedder_->GetColorProviderColor(kColorThumbnailTabBackground),
        embedder_->GetColorProviderColor(kColorThumbnailTabBackground));
  }

  return favicon::ThemeFavicon(
      source, embedder_->GetColorProviderColor(kColorToolbarButtonIcon),
      embedder_->GetColorProviderColor(kColorTabBackgroundActiveFrameActive),
      embedder_->GetColorProviderColor(kColorTabBackgroundInactiveFrameActive));
}

void TabStripPageHandler::ActivateTab(int32_t tab_id) {
  TabStripModel* tab_strip_model = browser_->tab_strip_model();
  for (int index = 0; index < tab_strip_model->count(); ++index) {
    content::WebContents* contents = tab_strip_model->GetWebContentsAt(index);
    if (extensions::ExtensionTabUtil::GetTabId(contents) == tab_id) {
      tab_strip_model->ActivateTabAt(index);
    }
  }
}

void TabStripPageHandler::OnThemeChanged() {
  page_->ThemeChanged();
}

void TabStripPageHandler::OnNativeThemeUpdated(
    ui::NativeTheme* observed_theme) {
  // There are two types of theme update. a) The observed theme change. e.g.
  // switch between light/dark mode. b) A different theme is enabled. e.g.
  // switch between GTK and classic theme on Linux. Reset observer in case b).
  ui::NativeTheme* current_theme =
      webui::GetNativeThemeDeprecated(web_ui_->GetWebContents());
  if (observed_theme != current_theme) {
    theme_observation_.Reset();
    theme_observation_.Observe(current_theme);
  }
  page_->ThemeChanged();
}
