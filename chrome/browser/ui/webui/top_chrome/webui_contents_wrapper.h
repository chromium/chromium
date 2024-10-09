// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_TOP_CHROME_WEBUI_CONTENTS_WRAPPER_H_
#define CHROME_BROWSER_UI_WEBUI_TOP_CHROME_WEBUI_CONTENTS_WRAPPER_H_

#include <memory>
#include <utility>

#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_observer.h"
#include "chrome/browser/ui/webui/top_chrome/top_chrome_web_ui_controller.h"
#include "chrome/browser/ui/webui/top_chrome/top_chrome_webui_config.h"
#include "chrome/browser/ui/webui_name_variants.h"
#include "content/public/browser/file_select_listener.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/referrer.h"
#include "third_party/blink/public/mojom/page/draggable_region.mojom.h"
#include "ui/base/models/menu_model.h"

// WebUIContentsWrapper wraps a WebContents that hosts a top chrome WebUI.
// This class notifies the Host when it should be shown or hidden via ShowUI()
// and CloseUI() in addition to passing through resize events so the Host can
// adjust bounds accordingly.
class WebUIContentsWrapper : public content::WebContentsDelegate,
                             public content::WebContentsObserver,
                             public ProfileObserver,
                             public TopChromeWebUIController::Embedder {
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
        const input::NativeWebKeyboardEvent& event);
    virtual bool HandleContextMenu(content::RenderFrameHost& render_frame_host,
                                   const content::ContextMenuParams& params);
    virtual void RequestMediaAccessPermission(
        content::WebContents* web_contents,
        const content::MediaStreamRequest& request,
        content::MediaResponseCallback callback) {}
    virtual content::WebContents* OpenURLFromTab(
        content::WebContents* source,
        const content::OpenURLParams& params,
        base::OnceCallback<void(content::NavigationHandle&)>
            navigation_handle_callback);
    virtual void RunFileChooser(
        content::RenderFrameHost* render_frame_host,
        scoped_refptr<content::FileSelectListener> listener,
        const blink::mojom::FileChooserParams& params) {}
    virtual void DraggableRegionsChanged(
        const std::vector<blink::mojom::DraggableRegionPtr>& regions,
        content::WebContents* contents) {}
    virtual void SetContentsBounds(content::WebContents* source,
                                   const gfx::Rect& bounds) {}
  };

  WebUIContentsWrapper(const GURL& webui_url,
                       Profile* profile,
                       int task_manager_string_id,
                       bool webui_resizes_host,
                       bool esc_closes_ui,
                       bool supports_draggable_regions,
                       const std::string& webui_name);
  ~WebUIContentsWrapper() override;

  // content::WebContentsDelegate:
  void ResizeDueToAutoResize(content::WebContents* source,
                             const gfx::Size& new_size) override;
  content::KeyboardEventProcessingResult PreHandleKeyboardEvent(
      content::WebContents* source,
      const input::NativeWebKeyboardEvent& event) override;
  bool HandleKeyboardEvent(content::WebContents* source,
                           const input::NativeWebKeyboardEvent& event) override;
  bool HandleContextMenu(content::RenderFrameHost& render_frame_host,
                         const content::ContextMenuParams& params) override;
  std::unique_ptr<content::EyeDropper> OpenEyeDropper(
      content::RenderFrameHost* frame,
      content::EyeDropperListener* listener) override;
  void RequestMediaAccessPermission(
      content::WebContents* web_contents,
      const content::MediaStreamRequest& request,
      content::MediaResponseCallback callback) override;
  content::WebContents* OpenURLFromTab(
      content::WebContents* source,
      const content::OpenURLParams& params,
      base::OnceCallback<void(content::NavigationHandle&)>
          navigation_handle_callback) override;
  void RunFileChooser(content::RenderFrameHost* render_frame_host,
                      scoped_refptr<content::FileSelectListener> listener,
                      const blink::mojom::FileChooserParams& params) override;
  void DraggableRegionsChanged(
      const std::vector<blink::mojom::DraggableRegionPtr>& regions,
      content::WebContents* contents) override;
  void SetContentsBounds(content::WebContents* source,
                         const gfx::Rect& bounds) override;

  // content::WebContentsObserver:
  void PrimaryPageChanged(content::Page& page) override;
  void PrimaryMainFrameRenderProcessGone(
      base::TerminationStatus status) override;

  // ProfileObserver:
  void OnProfileWillBeDestroyed(Profile* profile) override;

  // TopChromeWebUIController::Embedder:
  void CloseUI() override;
  void ShowUI() override;
  void ShowContextMenu(gfx::Point point,
                       std::unique_ptr<ui::MenuModel> menu_model) override;
  void HideContextMenu() override;

  // Reloads the WebContents hosting the WebUI.
  virtual void ReloadWebContents() = 0;

  // True if the host can show the contents immediately.
  bool is_ready_to_show() const { return is_ready_to_show_; }

  bool supports_draggable_regions() const {
    return supports_draggable_regions_;
  }

  // Gets weak ptr to prevent UAF.
  virtual base::WeakPtr<WebUIContentsWrapper> GetWeakPtr() = 0;

  base::WeakPtr<WebUIContentsWrapper::Host> GetHost();
  void SetHost(base::WeakPtr<WebUIContentsWrapper::Host> host);

  content::WebContents* web_contents() { return web_contents_.get(); }

  void SetWebContentsForTesting(
      std::unique_ptr<content::WebContents> web_contents);

 private:
  // If true will allow the wrapped WebContents to automatically resize its
  // RenderWidgetHostView and send back updates to `Host` for the new size.
  const bool webui_resizes_host_;

  bool is_ready_to_show_ = false;
  // If true will cause the ESC key to close the UI during pre-handling.
  const bool esc_closes_ui_;

  // Set if the wrapped contents should enable web platform draggable regions,
  // tagged using the -webkit-app-region CSS property.
  const bool supports_draggable_regions_;
  // The most recent draggable region set by DraggableRegionsChanged().
  std::optional<std::vector<blink::mojom::DraggableRegionPtr>>
      draggable_regions_;

  base::ScopedObservation<Profile, ProfileObserver> profile_observation_{this};

  base::WeakPtr<WebUIContentsWrapper::Host> host_;
  std::unique_ptr<content::WebContents> web_contents_;
};

