// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_CHROME_RENDER_FRAME_OBSERVER_H_
#define CHROME_RENDERER_CHROME_RENDER_FRAME_OBSERVER_H_

#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "chrome/common/chrome_render_frame.mojom.h"
#include "components/safe_browsing/buildflags.h"
#include "content/public/renderer/render_frame_observer.h"
#include "mojo/public/cpp/bindings/associated_receiver_set.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"

class SkBitmap;

namespace gfx {
class Size;
}

namespace optimization_guide {
class PageTextAgent;
}

namespace safe_browsing {
class PhishingClassifierDelegate;
class PhishingImageEmbedderDelegate;
}

namespace translate {
class TranslateAgent;
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

  ChromeRenderFrameObserver(const ChromeRenderFrameObserver&) = delete;
  ChromeRenderFrameObserver& operator=(const ChromeRenderFrameObserver&) =
      delete;

  ~ChromeRenderFrameObserver() override;

  service_manager::BinderRegistry* registry() { return &registry_; }
  blink::AssociatedInterfaceRegistry* associated_interfaces() {
    return &associated_interfaces_;
  }

#if BUILDFLAG(IS_ANDROID)
  // This is called on the main thread for subresources or worker threads for
  // dedicated workers.
  static std::string GetCCTClientHeader(
      const blink::LocalFrameToken& frame_token);
#endif

 private:
  friend class ChromeRenderFrameObserverTest;

  // RenderFrameObserver implementation.
  void OnInterfaceRequestForFrame(
      const std::string& interface_name,
      mojo::ScopedMessagePipeHandle* interface_pipe) override;
  bool OnAssociatedInterfaceRequestForFrame(
      const std::string& interface_name,
      mojo::ScopedInterfaceEndpointHandle* handle) override;
  void ReadyToCommitNavigation(
      blink::WebDocumentLoader* document_loader) override;
  void DidSetPageLifecycleState(bool restoring_from_bfcache) override;
  void DidFinishLoad() override;
  void DidCreateNewDocument() override;
  void DidCommitProvisionalLoad(ui::PageTransition transition) override;
  void DidClearWindowObject() override;
  void DidMeaningfulLayout(blink::WebMeaningfulLayout layout_type) override;
  void OnDestruct() override;
  void WillDetach(blink::DetachReason detach_reason) override;

  // chrome::mojom::ChromeRenderFrame:
  void SetWindowFeatures(
      blink::mojom::WindowFeaturesPtr window_features) override;
  void ExecuteWebUIJavaScript(const std::u16string& javascript) override;
  void RequestImageForContextNode(
      int32_t thumbnail_min_area_pixels,
      const gfx::Size& thumbnail_max_size_pixels,
      chrome::mojom::ImageFormat image_format,
      int32_t quality,
      RequestImageForContextNodeCallback callback) override;
  void RequestBitmapForContextNode(
      RequestBitmapForContextNodeCallback callback) override;
  void RequestBitmapForContextNodeWithBoundsHint(
      RequestBitmapForContextNodeWithBoundsHintCallback callback) override;
  void RequestBoundsHintForAllImages(
      RequestBoundsHintForAllImagesCallback callback) override;
  void FindImageElements(blink::WebElement element,
                         std::vector<blink::WebElement>& images);
  void RequestReloadImageForContextNode() override;
#if BUILDFLAG(IS_ANDROID)
  void SetCCTClientHeader(const std::string& header) override;
#endif
  void GetMediaFeedURL(GetMediaFeedURLCallback callback) override;
  void LoadBlockedPlugins(const std::string& identifier) override;
  void SetSupportsDraggableRegions(bool supports_draggable_regions) override;
  void SetShouldDeferMediaLoad(bool should_defer) override;

  // Initialize a |phishing_classifier_delegate_|.
  void SetClientSidePhishingDetection();

  void OnRenderFrameObserverRequest(
      mojo::PendingAssociatedReceiver<chrome::mojom::ChromeRenderFrame>
          receiver);

  // Captures page information using the top (main) frame of a frame tree.
  // Currently, this page information is just the text content of the local
  // frames, collected and concatenated until a certain limit (kMaxIndexChars)
  // is reached.
  void CapturePageText(blink::WebMeaningfulLayout layout_type);

  // Returns true if |CapturePageText| should be run for Translate or Phishing.
  bool ShouldCapturePageTextForTranslateOrPhishing(
      blink::WebMeaningfulLayout layout_type) const;

  // Check if the image need to downscale.
  static bool NeedsDownscale(const gfx::Size& original_image_size,
                             int32_t requested_image_min_area_pixels,
                             const gfx::Size& requested_image_max_size);

  // If the source image is null or occupies less area than
  // |requested_image_min_area_pixels|, we return the image unmodified.
  // Otherwise, we scale down the image so that the width and height do not
  // exceed |requested_image_max_size|, preserving the original aspect ratio.
  static SkBitmap Downscale(const SkBitmap& image,
                            int requested_image_min_area_pixels,
                            const gfx::Size& requested_image_max_size);

  // Check if the image need to encode to fit requested image format.
  static bool NeedsEncodeImage(const std::string& image_extension,
                               chrome::mojom::ImageFormat image_format);

  // Check if the image is an animated Webp image by looking for animation
  // feature flag
  static bool IsAnimatedWebp(const std::vector<uint8_t>& image_data);

  // Have the same lifetime as us.
  raw_ptr<translate::TranslateAgent> translate_agent_;
  raw_ptr<optimization_guide::PageTextAgent> page_text_agent_;
#if BUILDFLAG(SAFE_BROWSING_AVAILABLE)
  raw_ptr<safe_browsing::PhishingClassifierDelegate> phishing_classifier_ =
      nullptr;
  raw_ptr<safe_browsing::PhishingImageEmbedderDelegate>
      phishing_image_embedder_ = nullptr;
#endif

  // Owned by ChromeContentRendererClient and outlive us.
  raw_ptr<web_cache::WebCacheImpl> web_cache_impl_;

#if !BUILDFLAG(IS_ANDROID)
  // Save the JavaScript to preload if ExecuteWebUIJavaScript is invoked.
  std::vector<std::u16string> webui_javascript_;
#endif

  mojo::AssociatedReceiverSet<chrome::mojom::ChromeRenderFrame> receivers_;

  service_manager::BinderRegistry registry_;
  blink::AssociatedInterfaceRegistry associated_interfaces_;
};

#endif  // CHROME_RENDERER_CHROME_RENDER_FRAME_OBSERVER_H_
