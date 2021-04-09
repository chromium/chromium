// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/tab_strip/tab_strip_ui_handler.h"

#include <algorithm>
#include <memory>

#include "base/containers/span.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/optional.h"
#include "base/values.h"
#include "build/chromeos_buildflags.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/favicon/favicon_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_group.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_group_theme.h"
#include "chrome/browser/ui/tabs/tab_menu_model.h"
#include "chrome/browser/ui/tabs/tab_renderer_data.h"
#include "chrome/browser/ui/tabs/tab_utils.h"
#include "chrome/browser/ui/webui/tab_strip/tab_strip_ui.h"
#include "chrome/browser/ui/webui/tab_strip/tab_strip_ui_embedder.h"
#include "chrome/browser/ui/webui/tab_strip/tab_strip_ui_metrics.h"
#include "chrome/browser/ui/webui/tab_strip/tab_strip_ui_util.h"
#include "chrome/browser/ui/webui/util/image_util.h"
#include "chrome/grit/generated_resources.h"
#include "components/tab_groups/tab_group_color.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "third_party/blink/public/common/input/web_gesture_event.h"
#include "ui/aura/window_delegate.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/list_selection_model.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/base/theme_provider.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/events/gesture_event_details.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/range/range.h"

namespace {

// Delay in milliseconds of when the dragging UI should be shown for touch drag.
// Note: For better user experience, this is made shorter than
// ET_GESTURE_LONG_PRESS delay, which is too long for this case, e.g., about
// 650ms.
constexpr base::TimeDelta kTouchLongpressDelay =
    base::TimeDelta::FromMilliseconds(300);

std::string ConvertAlertStateToString(TabAlertState alert_state) {
  switch (alert_state) {
    case TabAlertState::MEDIA_RECORDING:
      return "media-recording";
    case TabAlertState::TAB_CAPTURING:
      return "tab-capturing";
    case TabAlertState::AUDIO_PLAYING:
      return "audio-playing";
    case TabAlertState::AUDIO_MUTING:
      return "audio-muting";
    case TabAlertState::BLUETOOTH_CONNECTED:
      return "bluetooth-connected";
    case TabAlertState::BLUETOOTH_SCAN_ACTIVE:
      return "bluetooth-connected";
    case TabAlertState::USB_CONNECTED:
      return "usb-connected";
    case TabAlertState::HID_CONNECTED:
      return "hid-connected";
    case TabAlertState::SERIAL_CONNECTED:
      return "serial-connected";
    case TabAlertState::PIP_PLAYING:
      return "pip-playing";
    case TabAlertState::DESKTOP_CAPTURING:
      return "desktop-capturing";
    case TabAlertState::VR_PRESENTING_IN_HEADSET:
      return "vr-presenting";
    default:
      NOTREACHED();
  }
}

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
  Browser* const browser_;
  const ui::AcceleratorProvider* const accelerator_provider_;
};

class WebUITabContextMenu : public ui::SimpleMenuModel::Delegate,
                            public TabMenuModel {
 public:
  WebUITabContextMenu(Browser* browser,
                      const ui::AcceleratorProvider* accelerator_provider,
                      int tab_index)
      : TabMenuModel(this, browser->tab_strip_model(), tab_index),
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
  Browser* const browser_;
  const ui::AcceleratorProvider* const accelerator_provider_;
  const int tab_index_;
};

bool IsSortedAndContiguous(base::span<const int> sequence) {
  if (sequence.size() < 2)
    return true;

  if (!std::is_sorted(sequence.begin(), sequence.end()))
    return false;

  return sequence.back() ==
         sequence.front() + static_cast<int>(sequence.size()) - 1;
}

}  // namespace

TabStripUIHandler::TabStripUIHandler(Browser* browser,
                                     TabStripUIEmbedder* embedder)
    : browser_(browser),
      embedder_(embedder),
      thumbnail_tracker_(
          base::BindRepeating(&TabStripUIHandler::HandleThumbnailUpdate,
                              base::Unretained(this))),
      tab_before_unload_tracker_(
          base::BindRepeating(&TabStripUIHandler::OnTabCloseCancelled,
                              base::Unretained(this))),
      long_press_timer_(std::make_unique<base::RetainingOneShotTimer>(
          FROM_HERE,
          kTouchLongpressDelay,
          base::BindRepeating(&TabStripUIHandler::OnLongPressTimer,
                              base::Unretained(this)))) {}
