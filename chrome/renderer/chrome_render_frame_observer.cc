// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/chrome_render_frame_observer.h"

#include <stddef.h>
#include <string.h>

#include <limits>
#include <map>
#include <set>
#include <utility>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/lock.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_isolated_world_ids.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/open_search_description_document_handler.mojom.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/renderer/chrome_content_settings_agent_delegate.h"
#include "chrome/renderer/media/media_feeds.h"
#include "components/crash/core/common/crash_key.h"
#include "components/lens/lens_metadata.mojom.h"
#include "components/no_state_prefetch/renderer/no_state_prefetch_helper.h"
#include "components/no_state_prefetch/renderer/no_state_prefetch_utils.h"
#include "components/offline_pages/buildflags/buildflags.h"
#include "components/optimization_guide/content/renderer/page_text_agent.h"
#include "components/translate/content/renderer/translate_agent.h"
#include "components/translate/core/common/translate_util.h"
#include "components/web_cache/renderer/web_cache_impl.h"
#include "content/public/common/bindings_policy.h"
#include "content/public/common/content_features.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_frame_visitor.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/window_features_converter.h"
#include "printing/buildflags/buildflags.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "skia/ext/image_operations.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/public/web/web_console_message.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_document_loader.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_frame_content_dumper.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_node.h"
#include "third_party/blink/public/web/web_security_policy.h"
#include "third_party/blink/public/web/web_view.h"
#include "third_party/libwebp/src/src/webp/decode.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/codec/webp_codec.h"
#include "ui/gfx/geometry/size_f.h"
#include "url/gurl.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/renderer/accessibility/read_anything_app_controller.h"
#include "chrome/renderer/searchbox/searchbox_extension.h"
#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(SAFE_BROWSING_AVAILABLE)
#include "components/safe_browsing/content/renderer/phishing_classifier/phishing_classifier_delegate.h"
#include "components/safe_browsing/content/renderer/phishing_classifier/phishing_image_embedder_delegate.h"
#endif

#if BUILDFLAG(ENABLE_OFFLINE_PAGES)
#include "chrome/common/mhtml_page_notifier.mojom.h"
#endif

#if BUILDFLAG(ENABLE_PLUGINS)
#include "chrome/renderer/plugins/chrome_plugin_placeholder.h"
#endif

using blink::WebDocumentLoader;
using blink::WebElement;
using blink::WebFrameContentDumper;
using blink::WebLocalFrame;
using blink::WebNode;
using blink::WebString;
using content::RenderFrame;

// Maximum number of characters in the document to index.
// Any text beyond this point will be clipped.
static const size_t kMaxIndexChars = 65535;


// For a page that auto-refreshes, we still show the bubble, if
// the refresh delay is less than this value (in seconds).
static constexpr base::TimeDelta kLocationChangeInterval = base::Seconds(10);

// For the context menu, we want to keep transparency as is instead of
// replacing transparent pixels with black ones
static const bool kDiscardTransparencyForContextMenu = false;

namespace {

const char kGifExtension[] = ".gif";
const char kPngExtension[] = ".png";
const char kJpgExtension[] = ".jpg";
const char kWebpExtension[] = ".webp";

#if BUILDFLAG(IS_ANDROID)
base::Lock& GetFrameHeaderMapLock() {
  static base::NoDestructor<base::Lock> s;
  return *s;
}

using FrameHeaderMap = std::map<blink::LocalFrameToken, std::string>;

FrameHeaderMap& GetFrameHeaderMap() {
  GetFrameHeaderMapLock().AssertAcquired();
  static base::NoDestructor<FrameHeaderMap> s;
  return *s;
}
#endif

// Renderers can handle multiple pages, especially in low-memory conditions.
// Record crash keys for a few origins, in the hope of finding more culprit
// origins for OOM crashes. Keys are recorded here and not via
// ChromeContentClient::SetActiveURL() because that method is only invoked in
// response to IPC messages and most OOMs do not occur in response to an IPC.
// https://crbug.com/1310046
void UpdateLoadedOriginCrashKeys() {
  // Capture the origin for each RenderFrame.
  struct Visitor : public content::RenderFrameVisitor {
    bool Visit(RenderFrame* render_frame) override {
      if (render_frame) {
        WebLocalFrame* web_frame = render_frame->GetWebFrame();
        if (web_frame) {
          frame_count_++;
          origins_.insert(web_frame->GetSecurityOrigin().ToString().Utf8());
        }
      }
      return true;  // Keep going.
    }
    int frame_count_ = 0;
    std::set<std::string> origins_;  // Use set to collapse duplicate origins.
  } visitor;
  RenderFrame::ForEach(&visitor);

  static crash_reporter::CrashKeyString<8> frame_count("web-frame-count");
  frame_count.Set(base::NumberToString(visitor.frame_count_));

  // Record 3 recently-loaded origins in crash keys (which 3 is arbitrary).
  using ArrayItemKey = crash_reporter::CrashKeyString<64>;
  static ArrayItemKey crash_keys[] = {
      {"loaded-origin-0", ArrayItemKey::Tag::kArray},
      {"loaded-origin-1", ArrayItemKey::Tag::kArray},
      {"loaded-origin-2", ArrayItemKey::Tag::kArray},
  };
  for (auto& crash_key : crash_keys) {
    if (!visitor.origins_.empty()) {
      auto origin_it = visitor.origins_.begin();
      crash_key.Set(*origin_it);
      visitor.origins_.erase(origin_it);
    } else {
      // If there are fewer than 3 origins, clear the remaining keys.
      crash_key.Clear();
    }
  }
}

}  // namespace

