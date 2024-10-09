// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/renderer_context_menu/render_view_context_menu_base.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/observer_list.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "ppapi/buildflags/buildflags.h"
#include "third_party/blink/public/mojom/context_menu/context_menu.mojom.h"
#include "ui/base/models/image_model.h"
#include "url/origin.h"

using content::BrowserContext;
using content::GlobalRenderFrameHostId;
using content::OpenURLParams;
using content::RenderFrameHost;
using content::RenderViewHost;
using content::WebContents;

namespace {

// The (inclusive) range of command IDs reserved for content's custom menus.
int content_context_custom_first = -1;
int content_context_custom_last = -1;

bool IsCustomItemEnabledInternal(
    const std::vector<blink::mojom::CustomContextMenuItemPtr>& items,
    int id) {
  DCHECK(RenderViewContextMenuBase::IsContentCustomCommandId(id));
  for (const auto& item : items) {
    int action_id = RenderViewContextMenuBase::ConvertToContentCustomCommandId(
        item->action);
    if (action_id == id)
      return item->enabled;
    if (item->type == blink::mojom::CustomContextMenuItemType::kSubMenu) {
      if (IsCustomItemEnabledInternal(item->submenu, id))
        return true;
    }
  }
  return false;
}

bool IsCustomItemCheckedInternal(
    const std::vector<blink::mojom::CustomContextMenuItemPtr>& items,
    int id) {
  DCHECK(RenderViewContextMenuBase::IsContentCustomCommandId(id));
  for (const auto& item : items) {
    int action_id = RenderViewContextMenuBase::ConvertToContentCustomCommandId(
        item->action);
    if (action_id == id)
      return item->checked;
    if (item->type == blink::mojom::CustomContextMenuItemType::kSubMenu) {
      if (IsCustomItemCheckedInternal(item->submenu, id))
        return true;
    }
  }
  return false;
}

const size_t kMaxCustomMenuDepth = 5;
const size_t kMaxCustomMenuTotalItems = 1000;

void AddCustomItemsToMenu(
    const std::vector<blink::mojom::CustomContextMenuItemPtr>& items,
    size_t depth,
    size_t* total_items,
    std::vector<std::unique_ptr<ui::SimpleMenuModel>>* submenus,
    ui::SimpleMenuModel::Delegate* delegate,
    ui::SimpleMenuModel* menu_model) {
  if (depth > kMaxCustomMenuDepth) {
    LOG(ERROR) << "Custom menu too deeply nested.";
    return;
  }
  for (const auto& item : items) {
    int command_id = RenderViewContextMenuBase::ConvertToContentCustomCommandId(
        item->action);
    if (!RenderViewContextMenuBase::IsContentCustomCommandId(command_id)) {
      LOG(ERROR) << "Custom menu action value out of range.";
      return;
    }
    if (*total_items >= kMaxCustomMenuTotalItems) {
      LOG(ERROR) << "Custom menu too large (too many items).";
      return;
    }
    (*total_items)++;
    switch (item->type) {
      case blink::mojom::CustomContextMenuItemType::kOption:
        menu_model->AddItem(
            RenderViewContextMenuBase::ConvertToContentCustomCommandId(
                item->action),
            item->label);
        if (item->is_experimental_feature) {
          menu_model->SetMinorIcon(
              menu_model->GetItemCount() - 1,
              ui::ImageModel::FromVectorIcon(vector_icons::kScienceIcon));
        }
        break;
      case blink::mojom::CustomContextMenuItemType::kCheckableOption:
        menu_model->AddCheckItem(
            RenderViewContextMenuBase::ConvertToContentCustomCommandId(
                item->action),
            item->label);
        break;
      case blink::mojom::CustomContextMenuItemType::kGroup:
        // TODO(viettrungluu): I don't know what this is supposed to do.
        NOTREACHED_IN_MIGRATION();
        break;
      case blink::mojom::CustomContextMenuItemType::kSeparator:
        menu_model->AddSeparator(ui::NORMAL_SEPARATOR);
        break;
      case blink::mojom::CustomContextMenuItemType::kSubMenu: {
        ui::SimpleMenuModel* submenu = new ui::SimpleMenuModel(delegate);
        submenus->push_back(base::WrapUnique(submenu));
        AddCustomItemsToMenu(item->submenu, depth + 1, total_items, submenus,
                             delegate, submenu);
        menu_model->AddSubMenu(
            RenderViewContextMenuBase::ConvertToContentCustomCommandId(
                item->action),
            item->label, submenu);
        break;
      }
      default:
        NOTREACHED_IN_MIGRATION();
        break;
    }
  }
}

}  // namespace

