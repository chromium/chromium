// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_RENDERER_CONTEXT_MENU_RENDER_VIEW_CONTEXT_MENU_BASE_H_
#define COMPONENTS_RENDERER_CONTEXT_MENU_RENDER_VIEW_CONTEXT_MENU_BASE_H_

#include <stddef.h>

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "components/renderer_context_menu/context_menu_content_type.h"
#include "components/renderer_context_menu/render_view_context_menu_observer.h"
#include "components/renderer_context_menu/render_view_context_menu_proxy.h"
#include "content/public/browser/context_menu_params.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/site_instance.h"
#include "ppapi/buildflags/buildflags.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"

namespace content {
class RenderFrameHost;
class WebContents;
}

class RenderViewContextMenuBase : public ui::SimpleMenuModel::Delegate,
                                  public RenderViewContextMenuProxy {
 public:
  // A delegate interface to communicate with the toolkit used by
  // the embedder.
  class ToolkitDelegate {
   public:
    virtual ~ToolkitDelegate() = default;
    // Initialize the toolkit's menu.
    virtual void Init(ui::SimpleMenuModel* menu_model) = 0;

    virtual void Cancel() = 0;

    // Updates the actual menu items controlled by the toolkit.
    virtual void UpdateMenuItem(int command_id,
                                bool enabled,
                                bool hidden,
                                const std::u16string& title) {}

    // Recreates the menu using the |menu_model_|.
    virtual void RebuildMenu(){}
  };

  static const size_t kMaxSelectionTextLength;

  static void SetContentCustomCommandIdRange(int first, int last);

  // Convert a command ID so that it fits within the range for
  // content context menu.
  static int ConvertToContentCustomCommandId(int id);

  // True if the given id is the one generated for content context menu.
  static bool IsContentCustomCommandId(int id);

  RenderViewContextMenuBase(content::RenderFrameHost& render_frame_host,
                            const content::ContextMenuParams& params);

  RenderViewContextMenuBase(const RenderViewContextMenuBase&) = delete;
  RenderViewContextMenuBase& operator=(const RenderViewContextMenuBase&) =
      delete;

  ~RenderViewContextMenuBase() override;

  // Displays the menu.
  // Different platform will have their own implementation.
  virtual void Show() = 0;

  // Initializes the context menu.
  void Init();

  // Programmatically closes the context menu.
  void Cancel();

  const ui::SimpleMenuModel& menu_model() const { return menu_model_; }
  const content::ContextMenuParams& params() const { return params_; }

  // Returns true if the specified command id is known and valid for
  // this menu. If the command is known |enabled| is set to indicate
  // if the command is enabled.
  bool IsCommandIdKnown(int command_id, bool* enabled) const;

  // SimpleMenuModel::Delegate implementation.
  bool IsCommandIdChecked(int command_id) const override;
  void ExecuteCommand(int command_id, int event_flags) override;
  void OnMenuWillShow(ui::SimpleMenuModel* source) override;
  void MenuClosed(ui::SimpleMenuModel* source) override;

  // RenderViewContextMenuProxy implementation.
  void AddMenuItem(int command_id, const std::u16string& title) override;
  void AddMenuItemWithIcon(int command_id,
                           const std::u16string& title,
                           const ui::ImageModel& icon) override;
  void AddCheckItem(int command_id, const std::u16string& title) override;
  void AddSeparator() override;
  void AddSubMenu(int command_id,
                  const std::u16string& label,
                  ui::MenuModel* model) override;
  void AddSubMenuWithStringIdAndIcon(int command_id,
                                     int message_id,
                                     ui::MenuModel* model,
                                     const ui::ImageModel& icon) override;
  void UpdateMenuItem(int command_id,
                      bool enabled,
                      bool hidden,
                      const std::u16string& title) override;
  void UpdateMenuIcon(int command_id, const ui::ImageModel& icon) override;
  void RemoveMenuItem(int command_id) override;
  void RemoveAdjacentSeparators() override;
  void RemoveSeparatorBeforeMenuItem(int command_id) override;
  content::WebContents* GetWebContents() const override;
  content::BrowserContext* GetBrowserContext() const override;

  // May return nullptr if the frame was deleted while the menu was open.
  content::RenderFrameHost* GetRenderFrameHost() const override;

 protected:
  friend class RenderViewContextMenuTest;
  friend class RenderViewContextMenuPrefsTest;

  void set_content_type(std::unique_ptr<ContextMenuContentType> content_type) {
    content_type_ = std::move(content_type);
  }

  void set_toolkit_delegate(std::unique_ptr<ToolkitDelegate> delegate) {
    toolkit_delegate_ = std::move(delegate);
  }

  ToolkitDelegate* toolkit_delegate() {
    return toolkit_delegate_.get();
  }

  // TODO(oshima): Make these methods delegate.

  // Menu Construction.
  virtual void InitMenu();

  // Increments histogram value for used items specified by |id|.
  virtual void RecordUsedItem(int id) = 0;

  // Increments histogram value for visible context menu item specified by |id|.
  virtual void RecordShownItem(int id, bool is_submenu) = 0;

#if BUILDFLAG(ENABLE_PLUGINS)
  virtual void HandleAuthorizeAllPlugins() = 0;
#endif

  // Subclasses should send notification.
  virtual void NotifyMenuShown() = 0;

  // TODO(oshima): Remove this.
  virtual void AppendPlatformEditableItems() {}

  bool IsCustomItemChecked(int id) const;
  bool IsCustomItemEnabled(int id) const;

  // Opens the specified URL.
  void OpenURL(const GURL& url,
               const GURL& referrer,
               const url::Origin& initiator,
               WindowOpenDisposition disposition,
               ui::PageTransition transition);

  // Opens the specified URL with extra headers.
  virtual void OpenURLWithExtraHeaders(const GURL& url,
                                       const GURL& referrer,
                                       const url::Origin& initiator,
                                       WindowOpenDisposition disposition,
                                       ui::PageTransition transition,
                                       const std::string& extra_headers,
                                       bool started_from_context_menu);

  // Populates OpenURLParams for opening the specified URL string in a new tab
  // with the extra headers.
  content::OpenURLParams GetOpenURLParamsWithExtraHeaders(
      const GURL& url,
      const GURL& referring_url,
      const url::Origin& initiator,
      WindowOpenDisposition disposition,
      ui::PageTransition transition,
      const std::string& extra_headers,
      bool started_from_context_menu);

  content::ContextMenuParams params_;
  const raw_ptr<content::WebContents, DanglingUntriaged> source_web_contents_;
  const raw_ptr<content::BrowserContext, DanglingUntriaged> browser_context_;

  ui::SimpleMenuModel menu_model_;

  // Renderer's frame id.
  const int render_frame_id_;

  // Renderer's frame token.
  const blink::LocalFrameToken render_frame_token_;

  // The RenderFrameHost's IDs.
  const int render_process_id_;

  // Renderer's frame SiteInstance.
  scoped_refptr<content::SiteInstance> site_instance_;

  // Our observers.
  mutable base::ObserverList<
      RenderViewContextMenuObserver>::UncheckedAndDanglingUntriaged observers_;

  // Whether a command has been executed. Used to track whether menu observers
  // should be notified of menu closing without execution.
  bool command_executed_;

  std::unique_ptr<ContextMenuContentType> content_type_;

 private:
  bool AppendCustomItems();

  std::unique_ptr<ToolkitDelegate> toolkit_delegate_;

  std::vector<std::unique_ptr<ui::SimpleMenuModel>> custom_submenus_;
};

#endif  // COMPONENTS_RENDERER_CONTEXT_MENU_RENDER_VIEW_CONTEXT_MENU_BASE_H_