ChromeRenderFrameObserver::ChromeRenderFrameObserver(
    content::RenderFrame* render_frame,
    web_cache::WebCacheImpl* web_cache_impl)
    : content::RenderFrameObserver(render_frame),
      translate_agent_(nullptr),
      page_text_agent_(new optimization_guide::PageTextAgent(render_frame)),
      web_cache_impl_(web_cache_impl) {
  render_frame->GetAssociatedInterfaceRegistry()
      ->AddInterface<chrome::mojom::ChromeRenderFrame>(base::BindRepeating(
          &ChromeRenderFrameObserver::OnRenderFrameObserverRequest,
          base::Unretained(this)));

  // Don't do anything else for subframes.
  if (!render_frame->IsMainFrame())
    return;

#if BUILDFLAG(SAFE_BROWSING_AVAILABLE)
  SetClientSidePhishingDetection();
#endif

  translate_agent_ =
      new translate::TranslateAgent(render_frame, ISOLATED_WORLD_ID_TRANSLATE);
}

ChromeRenderFrameObserver::~ChromeRenderFrameObserver() = default;

#if BUILDFLAG(IS_ANDROID)
std::string ChromeRenderFrameObserver::GetCCTClientHeader(
    const blink::LocalFrameToken& frame_token) {
  base::AutoLock auto_lock(GetFrameHeaderMapLock());
  auto frame_map = GetFrameHeaderMap();
  auto iter = frame_map.find(frame_token);
  return iter == frame_map.end() ? std::string() : iter->second;
}
#endif

void ChromeRenderFrameObserver::OnInterfaceRequestForFrame(
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle* interface_pipe) {
  registry_.TryBindInterface(interface_name, interface_pipe);
}

bool ChromeRenderFrameObserver::OnAssociatedInterfaceRequestForFrame(
    const std::string& interface_name,
    mojo::ScopedInterfaceEndpointHandle* handle) {
  return associated_interfaces_.TryBindInterface(interface_name, handle);
}

void ChromeRenderFrameObserver::ReadyToCommitNavigation(
    WebDocumentLoader* document_loader) {
  // Execute cache clear operations that were postponed until a navigation
  // event (including tab reload).
  if (render_frame()->IsMainFrame() && web_cache_impl_)
    web_cache_impl_->ExecutePendingClearCache();

  // Let translate_agent do any preparatory work for loading a URL.
  if (!translate_agent_)
    return;

  translate_agent_->PrepareForUrl(
      render_frame()->GetWebFrame()->GetDocument().Url());
}

void ChromeRenderFrameObserver::DidSetPageLifecycleState(
    bool restoring_from_bfcache) {
  if (restoring_from_bfcache && translate_agent_) {
    translate_agent_->RenewPageRegistration();
  }
}

void ChromeRenderFrameObserver::DidFinishLoad() {
  WebLocalFrame* frame = render_frame()->GetWebFrame();
  // Don't do anything for subframes.
  if (frame->Parent() || frame->IsInFencedFrameTree())
    return;

  GURL osdd_url = frame->GetDocument().OpenSearchDescriptionURL();
  if (!osdd_url.is_empty()) {
    mojo::Remote<chrome::mojom::OpenSearchDescriptionDocumentHandler>
        osdd_handler;
    render_frame()->GetBrowserInterfaceBroker().GetInterface(
        osdd_handler.BindNewPipeAndPassReceiver());
    osdd_handler->PageHasOpenSearchDescriptionDocument(
        frame->GetDocument().Url(), osdd_url);
  }
}