// static
void RenderViewContextMenuBase::SetContentCustomCommandIdRange(
    int first, int last) {
  // The range is inclusive.
  content_context_custom_first = first;
  content_context_custom_last = last;
}

// static
const size_t RenderViewContextMenuBase::kMaxSelectionTextLength = 50;

// static
int RenderViewContextMenuBase::ConvertToContentCustomCommandId(int id) {
  return content_context_custom_first + id;
}

// static
bool RenderViewContextMenuBase::IsContentCustomCommandId(int id) {
  return id >= content_context_custom_first &&
         id <= content_context_custom_last;
}

RenderViewContextMenuBase::RenderViewContextMenuBase(
    content::RenderFrameHost& render_frame_host,
    const content::ContextMenuParams& params)
    : params_(params),
      source_web_contents_(
          WebContents::FromRenderFrameHost(&render_frame_host)),
      browser_context_(source_web_contents_->GetBrowserContext()),
      menu_model_(this),
      render_frame_id_(render_frame_host.GetRoutingID()),
      render_frame_token_(render_frame_host.GetFrameToken()),
      render_process_id_(render_frame_host.GetProcess()->GetID()),
      site_instance_(render_frame_host.GetSiteInstance()),
      command_executed_(false) {}

RenderViewContextMenuBase::~RenderViewContextMenuBase() {
}

// Menu construction functions -------------------------------------------------

void RenderViewContextMenuBase::Init() {
  // Command id range must have been already initializerd.
  DCHECK_NE(-1, content_context_custom_first);
  DCHECK_NE(-1, content_context_custom_last);

  InitMenu();
  if (toolkit_delegate_)
    toolkit_delegate_->Init(&menu_model_);
}

void RenderViewContextMenuBase::Cancel() {
  if (toolkit_delegate_)
    toolkit_delegate_->Cancel();
}

void RenderViewContextMenuBase::InitMenu() {
  if (content_type_->SupportsGroup(ContextMenuContentType::ITEM_GROUP_CUSTOM)) {
    AppendCustomItems();

    const bool has_selection = !params_.selection_text.empty();
    if (has_selection) {
      // We will add more items if there's a selection, so add a separator.
      // TODO(lazyboy): Clean up separator logic.
      menu_model_.AddSeparator(ui::NORMAL_SEPARATOR);
    }
  }
}

void RenderViewContextMenuBase::AddMenuItem(int command_id,
                                            const std::u16string& title) {
  menu_model_.AddItem(command_id, title);
}

void RenderViewContextMenuBase::AddMenuItemWithIcon(
    int command_id,
    const std::u16string& title,
    const ui::ImageModel& icon) {
  menu_model_.AddItemWithIcon(command_id, title, icon);
}

void RenderViewContextMenuBase::AddCheckItem(int command_id,
                                             const std::u16string& title) {
  menu_model_.AddCheckItem(command_id, title);
}

void RenderViewContextMenuBase::AddSeparator() {
  menu_model_.AddSeparator(ui::NORMAL_SEPARATOR);
}

void RenderViewContextMenuBase::AddSubMenu(int command_id,
                                           const std::u16string& label,
                                           ui::MenuModel* model) {
  menu_model_.AddSubMenu(command_id, label, model);
}

void RenderViewContextMenuBase::AddSubMenuWithStringIdAndIcon(
    int command_id,
    int message_id,
    ui::MenuModel* model,
    const ui::ImageModel& icon) {
  menu_model_.AddSubMenuWithStringIdAndIcon(command_id, message_id, model,
                                            icon);
}

void RenderViewContextMenuBase::UpdateMenuItem(int command_id,
                                               bool enabled,
                                               bool hidden,
                                               const std::u16string& label) {
  std::optional<size_t> index = menu_model_.GetIndexOfCommandId(command_id);
  if (!index.has_value())
    return;

  menu_model_.SetLabel(index.value(), label);
  menu_model_.SetEnabledAt(index.value(), enabled);
  menu_model_.SetVisibleAt(index.value(), !hidden);
  if (toolkit_delegate_) {
#if BUILDFLAG(IS_MAC)
    toolkit_delegate_->UpdateMenuItem(command_id, enabled, hidden, label);
#else
    toolkit_delegate_->RebuildMenu();
#endif
  }
}

