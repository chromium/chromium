// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/chrome_render_frame_observer.h"

#include <stddef.h>
#include <string.h>

#include <limits>
#include <map>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/lock.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_isolated_world_ids.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/open_search_description_document_handler.mojom.h"
#include "chrome/common/web_page_metadata.mojom.h"
#include "chrome/renderer/chrome_content_settings_agent_delegate.h"
#include "chrome/renderer/media/media_feeds.h"
#include "chrome/renderer/web_page_metadata_extraction.h"
#include "components/crash/core/common/crash_key.h"
#include "components/no_state_prefetch/renderer/prerender_helper.h"
#include "components/offline_pages/buildflags/buildflags.h"
#include "components/translate/content/renderer/translate_agent.h"
#include "components/translate/core/common/translate_util.h"
#include "components/web_cache/renderer/web_cache_impl.h"
#include "content/public/common/bindings_policy.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/render_view.h"
#include "content/public/renderer/window_features_converter.h"
#include "extensions/common/constants.h"
#include "printing/buildflags/buildflags.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "skia/ext/image_operations.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
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
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/geometry/size_f.h"
#include "url/gurl.h"

#if !defined(OS_ANDROID)
#include "chrome/renderer/searchbox/searchbox_extension.h"
#endif  // !defined(OS_ANDROID)

#if BUILDFLAG(SAFE_BROWSING_AVAILABLE)
#include "components/safe_browsing/content/renderer/phishing_classifier/phishing_classifier_delegate.h"
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

// Constants for UMA statistic collection.
static const char kTranslateCaptureText[] = "Translate.CaptureText";

// For a page that auto-refreshes, we still show the bubble, if
// the refresh delay is less than this value (in seconds).
static constexpr base::TimeDelta kLocationChangeInterval =
    base::TimeDelta::FromSeconds(10);

// For the context menu, we want to keep transparency as is instead of
// replacing transparent pixels with black ones
static const bool kDiscardTransparencyForContextMenu = false;

namespace {

const char kGifExtension[] = ".gif";
const char kPngExtension[] = ".png";
const char kJpgExtension[] = ".jpg";

#if defined(OS_ANDROID)
base::Lock& GetFrameHeaderMapLock() {
  static base::NoDestructor<base::Lock> s;
  return *s;
}

using FrameHeaderMap = std::map<int, std::string>;

FrameHeaderMap& GetFrameHeaderMap() {
  GetFrameHeaderMapLock().AssertAcquired();
  static base::NoDestructor<FrameHeaderMap> s;
  return *s;
}
#endif

}  // namespace

ChromeRenderFrameObserver::ChromeRenderFrameObserver(
    content::RenderFrame* render_frame,
    web_cache::WebCacheImpl* web_cache_impl)
    : content::RenderFrameObserver(render_frame),
      translate_agent_(nullptr),
      web_cache_impl_(web_cache_impl) {
  render_frame->GetAssociatedInterfaceRegistry()->AddInterface(
      base::BindRepeating(
          &ChromeRenderFrameObserver::OnRenderFrameObserverRequest,
          base::Unretained(this)));

  // Don't do anything else for subframes.
  if (!render_frame->IsMainFrame())
    return;

#if BUILDFLAG(SAFE_BROWSING_AVAILABLE)
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  if (!command_line.HasSwitch(switches::kDisableClientSidePhishingDetection))
    SetClientSidePhishingDetection();
#endif
  if (!translate::IsSubFrameTranslationEnabled()) {
    translate_agent_ =
        new translate::TranslateAgent(render_frame, ISOLATED_WORLD_ID_TRANSLATE,
                                      extensions::kExtensionScheme);
  }
}

ChromeRenderFrameObserver::~ChromeRenderFrameObserver() {
#if defined(OS_ANDROID)
  base::AutoLock auto_lock(GetFrameHeaderMapLock());
  GetFrameHeaderMap().erase(routing_id());
#endif
}

#if defined(OS_ANDROID)
std::string ChromeRenderFrameObserver::GetCCTClientHeader(int render_frame_id) {
  base::AutoLock auto_lock(GetFrameHeaderMapLock());
  auto frame_map = GetFrameHeaderMap();
  auto iter = frame_map.find(render_frame_id);
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

void ChromeRenderFrameObserver::DidFinishLoad() {
  WebLocalFrame* frame = render_frame()->GetWebFrame();
  // Don't do anything for subframes.
  if (frame->Parent())
    return;

  GURL osdd_url = frame->GetDocument().OpenSearchDescriptionURL();
  if (!osdd_url.is_empty()) {
    mojo::AssociatedRemote<chrome::mojom::OpenSearchDescriptionDocumentHandler>
        osdd_handler;
    render_frame()->GetRemoteAssociatedInterfaces()->GetInterface(
        &osdd_handler);
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
  WebLocalFrame* frame = render_frame()->GetWebFrame();

  // Don't do anything for subframes.
  if (frame->Parent())
    return;

  static crash_reporter::CrashKeyString<8> view_count_key("view-count");
  view_count_key.Set(
      base::NumberToString(content::RenderView::GetRenderViewCount()));

#if !defined(OS_ANDROID)
  if (render_frame()->GetEnabledBindings() &
      content::kWebUIBindingsPolicyMask) {
    for (const auto& script : webui_javascript_)
      render_frame()->ExecuteJavaScript(script);
    webui_javascript_.clear();
  }
#endif
}

void ChromeRenderFrameObserver::DidClearWindowObject() {
#if !defined(OS_ANDROID)
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kInstantProcess))
    SearchBoxExtension::Install(render_frame()->GetWebFrame());
#endif  // !defined(OS_ANDROID)
}

