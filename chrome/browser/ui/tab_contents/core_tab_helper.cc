// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tab_contents/core_tab_helper.h"

#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/stringprintf.h"
#include "base/task/bind_post_task.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/chrome_render_frame.mojom.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/grit/generated_resources.h"
#include "components/lens/buildflags.h"
#include "components/lens/lens_constants.h"
#include "components/lens/lens_entrypoints.h"
#include "components/lens/lens_features.h"
#include "components/lens/lens_rendering_environment.h"
#include "components/lens/lens_url_utils.h"
#include "components/search/search.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "components/strings/grit/components_strings.h"
#include "components/translate/core/browser/language_state.h"
#include "components/translate/core/common/translate_constants.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "extensions/buildflags/buildflags.h"
#include "net/base/load_states.h"
#include "net/http/http_request_headers.h"
#include "skia/ext/image_operations.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/codec/webp_codec.h"
#include "ui/gfx/image/image_util.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/views/side_panel/companion/companion_tab_helper.h"
#include "chrome/browser/ui/views/side_panel/companion/companion_utils.h"
#include "chrome/browser/ui/views/side_panel/lens/lens_core_tab_side_panel_helper.h"
#endif

#if BUILDFLAG(ENABLE_LENS_DESKTOP_GOOGLE_BRANDED_FEATURES)
#include "chrome/browser/ui/views/lens/lens_side_panel_helper.h"
#endif

using content::WebContents;

namespace {

constexpr int kImageSearchThumbnailMinSize = 300 * 300;
constexpr int kImageSearchThumbnailMaxWidth = 600;
constexpr int kImageSearchThumbnailMaxHeight = 600;
constexpr char kUnifiedSidePanelVersion[] = "1";
constexpr int kEncodingQualityJpeg = 40;
constexpr int kEncodingQualityWebp = 45;

bool NeedsDownscale(gfx::Image image) {
  return (image.Height() * image.Width() > lens::kMaxAreaForImageSearch) &&
         (image.Width() > lens::kMaxPixelsForImageSearch ||
          image.Height() > lens::kMaxPixelsForImageSearch);
}

gfx::Image DownscaleImage(const gfx::Image& image) {
  return gfx::ResizedImageForMaxDimensions(
      image, lens::kMaxPixelsForImageSearch, lens::kMaxPixelsForImageSearch,
      lens::kMaxAreaForImageSearch);
}

#if BUILDFLAG(IS_ANDROID)
bool IsSidePanelEnabledForLens(content::WebContents* contents) {
  return false;
}
bool IsSidePanelEnabledFor3PDse(content::WebContents* contents) {
  return false;
}
#else
bool IsSidePanelEnabledForLens(content::WebContents* contents) {
  return lens::IsSidePanelEnabledForLens(contents);
}
bool IsSidePanelEnabledFor3PDse(content::WebContents* contents) {
  return lens::IsSidePanelEnabledFor3PDse(contents);
}
#endif

}  // namespace