void ChromeRenderFrameObserver::DidCreateNewDocument() {
#if BUILDFLAG(ENABLE_OFFLINE_PAGES)
  DCHECK(render_frame());
  if (!render_frame()->IsMainFrame())
    return;

  DCHECK(render_frame()->GetWebFrame());
  blink::WebDocumentLoader* doc_loader =
      render_frame()->GetWebFrame()->GetDocumentLoader();
  DCHECK(doc_loader);

  if (!doc_loader->HasBeenLoadedAsWebArchive())
    return;

  // Connect to Mojo service on browser to notify it of the page's archive
  // properties.
  mojo::AssociatedRemote<offline_pages::mojom::MhtmlPageNotifier>
      mhtml_notifier;
  render_frame()->GetRemoteAssociatedInterfaces()->GetInterface(
      &mhtml_notifier);
  DCHECK(mhtml_notifier);
  blink::WebArchiveInfo info = doc_loader->GetArchiveInfo();

  mhtml_notifier->NotifyMhtmlPageLoadAttempted(info.load_result, info.url,
                                               info.date);
#endif
}

void ChromeRenderFrameObserver::DidCommitProvisionalLoad(
    ui::PageTransition transition) {
  // Update crash keys on any frame transition, not just the main frame.
  UpdateLoadedOriginCrashKeys();

  WebLocalFrame* frame = render_frame()->GetWebFrame();

  // Don't do anything for subframes.
  if (frame->Parent())
    return;

  static crash_reporter::CrashKeyString<8> view_count_key("view-count");
  view_count_key.Set(base::NumberToString(blink::WebView::GetWebViewCount()));

#if !BUILDFLAG(IS_ANDROID)
  if (render_frame()->GetEnabledBindings().HasAny(
          content::kWebUIBindingsPolicySet)) {
    for (const auto& script : webui_javascript_)
      render_frame()->ExecuteJavaScript(script);
    webui_javascript_.clear();
  }
#endif
}

void ChromeRenderFrameObserver::DidClearWindowObject() {
#if !BUILDFLAG(IS_ANDROID)
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kInstantProcess))
    SearchBoxExtension::Install(render_frame()->GetWebFrame());

  // Install ReadAnythingAppController on render frames with the Read Anything
  // url, which is chrome-untrusted. ReadAnythingAppController installs v8
  // bindings in the chrome.readingMode namespace which are consumed by
  // read_anything/app.ts, the resource of the Read Anything WebUI.
  if (render_frame()->GetWebFrame()->GetDocument().Url() ==
      chrome::kChromeUIUntrustedReadAnythingSidePanelURL) {
    ReadAnythingAppController::Install(render_frame());
  }
#endif  // !BUILDFLAG(IS_ANDROID)
}

void ChromeRenderFrameObserver::DidMeaningfulLayout(
    blink::WebMeaningfulLayout layout_type) {
  CapturePageText(layout_type);
}

void ChromeRenderFrameObserver::OnDestruct() {
  delete this;
}

void ChromeRenderFrameObserver::WillDetach(blink::DetachReason detach_reason) {
#if BUILDFLAG(IS_ANDROID)
  base::AutoLock auto_lock(GetFrameHeaderMapLock());
  GetFrameHeaderMap().erase(
      render_frame()->GetWebFrame()->GetLocalFrameToken());
#endif
}

void ChromeRenderFrameObserver::SetWindowFeatures(
    blink::mojom::WindowFeaturesPtr window_features) {
  render_frame()->GetWebView()->SetWindowFeatures(
      content::ConvertMojoWindowFeaturesToWebWindowFeatures(*window_features));
}

void ChromeRenderFrameObserver::ExecuteWebUIJavaScript(
    const std::u16string& javascript) {
#if !BUILDFLAG(IS_ANDROID)
  webui_javascript_.push_back(javascript);
#endif
}