TabStripUIHandler::~TabStripUIHandler() = default;

void TabStripUIHandler::NotifyLayoutChanged() {
  if (!IsJavascriptAllowed())
    return;
  FireWebUIListener("layout-changed", embedder_->GetLayout().AsDictionary());
}

void TabStripUIHandler::NotifyReceivedKeyboardFocus() {
  if (!IsJavascriptAllowed())
    return;
  FireWebUIListener("received-keyboard-focus");
}

void TabStripUIHandler::NotifyContextMenuClosed() {
  if (!IsJavascriptAllowed())
    return;
  FireWebUIListener("context-menu-closed");
}

// content::WebUIMessageHandler:
void TabStripUIHandler::OnJavascriptAllowed() {
  web_ui()->GetWebContents()->SetDelegate(this);
  browser_->tab_strip_model()->AddObserver(this);
}

// TabStripModelObserver:
void TabStripUIHandler::OnTabGroupChanged(const TabGroupChange& change) {
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
      FireWebUIListener(
          "tab-group-visuals-changed", base::Value(change.group.ToString()),
          GetTabGroupData(
              browser_->tab_strip_model()->group_model()->GetTabGroup(
                  change.group)));
      break;
    }

    case TabGroupChange::kMoved: {
      const int start_tab = browser_->tab_strip_model()
                                ->group_model()
                                ->GetTabGroup(change.group)
                                ->ListTabs()
                                .start();
      FireWebUIListener("tab-group-moved", base::Value(change.group.ToString()),
                        base::Value(start_tab));
      break;
    }

    case TabGroupChange::kClosed: {
      FireWebUIListener("tab-group-closed",
                        base::Value(change.group.ToString()));
      break;
    }
  }
}

void TabStripUIHandler::TabGroupedStateChanged(
    base::Optional<tab_groups::TabGroupId> group,
    content::WebContents* contents,
    int index) {
  int tab_id = extensions::ExtensionTabUtil::GetTabId(contents);
  if (group.has_value()) {
    FireWebUIListener("tab-group-state-changed", base::Value(tab_id),
                      base::Value(index),
                      base::Value(group.value().ToString()));
  } else {
    FireWebUIListener("tab-group-state-changed", base::Value(tab_id),
                      base::Value(index));
  }
}

void TabStripUIHandler::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  if (tab_strip_model->empty())
    return;

  switch (change.type()) {
    case TabStripModelChange::kInserted: {
      for (const auto& contents : change.GetInsert()->contents) {
        FireWebUIListener("tab-created",
                          GetTabData(contents.contents, contents.index));
      }
      break;
    }
    case TabStripModelChange::kRemoved: {
      for (const auto& contents : change.GetRemove()->contents) {
        FireWebUIListener("tab-removed",
                          base::Value(extensions::ExtensionTabUtil::GetTabId(
                              contents.contents)));
      }
      break;
    }
    case TabStripModelChange::kMoved: {
      auto* move = change.GetMove();

      base::Optional<tab_groups::TabGroupId> tab_group_id =
          tab_strip_model->GetTabGroupForTab(move->to_index);
      if (tab_group_id.has_value()) {
        const gfx::Range tabs_in_group = tab_strip_model->group_model()
                                             ->GetTabGroup(tab_group_id.value())
                                             ->ListTabs();

        const ui::ListSelectionModel::SelectedIndices& sel =
            selection.new_model.selected_indices();
        const auto& selected_tabs = std::vector<int>(sel.begin(), sel.end());
        const bool all_tabs_in_group =
            IsSortedAndContiguous(base::make_span(selected_tabs)) &&
            selected_tabs.front() == static_cast<int>(tabs_in_group.start()) &&
            selected_tabs.size() == tabs_in_group.length();

        if (all_tabs_in_group) {
          // If the selection includes all the tabs within the changed tab's
          // group, it is an indication that the entire group is being moved.
          // To prevent sending multiple events for each tab in the group,
          // ignore these tabs moving as entire group moves will be handled by
          // TabGroupChange::kMoved.
          break;
        }
      }

      FireWebUIListener(
          "tab-moved",
          base::Value(extensions::ExtensionTabUtil::GetTabId(move->contents)),
          base::Value(move->to_index),
          base::Value(tab_strip_model->IsTabPinned(move->to_index)));
      break;
    }
    case TabStripModelChange::kReplaced: {
      auto* replace = change.GetReplace();
      FireWebUIListener("tab-replaced",
                        base::Value(extensions::ExtensionTabUtil::GetTabId(
                            replace->old_contents)),
                        base::Value(extensions::ExtensionTabUtil::GetTabId(
                            replace->new_contents)));
      break;
    }
    case TabStripModelChange::kSelectionOnly:
      // Multi-selection is not supported for touch.
      break;
  }

  if (selection.active_tab_changed()) {
    content::WebContents* new_contents = selection.new_contents;
    int index = selection.new_model.active();
    if (new_contents && index != TabStripModel::kNoTab) {
      FireWebUIListener(
          "tab-active-changed",
          base::Value(extensions::ExtensionTabUtil::GetTabId(new_contents)));
    }
  }
}