// WebUIContentsWrapperT is designed to be paired with the WebUIController
// subclass used by the hosted WebUI. This type information allows compile time
// checking that the WebUIController subclasses TopChromeWebUIController as
// expected.
// Upon the construction of this class, its wrapped web contents has started the
// navigation to `webui_url`, and `GetWebUIController()` is guaranteed to return
// a non-null pointer.
template <typename T>
class WebUIContentsWrapperT : public WebUIContentsWrapper {
 public:
  // TODO(tluk): Consider introducing init params to avoid further cluttering
  // constructor params.
  WebUIContentsWrapperT(const GURL& webui_url,
                        Profile* profile,
                        int task_manager_string_id,
                        bool esc_closes_ui = true,
                        bool supports_draggable_regions = false)
      : WebUIContentsWrapper(webui_url,
                             profile,
                             task_manager_string_id,
                             TopChromeWebUIConfig::From(profile, webui_url)
                                 ->ShouldAutoResizeHost(),
                             esc_closes_ui,
                             supports_draggable_regions,
                             T::GetWebUIName()),
        webui_url_(webui_url) {
    static_assert(views_metrics::IsValidWebUIName("." + T::GetWebUIName()));

    CHECK(GetWebUIController());
    GetWebUIController()->set_embedder(weak_ptr_factory_.GetWeakPtr());
  }

  void ReloadWebContents() override {
    web_contents()->GetController().LoadURL(webui_url_, content::Referrer(),
                                            ui::PAGE_TRANSITION_AUTO_TOPLEVEL,
                                            std::string());
    // WARNING: with RenderDocument enabled, every navigation creates a new
    // WebUI controller. If this is not the initial navigation,
    // `GetWebUIController()` will return the old controller.
    // TODO(crbug.com/40615943): provide an content API to access the new
    // WebUI object.
    if (T* webui_controller = GetWebUIController()) {
      webui_controller->set_embedder(weak_ptr_factory_.GetWeakPtr());
    }
  }

  // May return null.
  T* GetWebUIController() {
    content::WebUI* const webui = web_contents()->GetWebUI();
    return webui && webui->GetController()
               ? webui->GetController()->template GetAs<T>()
               : nullptr;
  }

  base::WeakPtr<WebUIContentsWrapper> GetWeakPtr() override {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  const GURL webui_url_;
  base::WeakPtrFactory<WebUIContentsWrapper> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_TOP_CHROME_WEBUI_CONTENTS_WRAPPER_H_
