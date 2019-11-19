// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_CHROME_RENDER_FRAME_OBSERVER_H_
#define CHROME_RENDERER_CHROME_RENDER_FRAME_OBSERVER_H_

#include "base/macros.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "chrome/common/chrome_render_frame.mojom.h"
#include "chrome/common/prerender_types.h"
#include "content/public/renderer/render_frame_observer.h"
#include "mojo/public/cpp/bindings/associated_receiver_set.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"

namespace gfx {
class Size;
}

namespace safe_browsing {
class PhishingClassifierDelegate;
}

namespace translate {
class TranslateHelper;
}

namespace web_cache {
class WebCacheImpl;
}

// This class holds the Chrome specific parts of RenderFrame, and has the same
// lifetime.
class ChromeRenderFrameObserver : public content::RenderFrameObserver,
                                  public chrome::mojom::ChromeRenderFrame {
 public:
  ChromeRenderFrameObserver(content::RenderFrame* render_frame,
                            web_cache::WebCacheImpl* web_cache_impl);
  ~ChromeRenderFrameObserver() override;

  service_manager::BinderRegistry* registry() { return &registry_; }
  blink::AssociatedInterfaceRegistry* associated_interfaces() {
    return &associated_interfaces_;
  }

 private:
  enum TextCaptureType { PRELIMINARY_CAPTURE, FINAL_CAPTURE };

  // RenderFrameObserver implementation.
  void OnInterfaceRequestForFrame(
      const std::string& interface_name,
      mojo::ScopedMessagePipeHandle* interface_pipe) override;
  bool OnAssociatedInterfaceRequestForFrame(
      const std::string& interface_name,
      mojo::ScopedInterfaceEndpointHandle* handle) override;
  bool OnMessageReceived(const IPC::Message& message) override;
  void ReadyToCommitNavigation(
      blink::WebDocumentLoader* document_loader) override;
  void DidFinishLoad() override;
  void DidCreateNewDocument() override;
  void DidCommitProvisionalLoad(bool is_same_document_navigation,
                                ui::PageTransition transition) override;
  void DidClearWindowObject() override;
  void DidMeaningfulLayout(blink::WebMeaningfulLayout layout_type) override;
  void OnDestruct() override;

  // IPC handlers
  void OnSetIsPrerendering(prerender::PrerenderMode mode,
                           const std::string& histogram_prefix);
  void OnRequestThumbnailForContextNode(
      int thumbnail_min_area_pixels,
      const gfx::Size& thumbnail_max_size_pixels,
      int callback_id);
  void OnPrintNodeUnderContextMenu();
  void OnSetClientSidePhishingDetection(bool enable_phishing_detection);

  // chrome::mojom::ChromeRenderFrame:
  void SetWindowFeatures(
      blink::mojom::WindowFeaturesPtr window_features) override;
  void ExecuteWebUIJavaScript(const base::string16& javascript) override;
  void RequestThumbnailForContextNode(
      int32_t thumbnail_min_area_pixels,
      const gfx::Size& thumbnail_max_size_pixels,
      chrome::mojom::ImageFormat image_format,
      RequestThumbnailForContextNodeCallback callback) override;
  void RequestReloadImageForContextNode() override;
  void SetClientSidePhishingDetection(bool enable_phishing_detection) override;
  void GetWebApplicationInfo(GetWebApplicationInfoCallback callback) override;

  void OnRenderFrameObserverRequest(
      mojo::PendingAssociatedReceiver<chrome::mojom::ChromeRenderFrame>
          receiver);

  // Captures page information using the top (main) frame of a frame tree.
  // Currently, this page information is just the text content of the all
  // frames, collected and concatenated until a certain limit (kMaxIndexChars)
  // is reached.
  // TODO(dglazkov): This is incompatible with OOPIF and needs to be updated.
  void CapturePageText(TextCaptureType capture_type);

  void CapturePageTextLater(TextCaptureType capture_type,
                            base::TimeDelta delay);

  // Have the same lifetime as us.
  translate::TranslateHelper* translate_helper_;
  safe_browsing::PhishingClassifierDelegate* phishing_classifier_;

  // Owned by ChromeContentRendererClient and outlive us.
  web_cache::WebCacheImpl* web_cache_impl_;

#if !defined(OS_ANDROID)
  // Save the JavaScript to preload if ExecuteWebUIJavaScript is invoked.
  std::vector<base::string16> webui_javascript_;
#endif

  mojo::AssociatedReceiverSet<chrome::mojom::ChromeRenderFrame> receivers_;

  service_manager::BinderRegistry registry_;
  blink::AssociatedInterfaceRegistry associated_interfaces_;

  DISALLOW_COPY_AND_ASSIGN(ChromeRenderFrameObserver);
};

#endif  // CHROME_RENDERER_CHROME_RENDER_FRAME_OBSERVER_H_