void TabStripUIHandler::TabChangedAt(content::WebContents* contents,
                                     int index,
                                     TabChangeType change_type) {
  FireWebUIListener("tab-updated", GetTabData(contents, index));
}

void TabStripUIHandler::TabPinnedStateChanged(TabStripModel* tab_strip_model,
                                              content::WebContents* contents,
                                              int index) {
  FireWebUIListener("tab-updated", GetTabData(contents, index));
}

void TabStripUIHandler::TabBlockedStateChanged(content::WebContents* contents,
                                               int index) {
  FireWebUIListener("tab-updated", GetTabData(contents, index));
}

bool TabStripUIHandler::PreHandleGestureEvent(
    content::WebContents* source,
    const blink::WebGestureEvent& event) {
  switch (event.GetType()) {
    case blink::WebInputEvent::Type::kGestureScrollBegin:
      // Drag and drop for the WebUI tab strip is currently only supported for
      // Aura platforms.
#if defined(USE_AURA)
      // If we are passed the `kTouchLongpressDelay` threshold since the initial
      // tap down initiate a drag on scroll start.
      if (!long_press_timer_->IsRunning()) {
        handling_gesture_scroll_ = true;

        // If we are about to start a drag ensure the context menu is closed.
        embedder_->CloseContextMenu();

        // Synthesize a long press event to start the drag and drop session.
        // TODO(tluk): Replace this with a better drag and drop trigger when
        // available.
        ui::GestureEventDetails press_details(ui::ET_GESTURE_LONG_PRESS);
        press_details.set_device_type(
            ui::GestureDeviceType::DEVICE_TOUCHSCREEN);
        ui::GestureEvent press_event(
            touch_drag_start_point_.x(), touch_drag_start_point_.y(),
            ui::EF_IS_SYNTHESIZED, base::TimeTicks::Now(), press_details);

        auto* window = web_ui()->GetWebContents()->GetContentNativeView();
        window->delegate()->OnGestureEvent(&press_event);

        // Following the long press we need to dispatch a scroll end event to
        // ensure the gesture stream is not left in an inconsistent state.
        ui::GestureEventDetails scroll_end_details(ui::ET_GESTURE_SCROLL_END);
        scroll_end_details.set_device_type(
            ui::GestureDeviceType::DEVICE_TOUCHSCREEN);
        ui::GestureEvent scroll_end_event(
            touch_drag_start_point_.x(), touch_drag_start_point_.y(),
            ui::EF_IS_SYNTHESIZED, base::TimeTicks::Now(), scroll_end_details);
        window->delegate()->OnGestureEvent(&scroll_end_event);
        return true;
      }
      long_press_timer_->Stop();
      return false;
#endif  // defined(USE_AURA)
      return false;
    case blink::WebInputEvent::Type::kGestureScrollEnd:
      handling_gesture_scroll_ = false;
      return false;
    case blink::WebInputEvent::Type::kGestureTapDown:
      touch_drag_start_point_ =
          gfx::ToRoundedPoint(event.PositionInRootFrame());
      long_press_timer_->Reset();
      return false;
    case blink::WebInputEvent::Type::kGestureLongPress:
      // Do not block the long press if handling a scroll gesture.
      if (handling_gesture_scroll_)
        return false;
      FireWebUIListener("show-context-menu");
      return true;
    default:
      break;
  }
  return false;
}