void RenderViewContextMenuBase::UpdateMenuIcon(int command_id,
                                               const ui::ImageModel& icon) {
  std::optional<size_t> index = menu_model_.GetIndexOfCommandId(command_id);
  if (!index.has_value())
    return;

  menu_model_.SetIcon(index.value(), icon);
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (toolkit_delegate_)
    toolkit_delegate_->RebuildMenu();
#endif
}

void RenderViewContextMenuBase::RemoveMenuItem(int command_id) {
  std::optional<size_t> index = menu_model_.GetIndexOfCommandId(command_id);
  if (!index.has_value())
    return;

  menu_model_.RemoveItemAt(index.value());
  if (toolkit_delegate_)
    toolkit_delegate_->RebuildMenu();
}

// Removes separators so that if there are two separators next to each other,
// only one of them remains.
void RenderViewContextMenuBase::RemoveAdjacentSeparators() {
  size_t num_items = menu_model_.GetItemCount();
  for (size_t index = num_items; index > 1; --index) {
    ui::MenuModel::ItemType curr_type = menu_model_.GetTypeAt(index - 1);
    ui::MenuModel::ItemType prev_type = menu_model_.GetTypeAt(index - 2);

    if (curr_type == ui::MenuModel::ItemType::TYPE_SEPARATOR &&
        prev_type == ui::MenuModel::ItemType::TYPE_SEPARATOR) {
      // We found adjacent separators, remove the one at the bottom.
      menu_model_.RemoveItemAt(index - 1);
    }
  }

  if (toolkit_delegate_)
    toolkit_delegate_->RebuildMenu();
}

void RenderViewContextMenuBase::RemoveSeparatorBeforeMenuItem(int command_id) {
  std::optional<size_t> index = menu_model_.GetIndexOfCommandId(command_id);
  // Ignore if command not found or if it's the first menu item.
  if (!index.has_value() || index == size_t{0})
    return;

  ui::MenuModel::ItemType prev_type = menu_model_.GetTypeAt(index.value() - 1);
  if (prev_type != ui::MenuModel::ItemType::TYPE_SEPARATOR)
    return;

  menu_model_.RemoveItemAt(index.value() - 1);

  if (toolkit_delegate_)
    toolkit_delegate_->RebuildMenu();
}

WebContents* RenderViewContextMenuBase::GetWebContents() const {
  return source_web_contents_;
}

BrowserContext* RenderViewContextMenuBase::GetBrowserContext() const {
  return browser_context_;
}

bool RenderViewContextMenuBase::AppendCustomItems() {
  size_t total_items = 0;
  AddCustomItemsToMenu(params_.custom_items, 0, &total_items, &custom_submenus_,
                       this, &menu_model_);
  return total_items > 0;
}

bool RenderViewContextMenuBase::IsCommandIdKnown(
    int id,
    bool* enabled) const {
  // If this command is added by one of our observers, we dispatch
  // it to the observer.
  for (auto& observer : observers_) {
    if (observer.IsCommandIdSupported(id)) {
      *enabled = observer.IsCommandIdEnabled(id);
      return true;
    }
  }

  // Custom items.
  if (IsContentCustomCommandId(id)) {
    *enabled = IsCustomItemEnabled(id);
    return true;
  }

  return false;
}

// Menu delegate functions -----------------------------------------------------

bool RenderViewContextMenuBase::IsCommandIdChecked(int id) const {
  // If this command is is added by one of our observers, we dispatch it to the
  // observer.
  for (auto& observer : observers_) {
    if (observer.IsCommandIdSupported(id))
      return observer.IsCommandIdChecked(id);
  }

  // Custom items.
  if (IsContentCustomCommandId(id))
    return IsCustomItemChecked(id);

  return false;
}

void RenderViewContextMenuBase::ExecuteCommand(int id, int event_flags) {
  command_executed_ = true;
  RecordUsedItem(id);

  // Notify all observers the command to be executed.
  for (auto& observer : observers_)
    observer.CommandWillBeExecuted(id);

  // If this command is is added by one of our observers, we dispatch
  // it to the observer.
  for (auto& observer : observers_) {
    if (observer.IsCommandIdSupported(id))
      return observer.ExecuteCommand(id);
  }

  // Process custom actions range.
  if (IsContentCustomCommandId(id)) {
    unsigned action = id - content_context_custom_first;
    const GURL& link_followed = params_.link_followed;
#if BUILDFLAG(ENABLE_PLUGINS)
    HandleAuthorizeAllPlugins();
#endif
    source_web_contents_->ExecuteCustomContextMenuCommand(action,
                                                          link_followed);
    return;
  }
  command_executed_ = false;
}

