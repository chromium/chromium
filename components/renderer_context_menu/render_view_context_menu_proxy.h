// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_RENDERER_CONTEXT_MENU_RENDER_VIEW_CONTEXT_MENU_PROXY_H_
#define COMPONENTS_RENDERER_CONTEXT_MENU_RENDER_VIEW_CONTEXT_MENU_PROXY_H_

#include <string>


namespace content {
class BrowserContext;
class RenderFrameHost;
class WebContents;
}

namespace ui {
class ImageModel;
class MenuModel;
}

// An interface that controls a RenderViewContextMenu instance from observers.
// This interface is designed mainly for controlling the instance while showing
// so we can add a context-menu item that takes long time to create its text,
// such as retrieving the item text from a server. The simplest usage is:
// 1. Adding an item with temporary text;
// 2. Posting a background task that creates the item text, and;
// 3. Calling UpdateMenuItem() in the callback function.
// The following snippet describes the simple usage that updates a context-menu
// item with this interface.
//
//   class MyTask : public net::URLFetcherDelegate {
//    public:
//     MyTask(RenderViewContextMenuProxy* proxy, int id)
//         : proxy_(proxy),
//           id_(id) {
//     }
//     virtual ~MyTask() {
//     }
//     virtual void OnURLFetchComplete(const net::URLFetcher* source,
//                                     const GURL& url,
//                                     const net::URLRequestStatus& status,
//                                     int response,
//                                     const net::ResponseCookies& cookies,
//                                     const std::string& data) {
//       bool enabled = response == 200;
//       const char* text = enabled ? "OK" : "ERROR";
//       proxy_->UpdateMenuItem(id_, enabled, base::ASCIIToUTF16(text));
//     }
//     void Start(const GURL* url, net::URLRequestContextGetter* context) {
//       fetcher_.reset(new URLFetcher(url, URLFetcher::GET, this));
//       fetcher_->SetRequestContext(context);
//       content::AssociateURLFetcherWithRenderView(
//           fetcher_.get(),
//           proxy_->GetRenderFrameHost()->GetSiteInstance()->GetSite(),
//           proxy_->GetRenderFrameHost()->GetProcess()->GetID(),
//           proxy_->GetRenderFrameHost()->GetRoutingID());
//       fetcher_->Start();
//     }
//
//    private:
//     URLFetcher fetcher_;
//     RenderViewContextMenuProxy* proxy_;
//     int id_;
//   };
//
//   void RenderViewContextMenu::AppendEditableItems() {
//     // Add a menu item with temporary text shown while we create the final
//     // text.
//     menu_model_.AddItemWithStringId(IDC_MY_ITEM, IDC_MY_TEXT);
//
//     // Start a task that creates the final text.
//     my_task_ = new MyTask(this, IDC_MY_ITEM);
//     my_task_->Start(...);
//   }
//
class RenderViewContextMenuProxy {
 public:
  // Add a menu item to a context menu.
  virtual void AddMenuItem(int command_id, const std::u16string& title) = 0;
  virtual void AddMenuItemWithIcon(int command_id,
                                   const std::u16string& title,
                                   const ui::ImageModel& icon) = 0;
  virtual void AddCheckItem(int command_id, const std::u16string& title) = 0;
  virtual void AddSeparator() = 0;

  // Add a submenu item to a context menu.
  virtual void AddSubMenu(int command_id,
                          const std::u16string& label,
                          ui::MenuModel* model) = 0;
  virtual void AddSubMenuWithStringIdAndIcon(int command_id,
                                             int message_id,
                                             ui::MenuModel* model,
                                             const ui::ImageModel& icon) = 0;

  // Update the status and text of the specified context-menu item.
  virtual void UpdateMenuItem(int command_id,
                              bool enabled,
                              bool hidden,
                              const std::u16string& title) = 0;

  // Update the icon of the specified context-menu item.
  virtual void UpdateMenuIcon(int command_id, const ui::ImageModel& icon) = 0;

  // Remove the specified context-menu item.
  virtual void RemoveMenuItem(int command_id) = 0;

  // Removes separators so that any adjacent duplicates are reduced to only 1.
  virtual void RemoveAdjacentSeparators() = 0;

  // Removes separator (if any) before the specified context menu item.
  virtual void RemoveSeparatorBeforeMenuItem(int command_id) = 0;

  // Add spell check service item to the context menu.
  virtual void AddSpellCheckServiceItem(bool is_checked) = 0;

  // Add accessibility labels service item to the context menu.
  virtual void AddAccessibilityLabelsServiceItem(bool is_checked) = 0;

  // Retrieve the given associated objects with a context menu.
  virtual content::RenderFrameHost* GetRenderFrameHost() const = 0;
  virtual content::WebContents* GetWebContents() const = 0;
  virtual content::BrowserContext* GetBrowserContext() const = 0;
};

#endif  // COMPONENTS_RENDERER_CONTEXT_MENU_RENDER_VIEW_CONTEXT_MENU_PROXY_H_