void ChromeRenderFrameObserver::RequestImageForContextNode(
    int32_t thumbnail_min_area_pixels,
    const gfx::Size& thumbnail_max_size_pixels,
    chrome::mojom::ImageFormat image_format,
    int32_t quality,
    RequestImageForContextNodeCallback callback) {
  WebNode context_node = render_frame()->GetWebFrame()->ContextMenuImageNode();
  std::vector<uint8_t> image_data;
  gfx::Size original_size;
  std::string image_extension;
  std::vector<lens::mojom::LatencyLogPtr> latency_logs;

  // Map for converting between multiple mojom ImageFormat structures to
  // prevent a circular dependency (go/lens-logging-in-chromium)
  // TODO(shivpatel): add default value UNKNOWN for if ImageFormat is updated
  const std::map<chrome::mojom::ImageFormat, lens::mojom::ImageFormat>
      image_format_conversion = {
          {chrome::mojom::ImageFormat::ORIGINAL,
           lens::mojom::ImageFormat::ORIGINAL},
          {chrome::mojom::ImageFormat::PNG, lens::mojom::ImageFormat::PNG},
          {chrome::mojom::ImageFormat::JPEG, lens::mojom::ImageFormat::JPEG},
          {chrome::mojom::ImageFormat::WEBP, lens::mojom::ImageFormat::WEBP},
      };

  if (context_node.IsNull() || !context_node.IsElementNode()) {
    // The downscaled size is the original size, since no downscaling was
    // required.
    std::move(callback).Run(image_data, original_size,
                            /*downscaled_size=*/original_size, image_extension,
                            std::move(latency_logs));
    return;
  }

  WebElement web_element = context_node.To<WebElement>();
  original_size = web_element.GetImageSize();
  image_extension = "." + web_element.ImageExtension();
  bool needs_downscale = NeedsDownscale(
      original_size, thumbnail_min_area_pixels, thumbnail_max_size_pixels);
  bool needs_encode = NeedsEncodeImage(image_extension, image_format) ||
                      IsAnimatedWebp(web_element.CopyOfImageData());
  if (!needs_encode && !needs_downscale) {
    image_data = web_element.CopyOfImageData();
    // The downscaled size is the original size, since no downscaling was
    // required.
    std::move(callback).Run(std::move(image_data), original_size,
                            /*downscaled_size=*/original_size, image_extension,
                            std::move(latency_logs));
    return;
  }
  SkBitmap image = web_element.ImageContents();
  if (needs_downscale) {
    latency_logs.push_back(lens::mojom::LatencyLog::New(
        lens::mojom::Phase::DOWNSCALE_START, original_size, gfx::Size(),
        image_format_conversion.at(image_format), base::Time::Now(),
        /*encoded_size_bytes=*/0));
  }
  SkBitmap thumbnail =
      Downscale(image, thumbnail_min_area_pixels, thumbnail_max_size_pixels);
  gfx::Size downscaled_size = gfx::Size(thumbnail.width(), thumbnail.height());
  if (needs_downscale) {
    latency_logs.push_back(lens::mojom::LatencyLog::New(
        lens::mojom::Phase::DOWNSCALE_END, original_size, downscaled_size,
        image_format_conversion.at(image_format), base::Time::Now(),
        /*encoded_size_bytes=*/0));
  }

  SkBitmap bitmap;
  if (thumbnail.colorType() == kN32_SkColorType) {
    bitmap = thumbnail;
  } else {
    SkImageInfo info = thumbnail.info().makeColorType(kN32_SkColorType);
    if (bitmap.tryAllocPixels(info)) {
      thumbnail.readPixels(info, bitmap.getPixels(), bitmap.rowBytes(), 0, 0);
    }
  }

  std::vector<unsigned char> data;
  if (image_format == chrome::mojom::ImageFormat::ORIGINAL) {
    // ORIGINAL will only fall back to here if the image needs to downscale.
    // Let's PNG downscale to PNG and JEPG downscale to JPEG.
    if (image_extension == kPngExtension) {
      image_format = chrome::mojom::ImageFormat::PNG;
    } else if (image_extension == kJpgExtension) {
      image_format = chrome::mojom::ImageFormat::JPEG;
    }
  }

  if (needs_encode) {
    latency_logs.push_back(lens::mojom::LatencyLog::New(
        lens::mojom::Phase::ENCODE_START, original_size, downscaled_size,
        image_format_conversion.at(image_format), base::Time::Now(),
        /*encoded_size_bytes=*/0));
  }
  switch (image_format) {
    case chrome::mojom::ImageFormat::PNG:
      if (gfx::PNGCodec::EncodeBGRASkBitmap(
              bitmap, kDiscardTransparencyForContextMenu, &data)) {
        image_data.swap(data);
        image_extension = kPngExtension;
      }
      break;
    case chrome::mojom::ImageFormat::WEBP:
      if (gfx::WebpCodec::Encode(bitmap, quality, &data)) {
        image_data.swap(data);
        image_extension = kWebpExtension;
      }
      break;
    case chrome::mojom::ImageFormat::ORIGINAL:
    // Any format other than PNG and JPEG fall back to here.
    case chrome::mojom::ImageFormat::JPEG:
      if (gfx::JPEGCodec::Encode(bitmap, quality, &data)) {
        image_data.swap(data);
        image_extension = kJpgExtension;
      }
      break;
  }
  if (needs_encode) {
    latency_logs.push_back(lens::mojom::LatencyLog::New(
        lens::mojom::Phase::ENCODE_END, original_size, downscaled_size,
        image_format_conversion.at(image_format), base::Time::Now(),
        sizeof(uint8_t) * image_data.size()));
  }

  std::move(callback).Run(image_data, original_size, downscaled_size,
                          image_extension, std::move(latency_logs));
}