CoreTabHelper::CoreTabHelper(WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<CoreTabHelper>(*web_contents) {}

CoreTabHelper::~CoreTabHelper() {}

// static
std::u16string CoreTabHelper::GetDefaultTitle() {
  return l10n_util::GetStringUTF16(IDS_DEFAULT_TAB_TITLE);
}

std::u16string CoreTabHelper::GetStatusText() const {
  std::u16string status_text;
  GetStatusTextForWebContents(&status_text, web_contents());
  return status_text;
}

void CoreTabHelper::UpdateContentRestrictions(int content_restrictions) {
  content_restrictions_ = content_restrictions;
#if !BUILDFLAG(IS_ANDROID)
  Browser* browser = chrome::FindBrowserWithTab(web_contents());
  if (!browser)
    return;

  browser->command_controller()->ContentRestrictionsChanged();
#endif
}

std::vector<unsigned char> CoreTabHelper::EncodeImage(
    const gfx::Image& image,
    std::string& content_type,
    lens::mojom::ImageFormat& image_format) {
  std::vector<unsigned char> data;

  if (gfx::JPEGCodec::Encode(image.AsBitmap(), kEncodingQualityJpeg, &data)) {
    content_type = "image/jpeg";
    image_format = lens::mojom::ImageFormat::JPEG;
    return data;
  }

  // Get the front and end of the image bytes in order to store them in the
  // search_args to be sent as part of the PostContent in the request
  content_type = "image/png";
  image_format = lens::mojom::ImageFormat::PNG;
  auto bytes = image.As1xPNGBytes();
  data.assign(bytes->begin(), bytes->end());
  return data;
}

// static
void CoreTabHelper::DownscaleAndEncodeBitmap(
    const SkBitmap& bitmap,
    int thumbnail_min_area,
    int thumbnail_max_width,
    int thumbnail_max_height,
    DownscaleAndEncodeBitmapCallback callback) {
  gfx::Size original_size;
  std::string content_type;
  gfx::Size downscaled_size;
  std::vector<lens::mojom::LatencyLogPtr> log_data;
  std::vector<unsigned char> thumbnail_data;

  if (bitmap.isNull()) {
    return std::move(callback).Run(thumbnail_data, content_type, original_size,
                                   downscaled_size, std::move(log_data));
  }

  original_size = gfx::Size(bitmap.width(), bitmap.height());
  gfx::SizeF scaled_size = gfx::SizeF(original_size);
  bool needs_downscale = false;

  if (original_size.GetArea() > thumbnail_min_area) {
    if (scaled_size.width() > thumbnail_max_width) {
      needs_downscale = true;
      scaled_size.Scale(thumbnail_max_width / scaled_size.width());
    }

    if (scaled_size.height() > thumbnail_max_height) {
      needs_downscale = true;
      scaled_size.Scale(thumbnail_max_height / scaled_size.height());
    }
  }

  SkBitmap thumbnail;
  if (needs_downscale) {
    log_data.push_back(lens::mojom::LatencyLog::New(
        lens::mojom::Phase::DOWNSCALE_START, original_size, gfx::Size(),
        lens::mojom::ImageFormat::ORIGINAL, base::Time::Now(),
        /*encoded_size_bytes=*/0));
    thumbnail = skia::ImageOperations::Resize(
        bitmap, skia::ImageOperations::RESIZE_GOOD,
        static_cast<int>(scaled_size.width()),
        static_cast<int>(scaled_size.height()));
    downscaled_size = gfx::Size(thumbnail.width(), thumbnail.height());
    log_data.push_back(lens::mojom::LatencyLog::New(
        lens::mojom::Phase::DOWNSCALE_END, original_size, downscaled_size,
        lens::mojom::ImageFormat::ORIGINAL, base::Time::Now(),
        /*encoded_size_bytes=*/0));
  } else {
    thumbnail = std::move(bitmap);
    downscaled_size = gfx::Size(original_size);
  }

  lens::mojom::ImageFormat encode_target_format;
  std::vector<unsigned char> encoded_data;
  log_data.push_back(lens::mojom::LatencyLog::New(
      lens::mojom::Phase::ENCODE_START, original_size, downscaled_size,
      lens::mojom::ImageFormat::ORIGINAL, base::Time::Now(),
      /*encoded_size_bytes=*/0));
  if (thumbnail.isOpaque() &&
      gfx::JPEGCodec::Encode(thumbnail, kEncodingQualityJpeg, &encoded_data)) {
    thumbnail_data.swap(encoded_data);
    content_type = "image/jpeg";
    encode_target_format = lens::mojom::ImageFormat::JPEG;
  } else if (gfx::WebpCodec::Encode(thumbnail, kEncodingQualityWebp,
                                    &encoded_data)) {
    thumbnail_data.swap(encoded_data);
    content_type = "image/webp";
    encode_target_format = lens::mojom::ImageFormat::WEBP;
  }
  log_data.push_back(lens::mojom::LatencyLog::New(
      lens::mojom::Phase::ENCODE_END, original_size, downscaled_size,
      encode_target_format, base::Time::Now(),
      /*encoded_size_bytes=*/sizeof(unsigned char) * thumbnail_data.size()));
  return std::move(callback).Run(thumbnail_data, content_type, original_size,
                                 downscaled_size, std::move(log_data));
}

// static
lens::mojom::ImageFormat CoreTabHelper::EncodeImageIntoSearchArgs(
    const gfx::Image& image,
    size_t& encoded_size_bytes,
    TemplateURLRef::SearchTermsArgs& search_args) {
  lens::mojom::ImageFormat image_format;
  std::string content_type;
  std::vector<unsigned char> data =
      EncodeImage(image, content_type, image_format);
  encoded_size_bytes = sizeof(unsigned char) * data.size();
  search_args.image_thumbnail_content.assign(data.begin(), data.end());
  search_args.image_thumbnail_content_type = content_type;
  return image_format;
}

void CoreTabHelper::SearchWithLens(content::RenderFrameHost* render_frame_host,
                                   const GURL& src_url,
                                   lens::EntryPoint entry_point,
                                   bool is_image_translate,
                                   bool force_open_in_new_tab) {
  bool use_side_panel =
      !force_open_in_new_tab && IsSidePanelEnabledForLens(web_contents());
  SearchByImageImpl(render_frame_host, src_url, kImageSearchThumbnailMinSize,
                    lens::kMaxPixelsForImageSearch,
                    lens::kMaxPixelsForImageSearch,
                    lens::GetQueryParametersForLensRequest(
                        entry_point, use_side_panel,
                        /*is_full_screen_request=*/false,
                        IsImageSearchSupportedForCompanion()),
                    use_side_panel, is_image_translate);
}

void CoreTabHelper::SearchWithLens(const gfx::Image& image,
                                   lens::EntryPoint entry_point,
                                   bool force_open_in_new_tab) {
  // Do not show the side panel on searches and modify the entry point if Lens
  // fullscreen search features are enabled.
  bool is_full_screen_request = lens::features::IsLensFullscreenSearchEnabled();
  lens::EntryPoint lens_entry_point =
      is_full_screen_request
          ? lens::EntryPoint::CHROME_FULLSCREEN_SEARCH_MENU_ITEM
          : entry_point;
  bool use_side_panel =
      !force_open_in_new_tab && IsSidePanelEnabledForLens(web_contents());
  bool is_companion_enabled = IsImageSearchSupportedForCompanion();

  auto lens_query_params = lens::GetQueryParametersForLensRequest(
      lens_entry_point, use_side_panel, is_full_screen_request,
      is_companion_enabled);

  SearchByImageImpl(image, lens_query_params, use_side_panel);
}

void CoreTabHelper::SearchByImage(content::RenderFrameHost* render_frame_host,
                                  const GURL& src_url) {
  SearchByImage(render_frame_host, src_url, /*is_image_translate=*/false);
}

void CoreTabHelper::SearchByImage(content::RenderFrameHost* render_frame_host,
                                  const GURL& src_url,
                                  bool is_image_translate) {
  SearchByImageImpl(render_frame_host, src_url, kImageSearchThumbnailMinSize,
                    kImageSearchThumbnailMaxWidth,
                    kImageSearchThumbnailMaxHeight, std::string(),
                    IsSidePanelEnabledFor3PDse(web_contents()),
                    is_image_translate);
}

void CoreTabHelper::SearchByImage(const gfx::Image& image) {
  SearchByImageImpl(image,
                    /*additional_query_params=*/std::string(),
                    IsSidePanelEnabledFor3PDse(web_contents()));
}

void CoreTabHelper::SearchByImageImpl(
    const gfx::Image& original_image,
    const std::string& additional_query_params,
    bool use_side_panel) {
  std::vector<lens::mojom::LatencyLogPtr> log_data;
  log_data.push_back(lens::mojom::LatencyLog::New(
      lens::mojom::Phase::OVERALL_START, gfx::Size(), gfx::Size(),
      lens::mojom::ImageFormat::ORIGINAL, base::Time::Now(),
      /*encoded_size_bytes=*/0));

  // Downscale the `original_image` if needed.
  gfx::Image image = original_image;
  if (NeedsDownscale(original_image)) {
    log_data.push_back(lens::mojom::LatencyLog::New(
        lens::mojom::Phase::DOWNSCALE_START, original_image.Size(), gfx::Size(),
        lens::mojom::ImageFormat::ORIGINAL, base::Time::Now(),
        /*encoded_size_bytes=*/0));

    image = DownscaleImage(original_image);

    log_data.push_back(lens::mojom::LatencyLog::New(
        lens::mojom::Phase::DOWNSCALE_END, original_image.Size(), image.Size(),
        lens::mojom::ImageFormat::ORIGINAL, base::Time::Now(),
        /*encoded_size_bytes=*/0));
  }

  TemplateURLService* template_url_service = GetTemplateURLService();
  const TemplateURL* const default_provider =
      template_url_service->GetDefaultSearchProvider();
  DCHECK(default_provider);
  bool is_companion_enabled = IsImageSearchSupportedForCompanion();

  TemplateURLRef::SearchTermsArgs search_args =
      TemplateURLRef::SearchTermsArgs(std::u16string());

  log_data.push_back(lens::mojom::LatencyLog::New(
      lens::mojom::Phase::ENCODE_START, original_image.Size(), gfx::Size(),
      lens::mojom::ImageFormat::ORIGINAL, base::Time::Now(),
      /*encoded_size_bytes=*/0));

  size_t encoded_size_bytes;
  std::string content_type;
  std::vector<unsigned char> encoded_image_bytes;
  lens::mojom::ImageFormat image_format;
  if (is_companion_enabled) {
    // We do not need to add the image to the search args when using the
    // companion.
    encoded_image_bytes = EncodeImage(image, content_type, image_format);
    encoded_size_bytes = sizeof(unsigned char) * encoded_image_bytes.size();
  } else {
    image_format =
        EncodeImageIntoSearchArgs(image, encoded_size_bytes, search_args);
  }
  log_data.push_back(lens::mojom::LatencyLog::New(
      lens::mojom::Phase::ENCODE_END, original_image.Size(), gfx::Size(),
      image_format, base::Time::Now(), encoded_size_bytes));

  std::string additional_query_params_modified = additional_query_params;
  if (lens::features::GetEnableLatencyLogging() &&
      search::DefaultSearchProviderIsGoogle(template_url_service)) {
    lens::AppendLogsQueryParam(&additional_query_params_modified,
                               std::move(log_data));
  }

#if !BUILDFLAG(IS_ANDROID)
  // If supported, launch image in the side panel.
  auto* companion_helper =
      companion::CompanionTabHelper::FromWebContents(web_contents());
  if (companion_helper && is_companion_enabled) {
    companion_helper->ShowCompanionSidePanelForImage(
        /*src_url=*/GURL(), /*is_image_translate=*/false,
        additional_query_params_modified, encoded_image_bytes,
        original_image.Size(), image.Size(), content_type);
    return;
  }
#endif

  if (search::DefaultSearchProviderIsGoogle(template_url_service)) {
    search_args.processed_image_dimensions =
        base::NumberToString(image.Size().width()) + "," +
        base::NumberToString(image.Size().height());
  }

  search_args.image_original_size = original_image.Size();
  search_args.additional_query_params = additional_query_params_modified;
  TemplateURLRef::PostContent post_content;
  GURL search_url(default_provider->image_url_ref().ReplaceSearchTerms(
      search_args, template_url_service->search_terms_data(), &post_content));
  if (use_side_panel) {
    search_url = template_url_service
                     ->GenerateSideImageSearchURLForDefaultSearchProvider(
                         search_url, kUnifiedSidePanelVersion);
  }
  PostContentToURL(post_content, search_url, use_side_panel);
}

void CoreTabHelper::SearchByImageImpl(
    content::RenderFrameHost* render_frame_host,
    const GURL& src_url,
    int thumbnail_min_area,
    int thumbnail_max_width,
    int thumbnail_max_height,
    const std::string& additional_query_params,
    bool use_side_panel,
    bool is_image_translate) {
  mojo::AssociatedRemote<chrome::mojom::ChromeRenderFrame> chrome_render_frame;
  render_frame_host->GetRemoteAssociatedInterfaces()->GetInterface(
      &chrome_render_frame);
  // Bind the InterfacePtr into the callback so that it's kept alive until
  // there's either a connection error or a response.
  auto* thumbnail_capturer_proxy = chrome_render_frame.get();
  thumbnail_capturer_proxy->RequestBitmapForContextNode(base::BindOnce(
      &CoreTabHelper::DoSearchByImageWithBitmap, weak_factory_.GetWeakPtr(),
      std::move(chrome_render_frame), src_url, additional_query_params,
      use_side_panel, is_image_translate, thumbnail_min_area,
      thumbnail_max_width, thumbnail_max_height));
}

// static
bool CoreTabHelper::GetStatusTextForWebContents(std::u16string* status_text,
                                                content::WebContents* source) {
#if BUILDFLAG(IS_ANDROID)
  NOTREACHED_IN_MIGRATION()
      << "If this ends up being used on Android update "
      << "ChromeContentBrowserClient::OverrideURLLoaderFactoryParams.";
  return false;
#else
  if (!source->IsLoading() ||
      source->GetLoadState().state == net::LOAD_STATE_IDLE) {
    return false;
  }

  switch (source->GetLoadState().state) {
    case net::LOAD_STATE_WAITING_FOR_STALLED_SOCKET_POOL:
    case net::LOAD_STATE_WAITING_FOR_AVAILABLE_SOCKET:
      *status_text =
          l10n_util::GetStringUTF16(IDS_LOAD_STATE_WAITING_FOR_SOCKET_SLOT);
      return true;
    case net::LOAD_STATE_WAITING_FOR_DELEGATE:
      if (!source->GetLoadState().param.empty()) {
        *status_text = l10n_util::GetStringFUTF16(
            IDS_LOAD_STATE_WAITING_FOR_DELEGATE,
            source->GetLoadState().param);
        return true;
      } else {
        *status_text = l10n_util::GetStringUTF16(
            IDS_LOAD_STATE_WAITING_FOR_DELEGATE_GENERIC);
        return true;
      }
    case net::LOAD_STATE_WAITING_FOR_CACHE:
      *status_text =
          l10n_util::GetStringUTF16(IDS_LOAD_STATE_WAITING_FOR_CACHE);
      return true;
    case net::LOAD_STATE_ESTABLISHING_PROXY_TUNNEL:
      *status_text =
          l10n_util::GetStringUTF16(IDS_LOAD_STATE_ESTABLISHING_PROXY_TUNNEL);
      return true;
    case net::LOAD_STATE_DOWNLOADING_PAC_FILE:
      *status_text =
          l10n_util::GetStringUTF16(IDS_LOAD_STATE_DOWNLOADING_PAC_FILE);
      return true;
    case net::LOAD_STATE_RESOLVING_PROXY_FOR_URL:
      *status_text =
          l10n_util::GetStringUTF16(IDS_LOAD_STATE_RESOLVING_PROXY_FOR_URL);
      return true;
    case net::LOAD_STATE_RESOLVING_HOST_IN_PAC_FILE:
      *status_text =
          l10n_util::GetStringUTF16(IDS_LOAD_STATE_RESOLVING_HOST_IN_PAC_FILE);
      return true;
    case net::LOAD_STATE_RESOLVING_HOST:
      *status_text = l10n_util::GetStringUTF16(IDS_LOAD_STATE_RESOLVING_HOST);
      return true;
    case net::LOAD_STATE_CONNECTING:
      *status_text = l10n_util::GetStringUTF16(IDS_LOAD_STATE_CONNECTING);
      return true;
    case net::LOAD_STATE_SSL_HANDSHAKE:
      *status_text = l10n_util::GetStringUTF16(IDS_LOAD_STATE_SSL_HANDSHAKE);
      return true;
    case net::LOAD_STATE_SENDING_REQUEST:
      if (source->GetUploadSize()) {
        *status_text = l10n_util::GetStringFUTF16Int(
            IDS_LOAD_STATE_SENDING_REQUEST_WITH_PROGRESS,
            static_cast<int>((100 * source->GetUploadPosition()) /
                source->GetUploadSize()));
        return true;
      } else {
        *status_text =
            l10n_util::GetStringUTF16(IDS_LOAD_STATE_SENDING_REQUEST);
        return true;
      }
    case net::LOAD_STATE_WAITING_FOR_RESPONSE:
      *status_text =
          l10n_util::GetStringFUTF16(IDS_LOAD_STATE_WAITING_FOR_RESPONSE,
                                     source->GetLoadStateHost());
      return true;
    // Ignore net::LOAD_STATE_READING_RESPONSE, net::LOAD_STATE_IDLE and
    // net::LOAD_STATE_OBSOLETE_WAITING_FOR_APPCACHE
    case net::LOAD_STATE_IDLE:
    case net::LOAD_STATE_READING_RESPONSE:
    case net::LOAD_STATE_OBSOLETE_WAITING_FOR_APPCACHE:
      break;
  }
  return false;
#endif  // BUILDFLAG(IS_ANDROID)
}

////////////////////////////////////////////////////////////////////////////////
// WebContentsObserver overrides

void CoreTabHelper::DidStartLoading() {
  UpdateContentRestrictions(0);
}

// Update back/forward buttons for web_contents that are active.
void CoreTabHelper::NavigationEntriesDeleted() {
#if !BUILDFLAG(IS_ANDROID)
  for (Browser* browser : *BrowserList::GetInstance()) {
    if (web_contents() == browser->tab_strip_model()->GetActiveWebContents())
      browser->command_controller()->TabStateChanged();
  }
#endif
}

// Notify browser commands that depend on whether focus is in the
// web contents or not.
void CoreTabHelper::OnWebContentsFocused(
    content::RenderWidgetHost* render_widget_host) {
#if !BUILDFLAG(IS_ANDROID)
  Browser* browser = chrome::FindBrowserWithTab(web_contents());
  if (browser)
    browser->command_controller()->WebContentsFocusChanged();
#endif  // BUILDFLAG(IS_ANDROID)
}

void CoreTabHelper::OnWebContentsLostFocus(
    content::RenderWidgetHost* render_widget_host) {
#if !BUILDFLAG(IS_ANDROID)
  Browser* browser = chrome::FindBrowserWithTab(web_contents());
  if (browser)
    browser->command_controller()->WebContentsFocusChanged();
#endif  // BUILDFLAG(IS_ANDROID)
}

void CoreTabHelper::DoSearchByImageWithBitmap(
    mojo::AssociatedRemote<chrome::mojom::ChromeRenderFrame>
        chrome_render_frame,
    const GURL& src_url,
    const std::string& additional_query_params,
    bool use_side_panel,
    bool is_image_translate,
    int thumbnail_min_area,
    int thumbnail_max_width,
    int thumbnail_max_height,
    const SkBitmap& bitmap) {
  base::ThreadPool::PostTask(base::BindOnce(
      &CoreTabHelper::DownscaleAndEncodeBitmap, bitmap, thumbnail_min_area,
      thumbnail_max_width, thumbnail_max_height,
      base::BindPostTask(base::SequencedTaskRunner::GetCurrentDefault(),
                         base::BindOnce(&CoreTabHelper::DoSearchByImage,
                                        weak_factory_.GetWeakPtr(), src_url,
                                        additional_query_params, use_side_panel,
                                        is_image_translate))));
}

void CoreTabHelper::DoSearchByImage(
    const GURL& src_url,
    const std::string& additional_query_params,
    bool use_side_panel,
    bool is_image_translate,
    const std::vector<unsigned char>& thumbnail_data,
    const std::string& content_type,
    const gfx::Size& original_size,
    const gfx::Size& downscaled_size,
    const std::vector<lens::mojom::LatencyLogPtr> log_data) {
  if (thumbnail_data.empty()) {
    return;
  }

  TemplateURLService* template_url_service = GetTemplateURLService();
  const TemplateURL* const default_provider =
      template_url_service->GetDefaultSearchProvider();
  DCHECK(default_provider);

  std::string additional_query_params_modified = additional_query_params;
  if (lens::features::GetEnableLatencyLogging() &&
      search::DefaultSearchProviderIsGoogle(template_url_service)) {
    lens::AppendLogsQueryParam(&additional_query_params_modified,
                               std::move(log_data));
  }

#if !BUILDFLAG(IS_ANDROID)
  auto* companion_helper =
      companion::CompanionTabHelper::FromWebContents(web_contents());
  if (companion_helper && IsImageSearchSupportedForCompanion()) {
    companion_helper->ShowCompanionSidePanelForImage(
        src_url, is_image_translate, additional_query_params_modified,
        thumbnail_data, original_size, downscaled_size, content_type);
    return;
  }
#endif

  TemplateURLRef::SearchTermsArgs search_args =
      TemplateURLRef::SearchTermsArgs(std::u16string());
  if (search::DefaultSearchProviderIsGoogle(template_url_service)) {
    search_args.processed_image_dimensions =
        base::NumberToString(downscaled_size.width()) + "," +
        base::NumberToString(downscaled_size.height());
  }

  search_args.image_thumbnail_content.assign(thumbnail_data.begin(),
                                             thumbnail_data.end());
  search_args.image_thumbnail_content_type = content_type;
  search_args.image_url = src_url;
  search_args.image_original_size = original_size;
  search_args.additional_query_params = additional_query_params_modified;
  if (is_image_translate) {
    MaybeSetSearchArgsForImageTranslate(search_args);
  }
  TemplateURLRef::PostContent post_content;
  const TemplateURLRef& template_url =
      is_image_translate ? default_provider->image_translate_url_ref()
                         : default_provider->image_url_ref();
  GURL search_url(template_url.ReplaceSearchTerms(
      search_args, template_url_service->search_terms_data(), &post_content));
  if (use_side_panel) {
    search_url = template_url_service
                     ->GenerateSideImageSearchURLForDefaultSearchProvider(
                         search_url, kUnifiedSidePanelVersion);
  }

  PostContentToURL(post_content, search_url, use_side_panel);
}

TemplateURLService* CoreTabHelper::GetTemplateURLService() {
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  DCHECK(profile);
  TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile);
  DCHECK(template_url_service);
  return template_url_service;
}

