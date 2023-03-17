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
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/lens/lens_core_tab_side_panel_helper.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/chrome_render_frame.mojom.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/grit/generated_resources.h"
#include "components/lens/buildflags.h"
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
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/codec/webp_codec.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/android/tab_android.h"
#else
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "components/guest_view/browser/guest_view_manager.h"
#endif

#if BUILDFLAG(ENABLE_LENS_DESKTOP_GOOGLE_BRANDED_FEATURES)
#include "chrome/browser/ui/lens/lens_side_panel_helper.h"
#endif

using content::WebContents;

namespace {

constexpr int kImageSearchThumbnailMinSize = 300 * 300;
constexpr int kImageSearchThumbnailMaxWidth = 600;
constexpr int kImageSearchThumbnailMaxHeight = 600;
constexpr char kUnifiedSidePanelVersion[] = "1";
}  // namespace

CoreTabHelper::CoreTabHelper(WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<CoreTabHelper>(*web_contents) {}

CoreTabHelper::~CoreTabHelper() {}

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
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents());
  if (!browser)
    return;

  browser->command_controller()->ContentRestrictionsChanged();
#endif
}

lens::mojom::ImageFormat CoreTabHelper::EncodeImageIntoSearchArgs(
    const gfx::Image& image,
    TemplateURLRef::SearchTermsArgs& search_args) {
  std::vector<unsigned char> data;
  if (lens::features::IsWebpForRegionSearchEnabled() &&
      gfx::WebpCodec::Encode(image.AsBitmap(),
                             lens::features::GetRegionSearchEncodingQuality(),
                             &data)) {
    search_args.image_thumbnail_content.assign(data.begin(), data.end());
    search_args.image_thumbnail_content_type = "image/webp";
    return lens::mojom::ImageFormat::WEBP;
  } else if (lens::features::IsJpegForRegionSearchEnabled() &&
             gfx::JPEGCodec::Encode(
                 image.AsBitmap(),
                 lens::features::GetRegionSearchEncodingQuality(), &data)) {
    search_args.image_thumbnail_content.assign(data.begin(), data.end());
    search_args.image_thumbnail_content_type = "image/jpeg";
    return lens::mojom::ImageFormat::JPEG;
  } else {
    // If the WebP/JPEG encoding fails, fall back to PNG.
    // Get the front and end of the image bytes in order to store them in the
    // search_args to be sent as part of the PostContent in the request.
    size_t image_bytes_size = image.As1xPNGBytes()->size();
    const unsigned char* image_bytes_begin = image.As1xPNGBytes()->front();
    const unsigned char* image_bytes_end = image_bytes_begin + image_bytes_size;
    search_args.image_thumbnail_content.assign(image_bytes_begin,
                                               image_bytes_end);
    search_args.image_thumbnail_content_type = "image/png";
    return lens::mojom::ImageFormat::PNG;
  }
}

void CoreTabHelper::SearchWithLens(content::RenderFrameHost* render_frame_host,
                                   const GURL& src_url,
                                   lens::EntryPoint entry_point,
                                   bool is_image_translate) {
  bool use_side_panel = lens::IsSidePanelEnabledForLens(web_contents());

  SearchByImageImpl(render_frame_host, src_url, kImageSearchThumbnailMinSize,
                    lens::features::GetMaxPixelsForImageSearch(),
                    lens::features::GetMaxPixelsForImageSearch(),
                    lens::GetQueryParametersForLensRequest(
                        entry_point, use_side_panel,
                        /** is_full_screen_region_search_request **/ false),
                    use_side_panel, is_image_translate);
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

void CoreTabHelper::RegionSearchWithLens(
    gfx::Image image,
    const gfx::Size& image_original_size,
    std::vector<lens::mojom::LatencyLogPtr> log_data) {
  // Do not show the side panel on region searches and modify the entry point
  // if Lens fullscreen search features are enabled.
  bool is_full_screen_region_search_request =
      lens::features::IsLensFullscreenSearchEnabled();
  lens::EntryPoint entry_point =
      is_full_screen_region_search_request
          ? lens::EntryPoint::CHROME_FULLSCREEN_SEARCH_MENU_ITEM
          : lens::EntryPoint::CHROME_REGION_SEARCH_MENU_ITEM;
  bool use_side_panel =
      lens::IsSidePanelEnabledForLensRegionSearch(web_contents());

  auto lens_query_params = lens::GetQueryParametersForLensRequest(
      entry_point, use_side_panel,
      /* is_full_screen_region_search_request= */
      is_full_screen_region_search_request);
  SearchByImageImpl(image, image_original_size, lens_query_params,
                    use_side_panel, std::move(log_data));
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
                    lens::IsSidePanelEnabledFor3PDse(web_contents()),
                    is_image_translate);
}

void CoreTabHelper::SearchByImage(const gfx::Image& image,
                                  const gfx::Size& image_original_size) {
  SearchByImageImpl(image, image_original_size,
                    /*additional_query_params=*/std::string(),
                    lens::IsSidePanelEnabledFor3PDse(web_contents()),
                    std::vector<lens::mojom::LatencyLogPtr>());
}