void ChromeRenderFrameObserver::RequestBitmapForContextNode(
    RequestBitmapForContextNodeCallback callback) {
  WebNode context_node = render_frame()->GetWebFrame()->ContextMenuImageNode();
  SkBitmap image;
  if (context_node.IsNull() || !context_node.IsElementNode()) {
    std::move(callback).Run(image);
    return;
  }

  WebElement web_element = context_node.To<WebElement>();
  image = web_element.ImageContents();
  std::move(callback).Run(image);
}

void ChromeRenderFrameObserver::RequestBitmapForContextNodeWithBoundsHint(
    RequestBitmapForContextNodeWithBoundsHintCallback callback) {
  WebNode context_node = render_frame()->GetWebFrame()->ContextMenuImageNode();
  SkBitmap image;
  gfx::Rect bounds;
  if (context_node.IsNull() || !context_node.IsElementNode()) {
    std::move(callback).Run(image, bounds);
    return;
  }

  WebElement web_element = context_node.To<WebElement>();
  image = web_element.ImageContents();
  bounds = web_element.BoundsInWidget();
  std::move(callback).Run(image, bounds);
}

void ChromeRenderFrameObserver::RequestBoundsHintForAllImages(
    RequestBoundsHintForAllImagesCallback callback) {
  std::vector<blink::WebElement> image_elements;
  std::vector<gfx::Rect> all_bounds;
  const blink::WebDocument doc = render_frame()->GetWebFrame()->GetDocument();
  if (doc.IsNull() || doc.Body().IsNull()) {
    return;
  }
  FindImageElements(doc.Body(), image_elements);

  for (auto& element : image_elements) {
    all_bounds.emplace_back(
        render_frame()->ConvertViewportToWindow(element.BoundsInWidget()));
  }
  std::move(callback).Run(all_bounds);
}

// Depth-first search for recursively traversing DOM elements and pulling out
// references for images.
void ChromeRenderFrameObserver::FindImageElements(
    blink::WebElement element,
    std::vector<blink::WebElement>& images) {
  if (element.ImageContents().isNull()) {
    for (blink::WebNode child = element.FirstChild(); !child.IsNull();
         child = child.NextSibling()) {
      if (child.IsElementNode()) {
        FindImageElements(child.To<blink::WebElement>(), images);
      }
    }
  } else {
    images.emplace_back(element);
  }
}

void ChromeRenderFrameObserver::RequestReloadImageForContextNode() {
  WebLocalFrame* frame = render_frame()->GetWebFrame();
  // TODO(dglazkov): This code is clearly in the wrong place. Need
  // to investigate what it is doing and fix (http://crbug.com/606164).
  WebNode context_node = frame->ContextMenuImageNode();
  if (!context_node.IsNull()) {
    frame->ReloadImage(context_node);
  }
}

#if BUILDFLAG(IS_ANDROID)
void ChromeRenderFrameObserver::SetCCTClientHeader(const std::string& header) {
  auto* web_frame = render_frame()->GetWebFrame();
  if (!web_frame) {
    return;
  }
  base::AutoLock auto_lock(GetFrameHeaderMapLock());
  GetFrameHeaderMap()[web_frame->GetLocalFrameToken()] = header;
}
#endif