bool CoreTabHelper::IsImageSearchSupportedForCompanion() {
#if !BUILDFLAG(IS_ANDROID)
  Browser* browser = chrome::FindBrowserWithTab(web_contents());
  if (browser) {
    return companion::IsSearchImageInCompanionSidePanelSupported(browser);
  }
#endif
  return false;
}

void CoreTabHelper::MaybeSetSearchArgsForImageTranslate(
    TemplateURLRef::SearchTermsArgs& search_args) {
  ChromeTranslateClient* chrome_translate_client =
      ChromeTranslateClient::FromWebContents(web_contents());
  if (!chrome_translate_client) {
    return;
  }
  const translate::LanguageState& language_state =
      chrome_translate_client->GetLanguageState();
  if (language_state.IsPageTranslated()) {
    if (language_state.source_language() != translate::kUnknownLanguageCode) {
      search_args.image_translate_source_locale =
          language_state.source_language();
    }
    if (language_state.current_language() != translate::kUnknownLanguageCode) {
      search_args.image_translate_target_locale =
          language_state.current_language();
    }
  }
}

void CoreTabHelper::PostContentToURL(TemplateURLRef::PostContent post_content,
                                     GURL url,
                                     bool use_side_panel) {
  if (!url.is_valid())
    return;
  content::OpenURLParams open_url_params(
      url, content::Referrer(), WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui::PAGE_TRANSITION_LINK, false);
  const std::string& content_type = post_content.first;
  const std::string& post_data = post_content.second;
  if (!post_data.empty()) {
    DCHECK(!content_type.empty());
    open_url_params.post_data = network::ResourceRequestBody::CreateFromBytes(
        post_data.data(), post_data.size());
    open_url_params.extra_headers += base::StringPrintf(
        "%s: %s\r\n", net::HttpRequestHeaders::kContentType,
        content_type.c_str());
  }

  if (use_side_panel) {
#if BUILDFLAG(ENABLE_LENS_DESKTOP_GOOGLE_BRANDED_FEATURES)
    lens::OpenLensSidePanel(chrome::FindBrowserWithTab(web_contents()),
                            open_url_params);
#else
    web_contents()->OpenURL(open_url_params, /*navigation_handle_callback=*/{});
#endif  // BUILDFLAG(ENABLE_LENS_DESKTOP_GOOGLE_BRANDED_FEATURES)
  } else {
    web_contents()->OpenURL(open_url_params, /*navigation_handle_callback=*/{});
  }
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(CoreTabHelper);
