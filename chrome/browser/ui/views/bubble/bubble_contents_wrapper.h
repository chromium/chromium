// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_BUBBLE_BUBBLE_CONTENTS_WRAPPER_H_
#define CHROME_BROWSER_UI_VIEWS_BUBBLE_BUBBLE_CONTENTS_WRAPPER_H_

#include <memory>
#include <utility>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/extensions/chrome_extension_web_contents_observer.h"
#include "chrome/browser/task_manager/web_contents_tags.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/common/referrer.h"
#include "ui/base/models/menu_model.h"
#include "ui/webui/mojo_bubble_web_ui_controller.h"

namespace content {
class BrowserContext;
}  // namespace content

// BubbleContentsWrapper wraps a WebContents that hosts a bubble WebUI (ie a
// WebUI with WebUIController subclassing MojoBubbleWebUIController). This class
// notifies the Host when it should be shown or hidden via ShowUI() and
// CloseUI() in addition to passing through resize events so the Host can adjust
// bounds accordingly.
class BubbleContentsWrapper : public content::WebContentsDelegate,
                              public content::WebContentsObserver,
                              public ui::MojoBubbleWebUIController::Embedder {
 public:
  class Host {
   public:
    virtual void CloseUI() = 0;
    virtual void ShowUI() = 0;
    virtual void ShowCustomContextMenu(
        gfx::Point point,
        std::unique_ptr<ui::MenuModel> menu_model) {}
    virtual void HideCustomContextMenu() {}
    virtual void ResizeDueToAutoResize(content::WebContents* source,
                                       const gfx::Size& new_size) {}
    virtual bool HandleKeyboardEvent(
        content::WebContents* source,
        const content::NativeWebKeyboardEvent& event);
  };

  BubbleContentsWrapper(const GURL& webui_url,
                        content::BrowserContext* browser_context,
                        int task_manager_string_id,
                        bool webui_resizes_host,
                        bool esc_closes_ui);
  ~BubbleContentsWrapper() override;

  // content::WebContentsDelegate:
  void ResizeDueToAutoResize(content::WebContents* source,
                             const gfx::Size& new_size) override;
  content::KeyboardEventProcessingResult PreHandleKeyboardEvent(
      content::WebContents* source,
      const content::NativeWebKeyboardEvent& event) override;
  bool HandleKeyboardEvent(
      content::WebContents* source,
      const content::NativeWebKeyboardEvent& event) override;
  bool HandleContextMenu(content::RenderFrameHost& render_frame_host,
                         const content::ContextMenuParams& params) override;
  std::unique_ptr<content::EyeDropper> OpenEyeDropper(
      content::RenderFrameHost* frame,
      content::EyeDropperListener* listener) override;

  // content::WebContentsObserver:
  void PrimaryPageChanged(content::Page& page) override;
  void PrimaryMainFrameRenderProcessGone(
      base::TerminationStatus status) override;

  // MojoBubbleWebUIController::Embedder:
  void CloseUI() override;
  void ShowUI() override;
  void ShowContextMenu(gfx::Point point,
                       std::unique_ptr<ui::MenuModel> menu_model) override;
  void HideContextMenu() override;

  // Reloads the WebContents hosting the WebUI.
  virtual void ReloadWebContents() = 0;

  base::WeakPtr<BubbleContentsWrapper::Host> GetHost();
  void SetHost(base::WeakPtr<BubbleContentsWrapper::Host> host);

  content::WebContents* web_contents() { return web_contents_.get(); }

  void SetWebContentsForTesting(
      std::unique_ptr<content::WebContents> web_contents);

 private:
  // If true will allow the wrapped WebContents to automatically resize its
  // RenderWidgetHostView and send back updates to `Host` for the new size.
  const bool webui_resizes_host_;
  // If true will cause the ESC key to close the UI during pre-handling.
  const bool esc_closes_ui_;
  base::WeakPtr<BubbleContentsWrapper::Host> host_;
  std::unique_ptr<content::WebContents> web_contents_;
};

// BubbleContentsWrapperT is designed to be paired with the WebUIController
// subclass used by the hosted WebUI. This type information allows compile time
// checking that the WebUIController subclasses MojoBubbleWebUIController as
// expected.
template <typename T>
class BubbleContentsWrapperT : public BubbleContentsWrapper {
 public:
  BubbleContentsWrapperT(const GURL& webui_url,
                         content::BrowserContext* browser_context,
                         int task_manager_string_id,
                         bool webui_resizes_host = true,
                         bool esc_closes_ui = true)
      : BubbleContentsWrapper(webui_url,
                              browser_context,
                              task_manager_string_id,
                              webui_resizes_host,
                              esc_closes_ui),
        webui_url_(webui_url) {}

  void ReloadWebContents() override {
    web_contents()->GetController().LoadURL(webui_url_, content::Referrer(),
                                            ui::PAGE_TRANSITION_AUTO_TOPLEVEL,
                                            std::string());
    // Depends on the WebUIController object being constructed synchronously
    // when the navigation is started in LoadInitialURL(). The WebUIController
    // may not be defined at this point if the content code encounteres an
    // error during navigation so check here to ensure the pointer is valid.
    if (T* webui_controller = GetWebUIController())
      webui_controller->set_embedder(weak_ptr_factory_.GetWeakPtr());
  }

  // May return null.
  T* GetWebUIController() {
    content::WebUI* const webui = web_contents()->GetWebUI();
    return webui && webui->GetController()
               ? webui->GetController()->template GetAs<T>()
               : nullptr;
  }

 private:
  const GURL webui_url_;
  base::WeakPtrFactory<BubbleContentsWrapper> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_BUBBLE_BUBBLE_CONTENTS_WRAPPER_H_