void ChromeRenderFrameObserver::DidMeaningfulLayout(
    blink::WebMeaningfulLayout layout_type) {
  // Don't do any work for subframes.
  if (!render_frame()->IsMainFrame())
    return;

  switch (layout_type) {
    case blink::WebMeaningfulLayout::kFinishedParsing:
      CapturePageText(PRELIMINARY_CAPTURE);
      break;
    case blink::WebMeaningfulLayout::kFinishedLoading:
      CapturePageText(FINAL_CAPTURE);
      break;
    default:
      break;
  }
}

void ChromeRenderFrameObserver::OnDestruct() {
  delete this;
}

void ChromeRenderFrameObserver::SetWindowFeatures(
    blink::mojom::WindowFeaturesPtr window_features) {
  render_frame()->GetRenderView()->GetWebView()->SetWindowFeatures(
      content::ConvertMojoWindowFeaturesToWebWindowFeatures(*window_features));
}

void ChromeRenderFrameObserver::ExecuteWebUIJavaScript(
    const base::string16& javascript) {
#if !defined(OS_ANDROID)
  webui_javascript_.push_back(javascript);
#endif
}

void ChromeRenderFrameObserver::RequestImageForContextNode(
    int32_t thumbnail_min_area_pixels,
    const gfx::Size& thumbnail_max_size_pixels,
    chrome::mojom::ImageFormat image_format,
    RequestImageForContextNodeCallback callback) {
  WebNode context_node = render_frame()->GetWebFrame()->ContextMenuNode();
  std::vector<uint8_t> image_data;
  gfx::Size original_size;
  std::string image_extension;

  if (context_node.IsNull() || !context_node.IsElementNode()) {
    std::move(callback).Run(image_data, original_size, image_extension);
    return;
  }

  WebElement web_element = context_node.To<WebElement>();
  original_size = web_element.GetImageSize();
  image_extension = "." + web_element.ImageExtension();
  if (!NeedsEncodeImage(image_extension, image_format) &&
      !NeedsDownscale(original_size, thumbnail_min_area_pixels,
                      thumbnail_max_size_pixels)) {
    image_data = web_element.CopyOfImageData();
    std::move(callback).Run(std::move(image_data), original_size,
                            image_extension);
    return;
  }

  SkBitmap image = web_element.ImageContents();
  SkBitmap thumbnail =
      Downscale(image, thumbnail_min_area_pixels, thumbnail_max_size_pixels);

  SkBitmap bitmap;
  if (thumbnail.colorType() == kN32_SkColorType) {
    bitmap = thumbnail;
  } else {
    SkImageInfo info = thumbnail.info().makeColorType(kN32_SkColorType);
    if (bitmap.tryAllocPixels(info)) {
      thumbnail.readPixels(info, bitmap.getPixels(), bitmap.rowBytes(), 0, 0);
    }
  }

  constexpr int kDefaultQuality = 90;
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

  switch (image_format) {
    case chrome::mojom::ImageFormat::PNG:
      if (gfx::PNGCodec::EncodeBGRASkBitmap(
              bitmap, kDiscardTransparencyForContextMenu, &data)) {
        image_data.swap(data);
        image_extension = kPngExtension;
      }
      break;
    case chrome::mojom::ImageFormat::ORIGINAL:
    // Any format other than PNG and JPEG fall back to here.
    case chrome::mojom::ImageFormat::JPEG:
      if (gfx::JPEGCodec::Encode(bitmap, kDefaultQuality, &data)) {
        image_data.swap(data);
        image_extension = kJpgExtension;
      }
      break;
  }
  std::move(callback).Run(image_data, original_size, image_extension);
}

void ChromeRenderFrameObserver::RequestReloadImageForContextNode() {
  WebLocalFrame* frame = render_frame()->GetWebFrame();
  // TODO(dglazkov): This code is clearly in the wrong place. Need
  // to investigate what it is doing and fix (http://crbug.com/606164).
  WebNode context_node = frame->ContextMenuNode();
  if (!context_node.IsNull()) {
    frame->ReloadImage(context_node);
  }
}