// content::WebUIMessageHandler:
void TabStripUIHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "createNewTab",
      base::BindRepeating(&TabStripUIHandler::HandleCreateNewTab,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getTabs", base::BindRepeating(&TabStripUIHandler::HandleGetTabs,
                                     base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getGroupVisualData",
      base::BindRepeating(&TabStripUIHandler::HandleGetGroupVisualData,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getThemeColors",
      base::BindRepeating(&TabStripUIHandler::HandleGetThemeColors,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "groupTab", base::BindRepeating(&TabStripUIHandler::HandleGroupTab,
                                      base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "ungroupTab", base::BindRepeating(&TabStripUIHandler::HandleUngroupTab,
                                        base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "moveGroup", base::BindRepeating(&TabStripUIHandler::HandleMoveGroup,
                                       base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "moveTab", base::BindRepeating(&TabStripUIHandler::HandleMoveTab,
                                     base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "setThumbnailTracked",
      base::BindRepeating(&TabStripUIHandler::HandleSetThumbnailTracked,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "closeContainer",
      base::BindRepeating(&TabStripUIHandler::HandleCloseContainer,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "closeTab", base::BindRepeating(&TabStripUIHandler::HandleCloseTab,
                                      base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "showBackgroundContextMenu",
      base::BindRepeating(&TabStripUIHandler::HandleShowBackgroundContextMenu,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "showEditDialogForGroup",
      base::BindRepeating(&TabStripUIHandler::HandleShowEditDialogForGroup,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "showTabContextMenu",
      base::BindRepeating(&TabStripUIHandler::HandleShowTabContextMenu,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getLayout", base::BindRepeating(&TabStripUIHandler::HandleGetLayout,
                                       base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "reportTabActivationDuration",
      base::BindRepeating(&TabStripUIHandler::HandleReportTabActivationDuration,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "reportTabDataReceivedDuration",
      base::BindRepeating(
          &TabStripUIHandler::HandleReportTabDataReceivedDuration,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "reportTabCreationDuration",
      base::BindRepeating(&TabStripUIHandler::HandleReportTabCreationDuration,
                          base::Unretained(this)));
}

void TabStripUIHandler::OnLongPressTimer() {
  if (!IsJavascriptAllowed())
    return;
  FireWebUIListener("long-press");
}

void TabStripUIHandler::HandleCreateNewTab(const base::ListValue* args) {
  UMA_HISTOGRAM_ENUMERATION("Tab.NewTab",
                            TabStripModel::NEW_TAB_BUTTON_IN_WEBUI_TAB_STRIP,
                            TabStripModel::NEW_TAB_ENUM_COUNT);
  chrome::ExecuteCommand(browser_, IDC_NEW_TAB);
}

base::DictionaryValue TabStripUIHandler::GetTabData(
    content::WebContents* contents,
    int index) {
  base::DictionaryValue tab_data;

  tab_data.SetBoolean("active",
                      browser_->tab_strip_model()->active_index() == index);
  tab_data.SetInteger("id", extensions::ExtensionTabUtil::GetTabId(contents));
  tab_data.SetInteger("index", index);

  const base::Optional<tab_groups::TabGroupId> group_id =
      browser_->tab_strip_model()->GetTabGroupForTab(index);
  if (group_id.has_value()) {
    tab_data.SetString("groupId", group_id.value().ToString());
  }

  TabRendererData tab_renderer_data =
      TabRendererData::FromTabInModel(browser_->tab_strip_model(), index);
  tab_data.SetBoolean("pinned", tab_renderer_data.pinned);
  tab_data.SetString("title", tab_renderer_data.title);
  tab_data.SetString("url", tab_renderer_data.visible_url.GetContent());

  if (!tab_renderer_data.favicon.isNull()) {
    tab_data.SetString("favIconUrl", webui::EncodePNGAndMakeDataURI(
                                         tab_renderer_data.favicon,
                                         web_ui()->GetDeviceScaleFactor()));
    tab_data.SetBoolean("isDefaultFavicon",
                        tab_renderer_data.favicon.BackedBySameObjectAs(
                            favicon::GetDefaultFavicon().AsImageSkia()));
  } else {
    tab_data.SetBoolean("isDefaultFavicon", true);
  }
  tab_data.SetBoolean("showIcon", tab_renderer_data.show_icon);
  tab_data.SetInteger("networkState",
                      static_cast<int>(tab_renderer_data.network_state));
  tab_data.SetBoolean("shouldHideThrobber",
                      tab_renderer_data.should_hide_throbber);
  tab_data.SetBoolean("blocked", tab_renderer_data.blocked);
  tab_data.SetBoolean("crashed", tab_renderer_data.IsCrashed());
  // TODO(johntlee): Add the rest of TabRendererData

  auto alert_states = std::make_unique<base::ListValue>();
  for (const auto alert_state :
       chrome::GetTabAlertStatesForContents(contents)) {
    alert_states->Append(ConvertAlertStateToString(alert_state));
  }
  tab_data.SetList("alertStates", std::move(alert_states));

  return tab_data;
}

base::DictionaryValue TabStripUIHandler::GetTabGroupData(TabGroup* group) {
  const tab_groups::TabGroupVisualData* visual_data = group->visual_data();

  base::DictionaryValue visual_data_dict;
  visual_data_dict.SetString("title", visual_data->title());

  // TODO the tab strip should support toggles between inactive and active frame
  // states. Currently the webui tab strip only uses active frame colors
  // (https://crbug.com/1060398).
  const int color_id = GetTabGroupTabStripColorId(visual_data->color(), true);
  const SkColor group_color = embedder_->GetColor(color_id);
  visual_data_dict.SetString("color",
                             color_utils::SkColorToRgbString(group_color));
  visual_data_dict.SetString(
      "textColor", color_utils::SkColorToRgbString(
                       color_utils::GetColorWithMaxContrast(group_color)));

  return visual_data_dict;
}

void TabStripUIHandler::HandleGetTabs(const base::ListValue* args) {
  AllowJavascript();
  const base::Value& callback_id = args->GetList()[0];

  base::ListValue tabs;
  TabStripModel* tab_strip_model = browser_->tab_strip_model();
  for (int i = 0; i < tab_strip_model->count(); ++i) {
    tabs.Append(GetTabData(tab_strip_model->GetWebContentsAt(i), i));
  }
  ResolveJavascriptCallback(callback_id, tabs);
}

void TabStripUIHandler::HandleGetGroupVisualData(const base::ListValue* args) {
  AllowJavascript();
  const base::Value& callback_id = args->GetList()[0];

  base::DictionaryValue group_visual_datas;
  std::vector<tab_groups::TabGroupId> groups =
      browser_->tab_strip_model()->group_model()->ListTabGroups();
  for (const tab_groups::TabGroupId& group : groups) {
    group_visual_datas.SetDictionary(
        group.ToString(),
        std::make_unique<base::DictionaryValue>(GetTabGroupData(
            browser_->tab_strip_model()->group_model()->GetTabGroup(group))));
  }
  ResolveJavascriptCallback(callback_id, group_visual_datas);
}

void TabStripUIHandler::HandleGetThemeColors(const base::ListValue* args) {
  AllowJavascript();
  const base::Value& callback_id = args->GetList()[0];

  // This should return an object of CSS variables to rgba values so that
  // the WebUI can use the CSS variables to color the tab strip
  base::DictionaryValue colors;
  colors.SetString("--tabstrip-background-color",
                   color_utils::SkColorToRgbaString(embedder_->GetColor(
                       ThemeProperties::COLOR_FRAME_ACTIVE)));
  colors.SetString("--tabstrip-tab-background-color",
                   color_utils::SkColorToRgbaString(
                       embedder_->GetColor(ThemeProperties::COLOR_TOOLBAR)));
  colors.SetString(
      "--tabstrip-tab-text-color",
      color_utils::SkColorToRgbaString(embedder_->GetColor(
          ThemeProperties::COLOR_TAB_FOREGROUND_ACTIVE_FRAME_ACTIVE)));
  colors.SetString(
      "--tabstrip-tab-separator-color",
      color_utils::SkColorToRgbaString(SkColorSetA(
          embedder_->GetColor(
              ThemeProperties::COLOR_TAB_FOREGROUND_ACTIVE_FRAME_ACTIVE),
          /* 16% opacity */ 0.16 * 255)));

  std::string throbber_color = color_utils::SkColorToRgbaString(
      embedder_->GetColor(ThemeProperties::COLOR_TAB_THROBBER_SPINNING));
  colors.SetString("--tabstrip-tab-loading-spinning-color", throbber_color);
  colors.SetString("--tabstrip-tab-waiting-spinning-color",
                   color_utils::SkColorToRgbaString(embedder_->GetColor(
                       ThemeProperties::COLOR_TAB_THROBBER_WAITING)));
  colors.SetString("--tabstrip-indicator-recording-color",
                   color_utils::SkColorToRgbaString(embedder_->GetSystemColor(
                       ui::NativeTheme::kColorId_AlertSeverityHigh)));
  colors.SetString("--tabstrip-indicator-pip-color", throbber_color);
  colors.SetString("--tabstrip-indicator-capturing-color", throbber_color);
  colors.SetString("--tabstrip-tab-blocked-color",
                   color_utils::SkColorToRgbaString(embedder_->GetSystemColor(
                       ui::NativeTheme::kColorId_ProminentButtonColor)));
  colors.SetString("--tabstrip-focus-outline-color",
                   color_utils::SkColorToRgbaString(embedder_->GetSystemColor(
                       ui::NativeTheme::kColorId_FocusedBorderColor)));

#if !BUILDFLAG(IS_CHROMEOS_ASH)
  colors.SetString(
      "--tabstrip-scrollbar-thumb-color-rgb",
      color_utils::SkColorToRgbString(color_utils::GetColorWithMaxContrast(
          embedder_->GetColor(ThemeProperties::COLOR_FRAME_ACTIVE))));
#endif

  ResolveJavascriptCallback(callback_id, colors);
}

void TabStripUIHandler::HandleGroupTab(const base::ListValue* args) {
  int tab_id = args->GetList()[0].GetInt();

  int tab_index = -1;
  bool got_tab = extensions::ExtensionTabUtil::GetTabById(
      tab_id, browser_->profile(), /*include_incognito=*/true, nullptr, nullptr,
      nullptr, &tab_index);
  DCHECK(got_tab);

  const std::string group_id_string = args->GetList()[1].GetString();
  base::Optional<tab_groups::TabGroupId> group_id =
      tab_strip_ui::GetTabGroupIdFromString(
          browser_->tab_strip_model()->group_model(), group_id_string);
  if (group_id.has_value()) {
    browser_->tab_strip_model()->AddToExistingGroup({tab_index},
                                                    group_id.value());
  }
}

void TabStripUIHandler::HandleUngroupTab(const base::ListValue* args) {
  int tab_id = args->GetList()[0].GetInt();

  int tab_index = -1;
  bool got_tab = extensions::ExtensionTabUtil::GetTabById(
      tab_id, browser_->profile(), /*include_incognito=*/true, nullptr, nullptr,
      nullptr, &tab_index);
  DCHECK(got_tab);

  browser_->tab_strip_model()->RemoveFromGroup({tab_index});
}

void TabStripUIHandler::HandleMoveGroup(const base::ListValue* args) {
  const std::string group_id_string = args->GetList()[0].GetString();

  int to_index = args->GetList()[1].GetInt();
  if (to_index == -1) {
    to_index = browser_->tab_strip_model()->count();
  }

  auto* target_browser = browser_;
  Browser* source_browser =
      tab_strip_ui::GetBrowserWithGroupId(browser_->profile(), group_id_string);
  if (!source_browser) {
    return;
  }

  base::Optional<tab_groups::TabGroupId> group_id =
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
    int active_index =
        target_browser->tab_strip_model()->selection_model().active();
    ui::ListSelectionModel group_selection;
    group_selection.SetSelectedIndex(tabs_in_group.start());
    group_selection.SetSelectionFromAnchorTo(tabs_in_group.end() - 1);
    group_selection.set_active(active_index);
    target_browser->tab_strip_model()->SetSelectionFromModel(group_selection);

    target_browser->tab_strip_model()->MoveGroupTo(group_id.value(), to_index);
    return;
  }

  target_browser->tab_strip_model()->group_model()->AddTabGroup(
      group_id.value(),
      base::Optional<tab_groups::TabGroupVisualData>{*group->visual_data()});

  gfx::Range source_tab_indices = group->ListTabs();
  const int tab_count = source_tab_indices.length();
  const int from_index = source_tab_indices.start();
  for (int i = 0; i < tab_count; i++) {
    tab_strip_ui::MoveTabAcrossWindows(source_browser, from_index,
                                       target_browser, to_index + i, group_id);
  }
}

void TabStripUIHandler::HandleMoveTab(const base::ListValue* args) {
  int tab_id = args->GetList()[0].GetInt();
  int to_index = args->GetList()[1].GetInt();
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

void TabStripUIHandler::HandleCloseContainer(const base::ListValue* args) {
  // We only autoclose for tab selection.
  RecordTabStripUICloseHistogram(TabStripUICloseAction::kTabSelected);
  DCHECK(embedder_);
  embedder_->CloseContainer();
}

void TabStripUIHandler::HandleCloseTab(const base::ListValue* args) {
  AllowJavascript();

  int tab_id = args->GetList()[0].GetInt();
  content::WebContents* tab = nullptr;
  if (!extensions::ExtensionTabUtil::GetTabById(tab_id, browser_->profile(),
                                                true, &tab)) {
    // ID didn't refer to a valid tab.
    DVLOG(1) << "Invalid tab ID";
    return;
  }

  bool tab_was_swiped = args->GetList()[1].GetBool();
  if (tab_was_swiped) {
    // The unload tracker will automatically unobserve the tab when it
    // successfully closes.
    tab_before_unload_tracker_.Observe(tab);
  }
  tab->Close();
}

void TabStripUIHandler::HandleShowBackgroundContextMenu(
    const base::ListValue* args) {
  gfx::PointF point;
  {
    double x = args->GetList()[0].GetDouble();
    double y = args->GetList()[1].GetDouble();
    point = gfx::PointF(x, y);
  }

  DCHECK(embedder_);
  embedder_->ShowContextMenuAtPoint(
      gfx::ToRoundedPoint(point),
      std::make_unique<WebUIBackgroundContextMenu>(
          browser_, embedder_->GetAcceleratorProvider()),
      base::BindRepeating(&TabStripUIHandler::NotifyContextMenuClosed,
                          weak_ptr_factory_.GetWeakPtr()));
}

void TabStripUIHandler::HandleShowEditDialogForGroup(
    const base::ListValue* args) {
  const std::string group_id_string = args->GetList()[0].GetString();
  base::Optional<tab_groups::TabGroupId> group_id =
      tab_strip_ui::GetTabGroupIdFromString(
          browser_->tab_strip_model()->group_model(), group_id_string);
  if (!group_id.has_value()) {
    return;
  }

  gfx::Point point;
  {
    double x = args->GetList()[1].GetDouble();
    double y = args->GetList()[2].GetDouble();
    point = gfx::Point(x, y);
  }

  gfx::Rect rect;
  {
    double width = args->GetList()[3].GetDouble();
    double height = args->GetList()[4].GetDouble();
    rect = gfx::Rect(width, height);
  }

  DCHECK(embedder_);
  embedder_->ShowEditDialogForGroupAtPoint(point, rect, group_id.value());
}

void TabStripUIHandler::HandleShowTabContextMenu(const base::ListValue* args) {
  int tab_id = args->GetList()[0].GetInt();

  gfx::PointF point;
  {
    double x = args->GetList()[1].GetDouble();
    double y = args->GetList()[2].GetDouble();
    point = gfx::PointF(x, y);
  }

  Browser* browser = nullptr;
  int tab_index = -1;
  const bool got_tab = extensions::ExtensionTabUtil::GetTabById(
      tab_id, browser_->profile(), true /* include_incognito */, &browser,
      nullptr, nullptr, &tab_index);
  CHECK(got_tab);

  if (browser != browser_) {
    // TODO(crbug.com/1141573): Investigate how a context menu is being opened
    // for a tab that is no longer in the tab strip. Until then, fire a
    // tab-removed event so the tab is removed from this tab strip.
    FireWebUIListener("tab-removed", base::Value(tab_id));
    return;
  }

  DCHECK(embedder_);
  embedder_->ShowContextMenuAtPoint(
      gfx::ToRoundedPoint(point),
      std::make_unique<WebUITabContextMenu>(
          browser, embedder_->GetAcceleratorProvider(), tab_index),
      base::BindRepeating(&TabStripUIHandler::NotifyContextMenuClosed,
                          weak_ptr_factory_.GetWeakPtr()));
}

void TabStripUIHandler::HandleGetLayout(const base::ListValue* args) {
  AllowJavascript();
  const base::Value& callback_id = args->GetList()[0];

  base::Value layout = embedder_->GetLayout().AsDictionary();
  ResolveJavascriptCallback(callback_id, layout);
}

void TabStripUIHandler::HandleSetThumbnailTracked(const base::ListValue* args) {
  AllowJavascript();

  int tab_id = args->GetList()[0].GetInt();
  const bool thumbnail_tracked = args->GetList()[1].GetBool();

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

void TabStripUIHandler::HandleReportTabActivationDuration(
    const base::ListValue* args) {
  int duration_ms = args->GetList()[0].GetInt();
  UMA_HISTOGRAM_TIMES("WebUITabStrip.TabActivation",
                      base::TimeDelta::FromMilliseconds(duration_ms));
}

void TabStripUIHandler::HandleReportTabDataReceivedDuration(
    const base::ListValue* args) {
  int tab_count = args->GetList()[0].GetInt();
  int duration_ms = args->GetList()[1].GetInt();
  ReportTabDurationHistogram("TabDataReceived", tab_count,
                             base::TimeDelta::FromMilliseconds(duration_ms));
}

void TabStripUIHandler::HandleReportTabCreationDuration(
    const base::ListValue* args) {
  int tab_count = args->GetList()[0].GetInt();
  int duration_ms = args->GetList()[1].GetInt();
  ReportTabDurationHistogram("TabCreation", tab_count,
                             base::TimeDelta::FromMilliseconds(duration_ms));
}

// Callback passed to |thumbnail_tracker_|. Called when a tab's thumbnail
// changes, or when we start watching the tab.
void TabStripUIHandler::HandleThumbnailUpdate(
    content::WebContents* tab,
    ThumbnailTracker::CompressedThumbnailData image) {
  // Send base-64 encoded image to JS side. If |image| is blank (i.e.
  // there is no data), send a blank URI.
  std::string data_uri;
  if (image)
    data_uri = webui::MakeDataURIForImage(base::make_span(image->data), "jpeg");

  const int tab_id = extensions::ExtensionTabUtil::GetTabId(tab);
  FireWebUIListener("tab-thumbnail-updated", base::Value(tab_id),
                    base::Value(data_uri));
}

void TabStripUIHandler::OnTabCloseCancelled(content::WebContents* tab) {
  tab_before_unload_tracker_.Unobserve(tab);
  const int tab_id = extensions::ExtensionTabUtil::GetTabId(tab);
  FireWebUIListener("tab-close-cancelled", base::Value(tab_id));
}

// Reports a histogram using the format
// WebUITabStrip.|histogram_fragment|.[tab count bucket].
void TabStripUIHandler::ReportTabDurationHistogram(
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