void ChromeRenderFrameObserver::GetMediaFeedURL(
    GetMediaFeedURLCallback callback) {
  std::move(callback).Run(MediaFeeds::GetMediaFeedURL(render_frame()));
}

void ChromeRenderFrameObserver::LoadBlockedPlugins(
    const std::string& identifier) {
  // Record that this plugin is temporarily allowed and notify all placeholders.

  ChromeContentSettingsAgentDelegate::Get(render_frame())
      ->AllowPluginTemporarily(identifier);

#if BUILDFLAG(ENABLE_PLUGINS)
  ChromePluginPlaceholder::ForEach(
      render_frame(), base::BindRepeating(
                          [](const std::string& identifier,
                             ChromePluginPlaceholder* placeholder) {
                            placeholder->MaybeLoadBlockedPlugin(identifier);
                          },
                          identifier));
#endif  // BUILDFLAG(ENABLE_PLUGINS)
}

void ChromeRenderFrameObserver::SetSupportsDraggableRegions(
    bool supports_draggable_regions) {
  render_frame()->GetWebView()->SetSupportsDraggableRegions(
      supports_draggable_regions);
}

void ChromeRenderFrameObserver::SetShouldDeferMediaLoad(bool should_defer) {
  prerender::SetShouldDeferMediaLoad(render_frame(), should_defer);
}

void ChromeRenderFrameObserver::SetClientSidePhishingDetection() {
#if BUILDFLAG(SAFE_BROWSING_AVAILABLE)
  phishing_classifier_ = safe_browsing::PhishingClassifierDelegate::Create(
      render_frame(), nullptr);
  phishing_image_embedder_ =
      safe_browsing::PhishingImageEmbedderDelegate::Create(render_frame());
#endif
}

void ChromeRenderFrameObserver::OnRenderFrameObserverRequest(
    mojo::PendingAssociatedReceiver<chrome::mojom::ChromeRenderFrame>
        receiver) {
  receivers_.Add(this, std::move(receiver));
}

bool ChromeRenderFrameObserver::ShouldCapturePageTextForTranslateOrPhishing(
    blink::WebMeaningfulLayout layout_type) const {
  WebLocalFrame* frame = render_frame()->GetWebFrame();
  if (!frame) {
    return false;
  }

  //////////////////////////////////////////////////////////////////////////////
  // Check |frame| for conditions shared by both Translate and Phishing.

  if (!render_frame()->IsMainFrame()) {
    return false;
  }

  // |kVisuallyNonEmpty| is ignored by Translate and Phishing.
  switch (layout_type) {
    case blink::WebMeaningfulLayout::kFinishedParsing:
    case blink::WebMeaningfulLayout::kFinishedLoading:
      break;
    case blink::WebMeaningfulLayout::kVisuallyNonEmpty:
    default:
      return false;
  }

  // Don't capture pages that have pending redirect or location change.
  if (frame->IsNavigationScheduledWithin(kLocationChangeInterval)) {
    return false;
  }

  // Don't capture pages that are in view source mode.
  if (frame->IsViewSourceModeEnabled()) {
    return false;
  }

  // Don't capture text of the error pages.
  WebDocumentLoader* document_loader = frame->GetDocumentLoader();
  if (document_loader && document_loader->HasUnreachableURL()) {
    return false;
  }

  // Don't capture pages that are being no-state prefetched.
  if (prerender::NoStatePrefetchHelper::IsPrefetching(render_frame())) {
    return false;
  }

  //////////////////////////////////////////////////////////////////////////////
  // Translate specific checks.
  bool should_capture_for_translate = !!translate_agent_;

  //////////////////////////////////////////////////////////////////////////////
  // Phishing specific checks.
  bool should_capture_for_phishing = false;

#if BUILDFLAG(SAFE_BROWSING_AVAILABLE)
  should_capture_for_phishing = phishing_classifier_->is_ready();
#endif

  return should_capture_for_translate || should_capture_for_phishing;
}