void RenderViewContextMenuBase::OnMenuWillShow(ui::SimpleMenuModel* source) {
  for (size_t i = 0; i < source->GetItemCount(); ++i) {
    if (source->IsVisibleAt(i) &&
        source->GetTypeAt(i) != ui::MenuModel::TYPE_SEPARATOR) {
      RecordShownItem(source->GetCommandIdAt(i),
                      source->GetTypeAt(i) == ui::MenuModel::TYPE_SUBMENU);
    }
  }

  // Ignore notifications from submenus.
  if (source != &menu_model_)
    return;

  source_web_contents_->SetShowingContextMenu(true);

  NotifyMenuShown();
}

void RenderViewContextMenuBase::MenuClosed(ui::SimpleMenuModel* source) {
  // Ignore notifications from submenus.
  if (source != &menu_model_)
    return;

  source_web_contents_->SetShowingContextMenu(false);
  source_web_contents_->NotifyContextMenuClosed(params_.link_followed);
  for (auto& observer : observers_) {
    observer.OnMenuClosed();
  }
}

RenderFrameHost* RenderViewContextMenuBase::GetRenderFrameHost() const {
  return RenderFrameHost::FromID(render_process_id_, render_frame_id_);
}

// Controller functions --------------------------------------------------------

void RenderViewContextMenuBase::OpenURL(const GURL& url,
                                        const GURL& referring_url,
                                        const url::Origin& initiator,
                                        WindowOpenDisposition disposition,
                                        ui::PageTransition transition) {
  OpenURLWithExtraHeaders(url, referring_url, initiator, disposition,
                          transition, "" /* extra_headers */,
                          true /* started_from_context_menu */);
}

void RenderViewContextMenuBase::OpenURLWithExtraHeaders(
    const GURL& url,
    const GURL& referring_url,
    const url::Origin& initiator,
    WindowOpenDisposition disposition,
    ui::PageTransition transition,
    const std::string& extra_headers,
    bool started_from_context_menu) {
  content::OpenURLParams open_url_params = GetOpenURLParamsWithExtraHeaders(
      url, referring_url, initiator, disposition, transition, extra_headers,
      started_from_context_menu);

  source_web_contents_->OpenURL(open_url_params,
                                /*navigation_handle_callback=*/{});
}

content::OpenURLParams
RenderViewContextMenuBase::GetOpenURLParamsWithExtraHeaders(
    const GURL& url,
    const GURL& referring_url,
    const url::Origin& initiator,
    WindowOpenDisposition disposition,
    ui::PageTransition transition,
    const std::string& extra_headers,
    bool started_from_context_menu) {
  // Do not send the referrer url to OTR windows. We still need the
  // |referring_url| to populate the |initiator_origin| below for browser UI.
  GURL referrer_url;
  if (disposition != WindowOpenDisposition::OFF_THE_RECORD) {
    referrer_url = referring_url.GetAsReferrer();
  }

  content::Referrer referrer = content::Referrer::SanitizeForRequest(
      url, content::Referrer(referrer_url, params_.referrer_policy));

  if (params_.link_url == url &&
      disposition != WindowOpenDisposition::OFF_THE_RECORD) {
    params_.link_followed = url;
  }

  OpenURLParams open_url_params(url, referrer, disposition, transition, false,
                                started_from_context_menu);
  if (!extra_headers.empty())
    open_url_params.extra_headers = extra_headers;

  open_url_params.source_render_process_id = render_process_id_;
  open_url_params.source_render_frame_id = render_frame_id_;

  open_url_params.initiator_frame_token = render_frame_token_;
  open_url_params.initiator_process_id = render_process_id_;
  open_url_params.initiator_origin = initiator;

  open_url_params.source_site_instance = site_instance_;

  if (disposition != WindowOpenDisposition::OFF_THE_RECORD)
    open_url_params.impression = params_.impression;

  return open_url_params;
}

bool RenderViewContextMenuBase::IsCustomItemChecked(int id) const {
  return IsCustomItemCheckedInternal(params_.custom_items, id);
}

bool RenderViewContextMenuBase::IsCustomItemEnabled(int id) const {
  return IsCustomItemEnabledInternal(params_.custom_items, id);
}
