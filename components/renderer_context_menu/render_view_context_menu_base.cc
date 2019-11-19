// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/renderer_context_menu/render_view_context_menu_base.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/logging.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/menu_item.h"
#include "ppapi/buildflags/buildflags.h"

using blink::WebString;
using blink::WebURL;
using content::BrowserContext;
using content::OpenURLParams;
using content::RenderFrameHost;
using content::RenderViewHost;
using content::WebContents;

namespace {

// The (inclusive) range of command IDs reserved for content's custom menus.
int content_context_custom_first = -1;
int content_context_custom_last = -1;

bool IsCustomItemEnabledInternal(const std::vector<content::MenuItem>& items,
                                 int id) {
  DCHECK(RenderViewContextMenuBase::IsContentCustomCommandId(id));
  for (size_t i = 0; i < items.size(); ++i) {
    int action_id = RenderViewContextMenuBase::ConvertToContentCustomCommandId(
        items[i].action);
    if (action_id == id)
      return items[i].enabled;
    if (items[i].type == content::MenuItem::SUBMENU) {
      if (IsCustomItemEnabledInternal(items[i].submenu, id))
        return true;
    }
  }
  return false;
}

bool IsCustomItemCheckedInternal(const std::vector<content::MenuItem>& items,
                                 int id) {
  DCHECK(RenderViewContextMenuBase::IsContentCustomCommandId(id));
  for (size_t i = 0; i < items.size(); ++i) {
    int action_id = RenderViewContextMenuBase::ConvertToContentCustomCommandId(
        items[i].action);
    if (action_id == id)
      return items[i].checked;
    if (items[i].type == content::MenuItem::SUBMENU) {
      if (IsCustomItemCheckedInternal(items[i].submenu, id))
        return true;
    }
  }
  return false;
}

const size_t kMaxCustomMenuDepth = 5;
const size_t kMaxCustomMenuTotalItems = 1000;

void AddCustomItemsToMenu(
    const std::vector<content::MenuItem>& items,
    size_t depth,
    size_t* total_items,
    std::vector<std::unique_ptr<ui::SimpleMenuModel>>* submenus,
    ui::SimpleMenuModel::Delegate* delegate,
    ui::SimpleMenuModel* menu_model) {
  if (depth > kMaxCustomMenuDepth) {
    LOG(ERROR) << "Custom menu too deeply nested.";
    return;
  }
  for (size_t i = 0; i < items.size(); ++i) {
    int command_id = RenderViewContextMenuBase::ConvertToContentCustomCommandId(
        items[i].action);
    if (!RenderViewContextMenuBase::IsContentCustomCommandId(command_id)) {
      LOG(ERROR) << "Custom menu action value out of range.";
      return;
    }
    if (*total_items >= kMaxCustomMenuTotalItems) {
      LOG(ERROR) << "Custom menu too large (too many items).";
      return;
    }
    (*total_items)++;
    switch (items[i].type) {
      case content::MenuItem::OPTION:
        menu_model->AddItem(
            RenderViewContextMenuBase::ConvertToContentCustomCommandId(
                items[i].action),
            items[i].label);
        break;
      case content::MenuItem::CHECKABLE_OPTION:
        menu_model->AddCheckItem(
            RenderViewContextMenuBase::ConvertToContentCustomCommandId(
                items[i].action),
            items[i].label);
        break;
      case content::MenuItem::GROUP:
        // TODO(viettrungluu): I don't know what this is supposed to do.
        NOTREACHED();
        break;
      case content::MenuItem::SEPARATOR:
        menu_model->AddSeparator(ui::NORMAL_SEPARATOR);
        break;
      case content::MenuItem::SUBMENU: {
        ui::SimpleMenuModel* submenu = new ui::SimpleMenuModel(delegate);
        submenus->push_back(base::WrapUnique(submenu));
        AddCustomItemsToMenu(items[i].submenu, depth + 1, total_items, submenus,
                             delegate, submenu);
        menu_model->AddSubMenu(
            RenderViewContextMenuBase::ConvertToContentCustomCommandId(
                items[i].action),
            items[i].label,
            submenu);
        break;
      }
      default:
        NOTREACHED();
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
    content::RenderFrameHost* render_frame_host,
    const content::ContextMenuParams& params)
    : params_(params),
      source_web_contents_(WebContents::FromRenderFrameHost(render_frame_host)),
      browser_context_(source_web_contents_->GetBrowserContext()),
      menu_model_(this),
      render_frame_id_(render_frame_host->GetRoutingID()),
      render_process_id_(render_frame_host->GetProcess()->GetID()),
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
                                            const base::string16& title) {
  menu_model_.AddItem(command_id, title);
}

void RenderViewContextMenuBase::AddMenuItemWithIcon(
    int command_id,
    const base::string16& title,
    const gfx::ImageSkia& image) {
  menu_model_.AddItemWithIcon(command_id, title, image);
}

void RenderViewContextMenuBase::AddMenuItemWithIcon(
    int command_id,
    const base::string16& title,
    const gfx::VectorIcon& icon) {
  menu_model_.AddItemWithIcon(command_id, title, icon);
}

void RenderViewContextMenuBase::AddCheckItem(int command_id,
                                         const base::string16& title) {
  menu_model_.AddCheckItem(command_id, title);
}

void RenderViewContextMenuBase::AddSeparator() {
  menu_model_.AddSeparator(ui::NORMAL_SEPARATOR);
}

void RenderViewContextMenuBase::AddSubMenu(int command_id,
                                       const base::string16& label,
                                       ui::MenuModel* model) {
  menu_model_.AddSubMenu(command_id, label, model);
}

void RenderViewContextMenuBase::AddSubMenuWithStringIdAndIcon(
    int command_id,
    int message_id,
    ui::MenuModel* model,
    const gfx::ImageSkia& image) {
  menu_model_.AddSubMenuWithStringIdAndIcon(command_id, message_id, model,
                                            image);
}

void RenderViewContextMenuBase::AddSubMenuWithStringIdAndIcon(
    int command_id,
    int message_id,
    ui::MenuModel* model,
    const gfx::VectorIcon& icon) {
  menu_model_.AddSubMenuWithStringIdAndIcon(command_id, message_id, model,
                                            icon);
}

void RenderViewContextMenuBase::UpdateMenuItem(int command_id,
                                           bool enabled,
                                           bool hidden,
                                           const base::string16& label) {
  int index = menu_model_.GetIndexOfCommandId(command_id);
  if (index == -1)
    return;

  menu_model_.SetLabel(index, label);
  menu_model_.SetEnabledAt(index, enabled);
  menu_model_.SetVisibleAt(index, !hidden);
  if (toolkit_delegate_)
    toolkit_delegate_->RebuildMenu();
}

void RenderViewContextMenuBase::UpdateMenuIcon(int command_id,
                                               const gfx::Image& image) {
  int index = menu_model_.GetIndexOfCommandId(command_id);
  if (index == -1)
    return;

  menu_model_.SetIcon(index, image);
#if defined(OS_CHROMEOS)
  if (toolkit_delegate_)
    toolkit_delegate_->RebuildMenu();
#endif
}

void RenderViewContextMenuBase::RemoveMenuItem(int command_id) {
  int index = menu_model_.GetIndexOfCommandId(command_id);
  if (index == -1)
    return;

  menu_model_.RemoveItemAt(index);
  if (toolkit_delegate_)
    toolkit_delegate_->RebuildMenu();
}

// Removes separators so that if there are two separators next to each other,
// only one of them remains.
void RenderViewContextMenuBase::RemoveAdjacentSeparators() {
  int num_items = menu_model_.GetItemCount();
  for (int index = num_items - 1; index > 0; --index) {
    ui::MenuModel::ItemType curr_type = menu_model_.GetTypeAt(index);
    ui::MenuModel::ItemType prev_type = menu_model_.GetTypeAt(index - 1);

    if (curr_type == ui::MenuModel::ItemType::TYPE_SEPARATOR &&
        prev_type == ui::MenuModel::ItemType::TYPE_SEPARATOR) {
      // We found adjacent separators, remove the one at the bottom.
      menu_model_.RemoveItemAt(index);
    }
  }

  if (toolkit_delegate_)
    toolkit_delegate_->RebuildMenu();
}

RenderViewHost* RenderViewContextMenuBase::GetRenderViewHost() const {
  return source_web_contents_->GetRenderViewHost();
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

  // If this command is is added by one of our observers, we dispatch
  // it to the observer.
  for (auto& observer : observers_) {
    if (observer.IsCommandIdSupported(id))
      return observer.ExecuteCommand(id);
  }

  // Process custom actions range.
  if (IsContentCustomCommandId(id)) {
    unsigned action = id - content_context_custom_first;
    const content::CustomContextMenuContext& context = params_.custom_context;
#if BUILDFLAG(ENABLE_PLUGINS)
    if (context.request_id && !context.is_pepper_menu)
      HandleAuthorizeAllPlugins();
#endif
    source_web_contents_->ExecuteCustomContextMenuCommand(action, context);
    return;
  }
  command_executed_ = false;
}

void RenderViewContextMenuBase::OnMenuWillShow(ui::SimpleMenuModel* source) {
  for (int i = 0; i < source->GetItemCount(); ++i) {
    if (source->IsVisibleAt(i) &&
        source->GetTypeAt(i) != ui::MenuModel::TYPE_SEPARATOR &&
        source->GetTypeAt(i) != ui::MenuModel::TYPE_SUBMENU) {
      RecordShownItem(source->GetCommandIdAt(i));
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
  source_web_contents_->NotifyContextMenuClosed(params_.custom_context);
}

RenderFrameHost* RenderViewContextMenuBase::GetRenderFrameHost() {
  return RenderFrameHost::FromID(render_process_id_, render_frame_id_);
}

// Controller functions --------------------------------------------------------

void RenderViewContextMenuBase::OpenURL(const GURL& url,
                                        const GURL& referring_url,
                                        WindowOpenDisposition disposition,
                                        ui::PageTransition transition) {
  OpenURLWithExtraHeaders(url, referring_url, disposition, transition,
                          "" /* extra_headers */,
                          false /* started_from_context_menu */);
}

void RenderViewContextMenuBase::OpenURLWithExtraHeaders(
    const GURL& url,
    const GURL& referring_url,
    WindowOpenDisposition disposition,
    ui::PageTransition transition,
    const std::string& extra_headers,
    bool started_from_context_menu) {
  content::Referrer referrer = content::Referrer::SanitizeForRequest(
      url,
      content::Referrer(referring_url.GetAsReferrer(),
                        params_.referrer_policy));

  if (params_.link_url == url &&
      disposition != WindowOpenDisposition::OFF_THE_RECORD)
    params_.custom_context.link_followed = url;

  OpenURLParams open_url_params(url, referrer, disposition, transition, false,
                                started_from_context_menu);
  if (!extra_headers.empty())
    open_url_params.extra_headers = extra_headers;

  open_url_params.source_render_process_id = render_process_id_;
  open_url_params.source_render_frame_id = render_frame_id_;

  source_web_contents_->OpenURL(open_url_params);
}

bool RenderViewContextMenuBase::IsCustomItemChecked(int id) const {
  return IsCustomItemCheckedInternal(params_.custom_items, id);
}

bool RenderViewContextMenuBase::IsCustomItemEnabled(int id) const {
  return IsCustomItemEnabledInternal(params_.custom_items, id);
}