void ChromeRenderFrameObserver::CapturePageText(
    blink::WebMeaningfulLayout layout_type) {
  bool capture_for_translate_phishing =
      ShouldCapturePageTextForTranslateOrPhishing(layout_type);

  uint32_t capture_max_size =
      capture_for_translate_phishing ? kMaxIndexChars : 0;
  auto text_callback = page_text_agent_->MaybeRequestTextDumpOnLayoutEvent(
      layout_type, &capture_max_size);
  bool capture_for_opt_guide = !!text_callback;

  if (!capture_for_translate_phishing && !capture_for_opt_guide) {
    return;
  }
  DCHECK_GT(capture_max_size, 0U);

  scoped_refptr<const base::RefCountedString16> contents;

  {
    TRACE_EVENT0("renderer", "ChromeRenderFrameObserver::CapturePageText");
    contents = base::MakeRefCounted<const base::RefCountedString16>(
        WebFrameContentDumper::DumpFrameTreeAsText(
            render_frame()->GetWebFrame(), capture_max_size)
            .Utf16());
  }

  // Language detection should run only once. Parsing finishes before the page
  // loads, so attempt detection here first.
  if (translate_agent_ &&
      (layout_type == blink::WebMeaningfulLayout::kFinishedParsing)) {
    translate_agent_->PageCaptured(contents);
  }

  if (text_callback) {
    std::move(text_callback).Run(contents);
  }

#if BUILDFLAG(SAFE_BROWSING_AVAILABLE)
  // Will swap out the string.
  if (phishing_classifier_) {
    phishing_classifier_->PageCaptured(
        contents, layout_type == blink::WebMeaningfulLayout::kFinishedParsing);
  }
  if (phishing_image_embedder_) {
    phishing_image_embedder_->PageCaptured(
        layout_type == blink::WebMeaningfulLayout::kFinishedParsing);
  }
#endif
}

// static
bool ChromeRenderFrameObserver::NeedsDownscale(
    const gfx::Size& original_image_size,
    int32_t requested_image_min_area_pixels,
    const gfx::Size& requested_image_max_size) {
  if (original_image_size.GetArea() < requested_image_min_area_pixels)
    return false;
  if (original_image_size.width() <= requested_image_max_size.width() &&
      original_image_size.height() <= requested_image_max_size.height())
    return false;
  return true;
}

// static
SkBitmap ChromeRenderFrameObserver::Downscale(
    const SkBitmap& image,
    int requested_image_min_area_pixels,
    const gfx::Size& requested_image_max_size) {
  if (image.isNull())
    return SkBitmap();

  gfx::Size image_size(image.width(), image.height());

  if (!NeedsDownscale(image_size, requested_image_min_area_pixels,
                      requested_image_max_size))
    return image;

  gfx::SizeF scaled_size = gfx::SizeF(image_size);

  if (scaled_size.width() > requested_image_max_size.width()) {
    scaled_size.Scale(requested_image_max_size.width() / scaled_size.width());
  }

  if (scaled_size.height() > requested_image_max_size.height()) {
    scaled_size.Scale(requested_image_max_size.height() / scaled_size.height());
  }

  return skia::ImageOperations::Resize(image,
                                       skia::ImageOperations::RESIZE_GOOD,
                                       static_cast<int>(scaled_size.width()),
                                       static_cast<int>(scaled_size.height()));
}

// static
bool ChromeRenderFrameObserver::NeedsEncodeImage(
    const std::string& image_extension,
    chrome::mojom::ImageFormat image_format) {
  switch (image_format) {
    case chrome::mojom::ImageFormat::PNG:
      return !base::EqualsCaseInsensitiveASCII(image_extension, kPngExtension);
    case chrome::mojom::ImageFormat::WEBP:
      return !base::EqualsCaseInsensitiveASCII(image_extension, kWebpExtension);
    case chrome::mojom::ImageFormat::JPEG:
      return !base::EqualsCaseInsensitiveASCII(image_extension, kJpgExtension);
    case chrome::mojom::ImageFormat::ORIGINAL:
      return !base::EqualsCaseInsensitiveASCII(image_extension,
                                               kGifExtension) &&
             !base::EqualsCaseInsensitiveASCII(image_extension,
                                               kJpgExtension) &&
             !base::EqualsCaseInsensitiveASCII(image_extension, kPngExtension);
  }

  // Should never hit this code since all cases were handled above.
  NOTREACHED_IN_MIGRATION();
  return true;
}

// static
bool ChromeRenderFrameObserver::IsAnimatedWebp(
    const std::vector<uint8_t>& image_data) {
  WebPBitstreamFeatures features{};
  VP8StatusCode status =
      WebPGetFeatures(image_data.data(), image_data.size(), &features);
  return status == VP8_STATUS_OK && features.has_animation;
}