void CoreTabHelper::SearchByImageImpl(
    const gfx::Image& image,
    const gfx::Size& image_original_size,
    const std::string& additional_query_params,
    bool use_side_panel,
    std::vector<lens::mojom::LatencyLogPtr> log_data) {
  TemplateURLService* template_url_service = GetTemplateURLService();
  const TemplateURL* const default_provider =
      template_url_service->GetDefaultSearchProvider();
  DCHECK(default_provider);

  TemplateURLRef::SearchTermsArgs search_args =
      TemplateURLRef::SearchTermsArgs(std::u16string());

  log_data.push_back(lens::mojom::LatencyLog::New(
      lens::mojom::Phase::ENCODE_START, image_original_size, gfx::Size(),
      lens::mojom::ImageFormat::ORIGINAL, base::Time::Now()));

  lens::mojom::ImageFormat image_format =
      EncodeImageIntoSearchArgs(image, search_args);

  log_data.push_back(lens::mojom::LatencyLog::New(
      lens::mojom::Phase::ENCODE_END, image_original_size, gfx::Size(),
      image_format, base::Time::Now()));

  std::string additional_query_params_modified = additional_query_params;
  if (lens::features::GetEnableLatencyLogging() &&
      search::DefaultSearchProviderIsGoogle(template_url_service)) {
    lens::AppendLogsQueryParam(&additional_query_params_modified,
                               std::move(log_data));
  }

  search_args.image_original_size = image_original_size;
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
    int thumbnail_min_size,
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
  thumbnail_capturer_proxy->RequestImageForContextNode(
      thumbnail_min_size, gfx::Size(thumbnail_max_width, thumbnail_max_height),
      lens::features::IsWebpForImageSearchEnabled()
          ? chrome::mojom::ImageFormat::WEBP
          : chrome::mojom::ImageFormat::JPEG,
      lens::features::GetImageSearchEncodingQuality(),
      base::BindOnce(
          &CoreTabHelper::DoSearchByImage, weak_factory_.GetWeakPtr(),
          std::move(chrome_render_frame), src_url, additional_query_params,
          use_side_panel, is_image_translate,
          lens::features::IsWebpForImageSearchEnabled() ? "image/webp"
                                                        : "image/jpeg"));
}

std::unique_ptr<content::WebContents> CoreTabHelper::SwapWebContents(
    std::unique_ptr<content::WebContents> new_contents,
    bool did_start_load,
    bool did_finish_load) {
#if BUILDFLAG(IS_ANDROID)
  TabAndroid* tab = TabAndroid::FromWebContents(web_contents());
  return tab->SwapWebContents(std::move(new_contents), did_start_load,
                              did_finish_load);
#else
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents());
  return browser->SwapWebContents(web_contents(), std::move(new_contents));
#endif
}

// static
bool CoreTabHelper::GetStatusTextForWebContents(std::u16string* status_text,
                                                content::WebContents* source) {
#if BUILDFLAG(IS_ANDROID)
  NOTREACHED() << "If this ends up being used on Android update "
               << "ChromeContentBrowserClient::OverrideURLLoaderFactoryParams.";
  return false;
#else
#if BUILDFLAG(ENABLE_EXTENSIONS)
  auto* guest_manager = guest_view::GuestViewManager::FromBrowserContext(
      source->GetBrowserContext());
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
  if (!source->IsLoading() ||
      source->GetLoadState().state == net::LOAD_STATE_IDLE) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
    if (!guest_manager)
      return false;
    return guest_manager->ForEachGuest(
        source, base::BindRepeating(&CoreTabHelper::GetStatusTextForWebContents,
                                    status_text));
#else  // !BUILDFLAG(ENABLE_EXTENSIONS)
    return false;
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
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
#if BUILDFLAG(ENABLE_EXTENSIONS)
  if (!guest_manager)
    return false;

  return guest_manager->ForEachGuest(
      source, base::BindRepeating(&CoreTabHelper::GetStatusTextForWebContents,
                                  status_text));
#else  // !BUILDFLAG(ENABLE_EXTENSIONS)
  return false;
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
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
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents());
  if (browser)
    browser->command_controller()->WebContentsFocusChanged();
#endif  // BUILDFLAG(IS_ANDROID)
}

void CoreTabHelper::OnWebContentsLostFocus(
    content::RenderWidgetHost* render_widget_host) {
#if !BUILDFLAG(IS_ANDROID)
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents());
  if (browser)
    browser->command_controller()->WebContentsFocusChanged();
#endif  // BUILDFLAG(IS_ANDROID)
}

// Handles the image thumbnail for the context node, composes a image search
// request based on the received thumbnail and opens the request in a new tab.
void CoreTabHelper::DoSearchByImage(
    mojo::AssociatedRemote<chrome::mojom::ChromeRenderFrame>
        chrome_render_frame,
    const GURL& src_url,
    const std::string& additional_query_params,
    bool use_side_panel,
    bool is_image_translate,
    const std::string& thumbnail_content_type,
    const std::vector<uint8_t>& thumbnail_data,
    const gfx::Size& original_size,
    const std::string& image_extension,
    const std::vector<lens::mojom::LatencyLogPtr> log_data) {
  if (thumbnail_data.empty())
    return;

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

  TemplateURLRef::SearchTermsArgs search_args =
      TemplateURLRef::SearchTermsArgs(std::u16string());
  search_args.image_thumbnail_content.assign(thumbnail_data.begin(),
                                             thumbnail_data.end());
  search_args.image_thumbnail_content_type = thumbnail_content_type;
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
    lens::OpenLensSidePanel(chrome::FindBrowserWithWebContents(web_contents()),
                            open_url_params);
#else
    web_contents()->OpenURL(open_url_params);
#endif  // BUILDFLAG(ENABLE_LENS_DESKTOP_GOOGLE_BRANDED_FEATURES)
  } else {
    web_contents()->OpenURL(open_url_params);
  }
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(CoreTabHelper);