void ChromeRenderFrameObserver::GetWebPageMetadata(
    GetWebPageMetadataCallback callback) {
  WebLocalFrame* frame = render_frame()->GetWebFrame();

  chrome::mojom::WebPageMetadataPtr web_page_metadata =
      chrome::ExtractWebPageMetadata(frame);

  // The warning below is specific to mobile but it doesn't hurt to show it even
  // if the Chromium build is running on a desktop. It will get more exposition.
  if (web_page_metadata->mobile_capable ==
      chrome::mojom::WebPageMobileCapable::ENABLED_APPLE) {
    blink::WebConsoleMessage message(
        blink::mojom::ConsoleMessageLevel::kWarning,
        "<meta name=\"apple-mobile-web-app-capable\" content=\"yes\"> is "
        "deprecated. Please include <meta name=\"mobile-web-app-capable\" "
        "content=\"yes\">");
    frame->AddMessageToConsole(message);
  }

  // Prune out any data URLs in the set of icons.  The browser process expects
  // any icon with a data URL to have originated from a favicon.  We don't want
  // to decode arbitrary data URLs in the browser process.  See
  // http://b/issue?id=1162972
  for (auto it = web_page_metadata->icons.begin();
       it != web_page_metadata->icons.end();) {
    if ((*it)->url.SchemeIs(url::kDataScheme))
      it = web_page_metadata->icons.erase(it);
    else
      ++it;
  }

  // Truncate the strings we send to the browser process.
  web_page_metadata->application_name =
      web_page_metadata->application_name.substr(
          0, chrome::kMaxMetaTagAttributeLength);
  web_page_metadata->description = web_page_metadata->description.substr(
      0, chrome::kMaxMetaTagAttributeLength);

  std::move(callback).Run(std::move(web_page_metadata));
}

#if defined(OS_ANDROID)
void ChromeRenderFrameObserver::SetCCTClientHeader(const std::string& header) {
  base::AutoLock auto_lock(GetFrameHeaderMapLock());
  GetFrameHeaderMap()[routing_id()] = header;
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

void ChromeRenderFrameObserver::SetClientSidePhishingDetection() {
#if BUILDFLAG(SAFE_BROWSING_AVAILABLE)
  phishing_classifier_ = safe_browsing::PhishingClassifierDelegate::Create(
      render_frame(), nullptr);
#endif
}

void ChromeRenderFrameObserver::OnRenderFrameObserverRequest(
    mojo::PendingAssociatedReceiver<chrome::mojom::ChromeRenderFrame>
        receiver) {
  receivers_.Add(this, std::move(receiver));
}

void ChromeRenderFrameObserver::CapturePageText(TextCaptureType capture_type) {
  WebLocalFrame* frame = render_frame()->GetWebFrame();
  if (!frame)
    return;

  // Don't capture pages that have pending redirect or location change.
  if (frame->IsNavigationScheduledWithin(kLocationChangeInterval))
    return;

  // Don't index/capture pages that are in view source mode.
  if (frame->IsViewSourceModeEnabled())
    return;

  // Don't capture text of the error pages.
  WebDocumentLoader* document_loader = frame->GetDocumentLoader();
  if (document_loader && document_loader->HasUnreachableURL())
    return;

  // Don't index/capture pages that are being prerendered.
  if (prerender::PrerenderHelper::IsPrerendering(render_frame()))
    return;

    // Don't capture contents unless there is either a translate agent or a
    // phishing classifier to consume them.
#if BUILDFLAG(SAFE_BROWSING_AVAILABLE)
  if (!translate_agent_ && !phishing_classifier_)
    return;
#else
  if (!translate_agent_)
    return;
#endif

  base::TimeTicks capture_begin_time = base::TimeTicks::Now();

  // Retrieve the frame's full text (up to kMaxIndexChars), and pass it to the
  // translate helper for language detection and possible translation.
  // TODO(dglazkov): WebFrameContentDumper should only be used for
  // testing purposes. See http://crbug.com/585164.
  base::string16 contents =
      WebFrameContentDumper::DeprecatedDumpFrameTreeAsText(frame,
                                                           kMaxIndexChars)
          .Utf16();

  UMA_HISTOGRAM_TIMES(kTranslateCaptureText,
                      base::TimeTicks::Now() - capture_begin_time);

  // We should run language detection only once. Parsing finishes before
  // the page loads, so let's pick that timing.
  if (translate_agent_ && capture_type == PRELIMINARY_CAPTURE) {
    translate_agent_->PageCaptured(contents);
  }

  TRACE_EVENT0("renderer", "ChromeRenderFrameObserver::CapturePageText");

#if BUILDFLAG(SAFE_BROWSING_AVAILABLE)
  // Will swap out the string.
  if (phishing_classifier_)
    phishing_classifier_->PageCaptured(&contents,
                                       capture_type == PRELIMINARY_CAPTURE);
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
      break;
    case chrome::mojom::ImageFormat::JPEG:
      return !base::EqualsCaseInsensitiveASCII(image_extension, kJpgExtension);
      break;
    case chrome::mojom::ImageFormat::ORIGINAL:
      return !base::EqualsCaseInsensitiveASCII(image_extension,
                                               kGifExtension) &&
             !base::EqualsCaseInsensitiveASCII(image_extension,
                                               kJpgExtension) &&
             !base::EqualsCaseInsensitiveASCII(image_extension, kPngExtension);
      break;
  }

  // Should never hit this code since all cases were handled above.
  NOTREACHED();
  return true;
}
