// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/searchbox/searchbox_extension.h"

#include <stddef.h>
#include <stdint.h>
#include <string>
#include <vector>

#include "base/i18n/rtl.h"
#include "base/json/string_escape.h"
#include "base/macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/common/search/instant_types.h"
#include "chrome/common/search/ntp_logging_events.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/renderer_resources.h"
#include "chrome/renderer/searchbox/searchbox.h"
#include "components/crx_file/id_util.h"
#include "components/ntp_tiles/constants.h"
#include "components/ntp_tiles/ntp_tile_impression.h"
#include "components/ntp_tiles/tile_source.h"
#include "components/ntp_tiles/tile_visual_type.h"
#include "content/public/common/page_zoom.h"
#include "content/public/renderer/chrome_object_extensions_utils.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_view.h"
#include "gin/data_object_builder.h"
#include "gin/handle.h"
#include "gin/object_template_builder.h"
#include "gin/wrappable.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/public/web/blink.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_script_source.h"
#include "third_party/blink/public/web/web_view.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/window_open_disposition.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/text_constants.h"
#include "ui/gfx/text_elider.h"
#include "url/gurl.h"
#include "url/url_constants.h"
#include "v8/include/v8.h"

namespace internal {  // for testing.

// Returns an array with the RGBA color components.
v8::Local<v8::Value> RGBAColorToArray(v8::Isolate* isolate,
                                      const RGBAColor& color) {
  v8::Local<v8::Array> color_array = v8::Array::New(isolate, 4);
  color_array->Set(0, v8::Int32::New(isolate, color.r));
  color_array->Set(1, v8::Int32::New(isolate, color.g));
  color_array->Set(2, v8::Int32::New(isolate, color.b));
  color_array->Set(3, v8::Int32::New(isolate, color.a));
  return color_array;
}

}  // namespace internal

namespace {

const char kCSSBackgroundImageFormat[] = "-webkit-image-set("
    "url(chrome-search://theme/IDR_THEME_NTP_BACKGROUND?%s) 1x, "
    "url(chrome-search://theme/IDR_THEME_NTP_BACKGROUND@2x?%s) 2x)";

const char kCSSBackgroundColorFormat[] = "rgba(%d,%d,%d,%s)";

const char kCSSBackgroundPositionCenter[] = "center";
const char kCSSBackgroundPositionLeft[] = "left";
const char kCSSBackgroundPositionTop[] = "top";
const char kCSSBackgroundPositionRight[] = "right";
const char kCSSBackgroundPositionBottom[] = "bottom";

const char kCSSBackgroundRepeatNo[] = "no-repeat";
const char kCSSBackgroundRepeatX[] = "repeat-x";
const char kCSSBackgroundRepeatY[] = "repeat-y";
const char kCSSBackgroundRepeat[] = "repeat";

const char kThemeAttributionFormat[] = "-webkit-image-set("
    "url(chrome-search://theme/IDR_THEME_NTP_ATTRIBUTION?%s) 1x, "
    "url(chrome-search://theme/IDR_THEME_NTP_ATTRIBUTION@2x?%s) 2x)";

const char kLTRHtmlTextDirection[] = "ltr";
const char kRTLHtmlTextDirection[] = "rtl";

// Max character limit for custom link titles.
const size_t kMaxCustomLinkTitleLength = 150;

void Dispatch(blink::WebLocalFrame* frame, const blink::WebString& script) {
  if (!frame)
    return;
  frame->ExecuteScript(blink::WebScriptSource(script));
}

std::string GenerateThumbnailURL(int render_view_id,
                                 InstantRestrictedID most_visited_item_id) {
  return base::StringPrintf("chrome-search://thumb/%d/%d", render_view_id,
                            most_visited_item_id);
}

std::string GenerateThumb2URL(const GURL& page_url,
                              const GURL& fallback_thumb_url) {
  return base::StringPrintf("chrome-search://thumb2/%s?fb=%s",
                            page_url.spec().c_str(),
                            fallback_thumb_url.spec().c_str());
}

// Populates a Javascript MostVisitedItem object for returning from
// newTabPage.mostVisited. This does not include private data such as "url" or
// "title".
v8::Local<v8::Object> GenerateMostVisitedItem(
    v8::Isolate* isolate,
    float device_pixel_ratio,
    int render_view_id,
    InstantRestrictedID restricted_id) {
  return gin::DataObjectBuilder(isolate)
      .Set("rid", restricted_id)
      .Set("faviconUrl", base::StringPrintf(
                             "chrome-search://favicon/size/16@%fx/%d/%d",
                             device_pixel_ratio, render_view_id, restricted_id))
      .Build();
}

// Populates a Javascript MostVisitedItem object appropriate for returning from
// newTabPage.getMostVisitedItemData.
// NOTE: Includes private data such as "url", "title", and "domain", so this
// should not be returned to the host page (via newTabPage.mostVisited). It is
// only accessible to most-visited iframes via getMostVisitedItemData.
v8::Local<v8::Object> GenerateMostVisitedItemData(
    v8::Isolate* isolate,
    int render_view_id,
    InstantRestrictedID restricted_id,
    const InstantMostVisitedItem& mv_item) {
  // We set the "dir" attribute of the title, so that in RTL locales, a LTR
  // title is rendered left-to-right and truncated from the right. For
  // example, the title of http://msdn.microsoft.com/en-us/default.aspx is
  // "MSDN: Microsoft developer network". In RTL locales, in the New Tab
  // page, if the "dir" of this title is not specified, it takes Chrome UI's
  // directionality. So the title will be truncated as "soft developer
  // network". Setting the "dir" attribute as "ltr" renders the truncated
  // title as "MSDN: Microsoft D...". As another example, the title of
  // http://yahoo.com is "Yahoo!". In RTL locales, in the New Tab page, the
  // title will be rendered as "!Yahoo" if its "dir" attribute is not set to
  // "ltr".
  const char* direction;
  if (base::i18n::StringContainsStrongRTLChars(mv_item.title))
    direction = kRTLHtmlTextDirection;
  else
    direction = kLTRHtmlTextDirection;

  std::string title = base::UTF16ToUTF8(mv_item.title);
  if (title.empty())
    title = mv_item.url.spec();

  // If the suggestion already has a suggested thumbnail, we create a thumbnail
  // URL with both the local thumbnail and the proposed one as a fallback.
  // Otherwise, we just pass on the generated one.
  std::string thumbnail_url =
      mv_item.thumbnail.is_valid()
          ? GenerateThumb2URL(mv_item.url, mv_item.thumbnail)
          : GenerateThumbnailURL(render_view_id, restricted_id);

  gin::DataObjectBuilder builder(isolate);
  builder.Set("renderViewId", render_view_id)
      .Set("rid", restricted_id)
      .Set("thumbnailUrl", thumbnail_url)
      .Set("tileTitleSource", static_cast<int>(mv_item.title_source))
      .Set("tileSource", static_cast<int>(mv_item.source))
      .Set("title", title)
      .Set("domain", mv_item.url.host())
      .Set("direction", base::StringPiece(direction))
      .Set("url", mv_item.url.spec())
      .Set("dataGenerationTime",
           mv_item.data_generation_time.is_null()
               ? v8::Local<v8::Value>(v8::Null(isolate))
               : v8::Date::New(isolate->GetCurrentContext(),
                               mv_item.data_generation_time.ToJsTime())
                     .ToLocalChecked());

  // If the suggestion already has a favicon, we populate the element with it.
  if (!mv_item.favicon.spec().empty())
    builder.Set("faviconUrl", mv_item.favicon.spec());

  return builder.Build();
}

base::Time ConvertDateValueToTime(v8::Value* value) {
  DCHECK(value);
  if (value->IsNull() || !value->IsDate())
    return base::Time();

  return base::Time::FromJsTime(v8::Date::Cast(value)->ValueOf());
}

base::Optional<int> CoerceToInt(v8::Isolate* isolate, v8::Value* value) {
  DCHECK(value);
  v8::MaybeLocal<v8::Int32> maybe_int =
      value->ToInt32(isolate->GetCurrentContext());
  if (maybe_int.IsEmpty())
    return base::nullopt;
  return maybe_int.ToLocalChecked()->Value();
}

v8::Local<v8::Object> GenerateThemeBackgroundInfo(
    v8::Isolate* isolate,
    const ThemeBackgroundInfo& theme_info) {
  gin::DataObjectBuilder builder(isolate);

  builder.Set("usingDefaultTheme", theme_info.using_default_theme);

  // The theme background color is in RGBA format "rgba(R,G,B,A)" where R, G and
  // B are between 0 and 255 inclusive, and A is a double between 0 and 1
  // inclusive.
  // This is the CSS "background-color" format.
  // Value is always valid.
  // TODO(jfweitz): Remove this field after GWS is modified to use the new
  // backgroundColorRgba field.
  builder.Set(
      "colorRgba",
      // Convert the alpha using NumberToString because StringPrintf will
      // use locale specific formatters (e.g., use , instead of . in
      // German).
      base::StringPrintf(
          kCSSBackgroundColorFormat, theme_info.background_color.r,
          theme_info.background_color.g, theme_info.background_color.b,
          base::NumberToString(theme_info.background_color.a / 255.0).c_str()));

  // Theme color for background as an array with the RGBA components in order.
  // Value is always valid.
  builder.Set("backgroundColorRgba",
              internal::RGBAColorToArray(isolate, theme_info.background_color));

  // Theme color for text as an array with the RGBA components in order.
  // Value is always valid.
  builder.Set("textColorRgba",
              internal::RGBAColorToArray(isolate, theme_info.text_color));

  // Theme color for links as an array with the RGBA components in order.
  // Value is always valid.
  builder.Set("linkColorRgba",
              internal::RGBAColorToArray(isolate, theme_info.link_color));

  // Theme color for light text as an array with the RGBA components in order.
  // Value is always valid.
  builder.Set("textColorLightRgba",
              internal::RGBAColorToArray(isolate, theme_info.text_color_light));

  // Theme color for header as an array with the RGBA components in order.
  // Value is always valid.
  builder.Set("headerColorRgba",
              internal::RGBAColorToArray(isolate, theme_info.header_color));

  // Theme color for section border as an array with the RGBA components in
  // order. Value is always valid.
  builder.Set(
      "sectionBorderColorRgba",
      internal::RGBAColorToArray(isolate, theme_info.section_border_color));

  // The theme alternate logo value indicates a white logo when TRUE and a
  // colorful one when FALSE.
  builder.Set("alternateLogo", theme_info.logo_alternate);

  // The theme background image url is of format kCSSBackgroundImageFormat
  // where both instances of "%s" are replaced with the id that identifies the
  // theme.
  // This is the CSS "background-image" format.
  // Value is only valid if there's a custom theme background image.
  if (crx_file::id_util::IdIsValid(theme_info.theme_id)) {
    builder.Set("imageUrl", base::StringPrintf(kCSSBackgroundImageFormat,
                                               theme_info.theme_id.c_str(),
                                               theme_info.theme_id.c_str()));

    // The theme background image horizontal alignment is one of "left",
    // "right", "center".
    // This is the horizontal component of the CSS "background-position" format.
    // Value is only valid if |imageUrl| is not empty.
    std::string alignment = kCSSBackgroundPositionCenter;
    if (theme_info.image_horizontal_alignment ==
        THEME_BKGRND_IMAGE_ALIGN_LEFT) {
      alignment = kCSSBackgroundPositionLeft;
    } else if (theme_info.image_horizontal_alignment ==
               THEME_BKGRND_IMAGE_ALIGN_RIGHT) {
      alignment = kCSSBackgroundPositionRight;
    }
    builder.Set("imageHorizontalAlignment", alignment);

    // The theme background image vertical alignment is one of "top", "bottom",
    // "center".
    // This is the vertical component of the CSS "background-position" format.
    // Value is only valid if |image_url| is not empty.
    if (theme_info.image_vertical_alignment == THEME_BKGRND_IMAGE_ALIGN_TOP) {
      alignment = kCSSBackgroundPositionTop;
    } else if (theme_info.image_vertical_alignment ==
               THEME_BKGRND_IMAGE_ALIGN_BOTTOM) {
      alignment = kCSSBackgroundPositionBottom;
    } else {
      alignment = kCSSBackgroundPositionCenter;
    }
    builder.Set("imageVerticalAlignment", alignment);

    // The tiling of the theme background image is one of "no-repeat",
    // "repeat-x", "repeat-y", "repeat".
    // This is the CSS "background-repeat" format.
    // Value is only valid if |image_url| is not empty.
    std::string tiling = kCSSBackgroundRepeatNo;
    switch (theme_info.image_tiling) {
      case THEME_BKGRND_IMAGE_NO_REPEAT:
        tiling = kCSSBackgroundRepeatNo;
        break;
      case THEME_BKGRND_IMAGE_REPEAT_X:
        tiling = kCSSBackgroundRepeatX;
        break;
      case THEME_BKGRND_IMAGE_REPEAT_Y:
        tiling = kCSSBackgroundRepeatY;
        break;
      case THEME_BKGRND_IMAGE_REPEAT:
        tiling = kCSSBackgroundRepeat;
        break;
    }
    builder.Set("imageTiling", tiling);

    // The attribution URL is only valid if the theme has attribution logo.
    if (theme_info.has_attribution) {
      builder.Set("attributionUrl",
                  base::StringPrintf(kThemeAttributionFormat,
                                     theme_info.theme_id.c_str(),
                                     theme_info.theme_id.c_str()));
    }
  }

  // Assume that a custom background has not been configured and then
  // override based on the condition below.
  builder.Set("customBackgroundConfigured", false);

  // If a custom background has been set provide the relevant information to the
  // page.
  if (!theme_info.custom_background_url.is_empty()) {
    builder.Set("alternateLogo", true);
    RGBAColor whiteTextRgba = RGBAColor{255, 255, 255, 255};
    builder.Set("textColorRgba",
                internal::RGBAColorToArray(isolate, whiteTextRgba));
    builder.Set("customBackgroundConfigured", true);
    builder.Set("imageUrl", theme_info.custom_background_url.spec());
    builder.Set("attributionActionUrl",
                theme_info.custom_background_attribution_action_url.spec());
    builder.Set("attribution1",
                theme_info.custom_background_attribution_line_1);
    builder.Set("attribution2",
                theme_info.custom_background_attribution_line_2);
  }

  return builder.Build();
}

content::RenderFrame* GetMainRenderFrameForCurrentContext() {
  blink::WebLocalFrame* frame = blink::WebLocalFrame::FrameForCurrentContext();
  if (!frame)
    return nullptr;
  content::RenderFrame* main_frame =
      content::RenderFrame::FromWebFrame(frame->LocalRoot());
  if (!main_frame || !main_frame->IsMainFrame())
    return nullptr;
  return main_frame;
}

SearchBox* GetSearchBoxForCurrentContext() {
  content::RenderFrame* main_frame = GetMainRenderFrameForCurrentContext();
  if (!main_frame)
    return nullptr;
  return SearchBox::Get(main_frame);
}

static const char kDispatchChromeIdentityCheckResult[] =
    "if (window.chrome &&"
    "    window.chrome.embeddedSearch &&"
    "    window.chrome.embeddedSearch.newTabPage &&"
    "    window.chrome.embeddedSearch.newTabPage.onsignedincheckdone &&"
    "    typeof window.chrome.embeddedSearch.newTabPage"
    "        .onsignedincheckdone === 'function') {"
    "  window.chrome.embeddedSearch.newTabPage.onsignedincheckdone(%s, %s);"
    "  true;"
    "}";

static const char kDispatchFocusChangedScript[] =
    "if (window.chrome &&"
    "    window.chrome.embeddedSearch &&"
    "    window.chrome.embeddedSearch.searchBox &&"
    "    window.chrome.embeddedSearch.searchBox.onfocuschange &&"
    "    typeof window.chrome.embeddedSearch.searchBox.onfocuschange =="
    "         'function') {"
    "  window.chrome.embeddedSearch.searchBox.onfocuschange();"
    "  true;"
    "}";

static const char kDispatchHistorySyncCheckResult[] =
    "if (window.chrome &&"
    "    window.chrome.embeddedSearch &&"
    "    window.chrome.embeddedSearch.newTabPage &&"
    "    window.chrome.embeddedSearch.newTabPage.onhistorysynccheckdone &&"
    "    typeof window.chrome.embeddedSearch.newTabPage"
    "        .onhistorysynccheckdone === 'function') {"
    "  window.chrome.embeddedSearch.newTabPage.onhistorysynccheckdone(%s);"
    "  true;"
    "}";

static const char kDispatchAddCustomLinkResult[] =
    "if (window.chrome &&"
    "    window.chrome.embeddedSearch &&"
    "    window.chrome.embeddedSearch.newTabPage &&"
    "    window.chrome.embeddedSearch.newTabPage.onaddcustomlinkdone &&"
    "    typeof window.chrome.embeddedSearch.newTabPage"
    "        .onaddcustomlinkdone === 'function') {"
    "  window.chrome.embeddedSearch.newTabPage.onaddcustomlinkdone(%s);"
    "  true;"
    "}";

static const char kDispatchUpdateCustomLinkResult[] =
    "if (window.chrome &&"
    "    window.chrome.embeddedSearch &&"
    "    window.chrome.embeddedSearch.newTabPage &&"
    "    window.chrome.embeddedSearch.newTabPage.onupdatecustomlinkdone &&"
    "    typeof window.chrome.embeddedSearch.newTabPage"
    "        .onupdatecustomlinkdone === 'function') {"
    "  window.chrome.embeddedSearch.newTabPage.onupdatecustomlinkdone(%s);"
    "  true;"
    "}";

static const char kDispatchDeleteCustomLinkResult[] =
    "if (window.chrome &&"
    "    window.chrome.embeddedSearch &&"
    "    window.chrome.embeddedSearch.newTabPage &&"
    "    window.chrome.embeddedSearch.newTabPage.ondeletecustomlinkdone &&"
    "    typeof window.chrome.embeddedSearch.newTabPage"
    "        .ondeletecustomlinkdone === 'function') {"
    "  window.chrome.embeddedSearch.newTabPage.ondeletecustomlinkdone(%s);"
    "  true;"
    "}";

static const char kDispatchDoesUrlResolveResultScript[] =
    "if (window.chrome &&"
    "    window.chrome.embeddedSearch &&"
    "    window.chrome.embeddedSearch.newTabPage &&"
    "    window.chrome.embeddedSearch.newTabPage.doesurlresolve &&"
    "    typeof window.chrome.embeddedSearch.newTabPage.doesurlresolve =="
    "        'function') {"
    "  window.chrome.embeddedSearch.newTabPage.doesurlresolve(%s);"
    "  true;"
    "}";

static const char kDispatchInputCancelScript[] =
    "if (window.chrome &&"
    "    window.chrome.embeddedSearch &&"
    "    window.chrome.embeddedSearch.newTabPage &&"
    "    window.chrome.embeddedSearch.newTabPage.oninputcancel &&"
    "    typeof window.chrome.embeddedSearch.newTabPage.oninputcancel =="
    "         'function') {"
    "  window.chrome.embeddedSearch.newTabPage.oninputcancel();"
    "  true;"
    "}";

static const char kDispatchInputStartScript[] =
    "if (window.chrome &&"
    "    window.chrome.embeddedSearch &&"
    "    window.chrome.embeddedSearch.newTabPage &&"
    "    window.chrome.embeddedSearch.newTabPage.oninputstart &&"
    "    typeof window.chrome.embeddedSearch.newTabPage.oninputstart =="
    "         'function') {"
    "  window.chrome.embeddedSearch.newTabPage.oninputstart();"
    "  true;"
    "}";

static const char kDispatchKeyCaptureChangeScript[] =
    "if (window.chrome &&"
    "    window.chrome.embeddedSearch &&"
    "    window.chrome.embeddedSearch.searchBox &&"
    "    window.chrome.embeddedSearch.searchBox.onkeycapturechange &&"
    "    typeof window.chrome.embeddedSearch.searchBox.onkeycapturechange =="
    "        'function') {"
    "  window.chrome.embeddedSearch.searchBox.onkeycapturechange();"
    "  true;"
    "}";

static const char kDispatchMostVisitedChangedScript[] =
    "if (window.chrome &&"
    "    window.chrome.embeddedSearch &&"
    "    window.chrome.embeddedSearch.newTabPage &&"
    "    window.chrome.embeddedSearch.newTabPage.onmostvisitedchange &&"
    "    typeof window.chrome.embeddedSearch.newTabPage.onmostvisitedchange =="
    "         'function') {"
    "  window.chrome.embeddedSearch.newTabPage.onmostvisitedchange();"
    "  true;"
    "}";

static const char kDispatchThemeChangeEventScript[] =
    "if (window.chrome &&"
    "    window.chrome.embeddedSearch &&"
    "    window.chrome.embeddedSearch.newTabPage &&"
    "    window.chrome.embeddedSearch.newTabPage.onthemechange &&"
    "    typeof window.chrome.embeddedSearch.newTabPage.onthemechange =="
    "        'function') {"
    "  window.chrome.embeddedSearch.newTabPage.onthemechange();"
    "  true;"
    "}";

// ----------------------------------------------------------------------------

class SearchBoxBindings : public gin::Wrappable<SearchBoxBindings> {
 public:
  static gin::WrapperInfo kWrapperInfo;

  SearchBoxBindings();
  ~SearchBoxBindings() override;

 private:
  // gin::Wrappable.
  gin::ObjectTemplateBuilder GetObjectTemplateBuilder(
      v8::Isolate* isolate) final;

  // Handlers for JS properties.
  static bool GetRightToLeft();
  static bool IsFocused();
  static bool IsKeyCaptureEnabled();

  // Handlers for JS functions.
  static void Paste(const std::string& text);
  static void StartCapturingKeyStrokes();
  static void StopCapturingKeyStrokes();

  DISALLOW_COPY_AND_ASSIGN(SearchBoxBindings);
};

gin::WrapperInfo SearchBoxBindings::kWrapperInfo = {gin::kEmbedderNativeGin};

SearchBoxBindings::SearchBoxBindings() = default;

SearchBoxBindings::~SearchBoxBindings() = default;

gin::ObjectTemplateBuilder SearchBoxBindings::GetObjectTemplateBuilder(
    v8::Isolate* isolate) {
  return gin::Wrappable<SearchBoxBindings>::GetObjectTemplateBuilder(isolate)
      .SetProperty("rtl", &SearchBoxBindings::GetRightToLeft)
      .SetProperty("isFocused", &SearchBoxBindings::IsFocused)
      .SetProperty("isKeyCaptureEnabled",
                   &SearchBoxBindings::IsKeyCaptureEnabled)
      .SetMethod("paste", &SearchBoxBindings::Paste)
      .SetMethod("startCapturingKeyStrokes",
                 &SearchBoxBindings::StartCapturingKeyStrokes)
      .SetMethod("stopCapturingKeyStrokes",
                 &SearchBoxBindings::StopCapturingKeyStrokes);
}

bool SearchBoxBindings::GetRightToLeft() {
  return base::i18n::IsRTL();
}

// static
bool SearchBoxBindings::IsFocused() {
  const SearchBox* search_box = GetSearchBoxForCurrentContext();
  if (!search_box)
    return false;
  return search_box->is_focused();
}

// static
bool SearchBoxBindings::IsKeyCaptureEnabled() {
  const SearchBox* search_box = GetSearchBoxForCurrentContext();
  if (!search_box)
    return false;
  return search_box->is_key_capture_enabled();
}

// static
void SearchBoxBindings::Paste(const std::string& text) {
  SearchBox* search_box = GetSearchBoxForCurrentContext();
  if (!search_box)
    return;
  search_box->Paste(base::UTF8ToUTF16(text));
}

// static
void SearchBoxBindings::StartCapturingKeyStrokes() {
  SearchBox* search_box = GetSearchBoxForCurrentContext();
  if (!search_box)
    return;
  search_box->StartCapturingKeyStrokes();
}

// static
void SearchBoxBindings::StopCapturingKeyStrokes() {
  SearchBox* search_box = GetSearchBoxForCurrentContext();
  if (!search_box)
    return;
  search_box->StopCapturingKeyStrokes();
}

class NewTabPageBindings : public gin::Wrappable<NewTabPageBindings> {
 public:
  static gin::WrapperInfo kWrapperInfo;

  NewTabPageBindings();
  ~NewTabPageBindings() override;

 private:
  // gin::Wrappable.
  gin::ObjectTemplateBuilder GetObjectTemplateBuilder(
      v8::Isolate* isolate) final;

  static bool HasOrigin(const GURL& origin);

  // Handlers for JS properties.
  static bool IsInputInProgress();
  static v8::Local<v8::Value> GetMostVisited(v8::Isolate* isolate);
  static bool GetMostVisitedAvailable(v8::Isolate* isolate);
  static v8::Local<v8::Value> GetThemeBackgroundInfo(v8::Isolate* isolate);
  static bool GetIsCustomLinks();

  // Handlers for JS functions visible to all NTPs.
  static void CheckIsUserSignedInToChromeAs(const std::string& identity);
  static void CheckIsUserSyncingHistory();
  static void DeleteMostVisitedItem(v8::Isolate* isolate,
                                    v8::Local<v8::Value> rid);
  static void UndoAllMostVisitedDeletions();
  static void UndoMostVisitedDeletion(v8::Isolate* isolate,
                                      v8::Local<v8::Value> rid);

  // Handlers for JS functions visible only to the most visited iframe, the edit
  // custom links iframe, and/or the local NTP.
  static v8::Local<v8::Value> GetMostVisitedItemData(v8::Isolate* isolate,
                                                     int rid);
  static void UpdateCustomLink(int rid,
                               const std::string& url,
                               const std::string& title);
  static void UndoCustomLinkAction();
  static void ResetCustomLinks();
  static std::string FixupAndValidateUrl(const std::string& url);
  static void LogEvent(int event);
  static void LogMostVisitedImpression(
      int position,
      int tile_title_source,
      int tile_source,
      int tile_type,
      v8::Local<v8::Value> data_generation_time);
  static void LogMostVisitedNavigation(
      int position,
      int tile_title_source,
      int tile_source,
      int tile_type,
      v8::Local<v8::Value> data_generation_time);
  static void SetCustomBackgroundURL(const std::string& background_url);
  static void SetCustomBackgroundURLWithAttributions(
      const std::string& background_url,
      const std::string& attribution_line_1,
      const std::string& attribution_line_2,
      const std::string& attributionActionUrl);
  static void SelectLocalBackgroundImage();

  DISALLOW_COPY_AND_ASSIGN(NewTabPageBindings);
};

gin::WrapperInfo NewTabPageBindings::kWrapperInfo = {gin::kEmbedderNativeGin};

NewTabPageBindings::NewTabPageBindings() = default;

NewTabPageBindings::~NewTabPageBindings() = default;

gin::ObjectTemplateBuilder NewTabPageBindings::GetObjectTemplateBuilder(
    v8::Isolate* isolate) {
  return gin::Wrappable<NewTabPageBindings>::GetObjectTemplateBuilder(isolate)
      .SetProperty("isInputInProgress", &NewTabPageBindings::IsInputInProgress)
      .SetProperty("mostVisited", &NewTabPageBindings::GetMostVisited)
      .SetProperty("mostVisitedAvailable",
                   &NewTabPageBindings::GetMostVisitedAvailable)
      .SetProperty("themeBackgroundInfo",
                   &NewTabPageBindings::GetThemeBackgroundInfo)
      .SetProperty("isCustomLinks", &NewTabPageBindings::GetIsCustomLinks)
      .SetMethod("checkIsUserSignedIntoChromeAs",
                 &NewTabPageBindings::CheckIsUserSignedInToChromeAs)
      .SetMethod("checkIsUserSyncingHistory",
                 &NewTabPageBindings::CheckIsUserSyncingHistory)
      .SetMethod("deleteMostVisitedItem",
                 &NewTabPageBindings::DeleteMostVisitedItem)
      .SetMethod("undoAllMostVisitedDeletions",
                 &NewTabPageBindings::UndoAllMostVisitedDeletions)
      .SetMethod("undoMostVisitedDeletion",
                 &NewTabPageBindings::UndoMostVisitedDeletion)
      .SetMethod("getMostVisitedItemData",
                 &NewTabPageBindings::GetMostVisitedItemData)
      .SetMethod("updateCustomLink", &NewTabPageBindings::UpdateCustomLink)
      .SetMethod("undoCustomLinkAction",
                 &NewTabPageBindings::UndoCustomLinkAction)
      .SetMethod("resetCustomLinks", &NewTabPageBindings::ResetCustomLinks)
      .SetMethod("fixupAndValidateUrl",
                 &NewTabPageBindings::FixupAndValidateUrl)
      .SetMethod("logEvent", &NewTabPageBindings::LogEvent)
      .SetMethod("logMostVisitedImpression",
                 &NewTabPageBindings::LogMostVisitedImpression)
      .SetMethod("logMostVisitedNavigation",
                 &NewTabPageBindings::LogMostVisitedNavigation)
      .SetMethod("setBackgroundURL",
                 &NewTabPageBindings::SetCustomBackgroundURL)
      .SetMethod("setBackgroundURLWithAttributions",
                 &NewTabPageBindings::SetCustomBackgroundURLWithAttributions)
      .SetMethod("selectLocalBackgroundImage",
                 &NewTabPageBindings::SelectLocalBackgroundImage);
}

// static
bool NewTabPageBindings::HasOrigin(const GURL& origin) {
  blink::WebLocalFrame* frame = blink::WebLocalFrame::FrameForCurrentContext();
  if (!frame)
    return false;
  GURL url(frame->GetDocument().Url());
  return url.GetOrigin() == origin.GetOrigin();
}

// static
bool NewTabPageBindings::IsInputInProgress() {
  SearchBox* search_box = GetSearchBoxForCurrentContext();
  if (!search_box)
    return false;
  return search_box->is_input_in_progress();
}

// static
v8::Local<v8::Value> NewTabPageBindings::GetMostVisited(v8::Isolate* isolate) {
  const SearchBox* search_box = GetSearchBoxForCurrentContext();
  if (!search_box)
    return v8::Null(isolate);

  content::RenderView* render_view =
      GetMainRenderFrameForCurrentContext()->GetRenderView();

  // This corresponds to "window.devicePixelRatio" in JavaScript.
  float zoom_factor =
      content::ZoomLevelToZoomFactor(render_view->GetZoomLevel());
  float device_pixel_ratio = render_view->GetDeviceScaleFactor() * zoom_factor;

  int render_view_id = render_view->GetRoutingID();

  std::vector<InstantMostVisitedItemIDPair> instant_mv_items;
  search_box->GetMostVisitedItems(&instant_mv_items);
  v8::Local<v8::Object> v8_mv_items =
      v8::Array::New(isolate, instant_mv_items.size());
  for (size_t i = 0; i < instant_mv_items.size(); ++i) {
    InstantRestrictedID rid = instant_mv_items[i].first;
    v8_mv_items->Set(i, GenerateMostVisitedItem(isolate, device_pixel_ratio,
                                                render_view_id, rid));
  }
  return v8_mv_items;
}

// static
bool NewTabPageBindings::GetMostVisitedAvailable(v8::Isolate* isolate) {
  const SearchBox* search_box = GetSearchBoxForCurrentContext();
  if (!search_box)
    return false;

  return search_box->AreMostVisitedItemsAvailable();
}

// static
v8::Local<v8::Value> NewTabPageBindings::GetThemeBackgroundInfo(
    v8::Isolate* isolate) {
  const SearchBox* search_box = GetSearchBoxForCurrentContext();
  if (!search_box)
    return v8::Null(isolate);
  const ThemeBackgroundInfo& theme_info = search_box->GetThemeBackgroundInfo();
  return GenerateThemeBackgroundInfo(isolate, theme_info);
}

// static
bool NewTabPageBindings::GetIsCustomLinks() {
  const SearchBox* search_box = GetSearchBoxForCurrentContext();
  if (!search_box || !HasOrigin(GURL(chrome::kChromeSearchMostVisitedUrl)))
    return false;

  return search_box->IsCustomLinks();
}

// static
void NewTabPageBindings::CheckIsUserSignedInToChromeAs(
    const std::string& identity) {
  SearchBox* search_box = GetSearchBoxForCurrentContext();
  if (!search_box)
    return;
  search_box->CheckIsUserSignedInToChromeAs(base::UTF8ToUTF16(identity));
}

// static
void NewTabPageBindings::CheckIsUserSyncingHistory() {
  SearchBox* search_box = GetSearchBoxForCurrentContext();
  if (!search_box)
    return;
  search_box->CheckIsUserSyncingHistory();
}

// static
void NewTabPageBindings::DeleteMostVisitedItem(v8::Isolate* isolate,
                                               v8::Local<v8::Value> rid_value) {
  // Manually convert to integer, so that the string "\"1\"" is also accepted.
  base::Optional<int> rid = CoerceToInt(isolate, *rid_value);
  if (!rid.has_value())
    return;
  SearchBox* search_box = GetSearchBoxForCurrentContext();
  if (!search_box)
    return;

  // Treat the Most Visited item as a custom link if called from the Most
  // Visited or edit custom link iframes, and if custom links is enabled. This
  // will initialize custom links if they have not already been initialized.
  if (ntp_tiles::IsCustomLinksEnabled() &&
      HasOrigin(GURL(chrome::kChromeSearchMostVisitedUrl))) {
    search_box->DeleteCustomLink(*rid);
    search_box->LogEvent(NTPLoggingEventType::NTP_CUSTOMIZE_SHORTCUT_REMOVE);
  } else {
    search_box->DeleteMostVisitedItem(*rid);
  }
}

// static
void NewTabPageBindings::UndoAllMostVisitedDeletions() {
  SearchBox* search_box = GetSearchBoxForCurrentContext();
  if (!search_box)
    return;
  search_box->UndoAllMostVisitedDeletions();
}

// static
void NewTabPageBindings::UndoMostVisitedDeletion(
    v8::Isolate* isolate,
    v8::Local<v8::Value> rid_value) {
  // Manually convert to integer, so that the string "\"1\"" is also accepted.
  base::Optional<int> rid = CoerceToInt(isolate, *rid_value);
  if (!rid.has_value())
    return;
  SearchBox* search_box = GetSearchBoxForCurrentContext();
  if (!search_box)
    return;

  search_box->UndoMostVisitedDeletion(*rid);
}

// static
v8::Local<v8::Value> NewTabPageBindings::GetMostVisitedItemData(
    v8::Isolate* isolate,
    int rid) {
  const SearchBox* search_box = GetSearchBoxForCurrentContext();
  if (!search_box || !HasOrigin(GURL(chrome::kChromeSearchMostVisitedUrl)))
    return v8::Null(isolate);

  InstantMostVisitedItem item;
  if (!search_box->GetMostVisitedItemWithID(rid, &item))
    return v8::Null(isolate);

  int render_view_id =
      GetMainRenderFrameForCurrentContext()->GetRenderView()->GetRoutingID();
  return GenerateMostVisitedItemData(isolate, render_view_id, rid, item);
}

// static
void NewTabPageBindings::UpdateCustomLink(int rid,
                                          const std::string& url,
                                          const std::string& title) {
  if (!ntp_tiles::IsCustomLinksEnabled())
    return;
  SearchBox* search_box = GetSearchBoxForCurrentContext();
  if (!search_box || !HasOrigin(GURL(chrome::kChromeSearchMostVisitedUrl)))
    return;

  // Limit the title to |kMaxCustomLinkTitleLength| characters. If truncated,
  // adds an ellipsis.
  base::string16 truncated_title =
      gfx::TruncateString(base::UTF8ToUTF16(title), kMaxCustomLinkTitleLength,
                          gfx::CHARACTER_BREAK);

  const GURL gurl(url);
  // If rid is -1, adds a new link. Otherwise, updates the existing link
  // indicated by the rid (empty fields will passed as empty strings). This will
  // initialize custom links if they have not already been initialized.
  if (rid == -1) {
    if (!gurl.is_valid() || truncated_title.empty())
      return;
    search_box->AddCustomLink(gurl, base::UTF16ToUTF8(truncated_title));
    search_box->LogEvent(NTPLoggingEventType::NTP_CUSTOMIZE_SHORTCUT_ADD);
  } else {
    // Check that the URL, if provided, is valid.
    if (!url.empty() && !gurl.is_valid())
      return;
    search_box->UpdateCustomLink(rid, gurl, base::UTF16ToUTF8(truncated_title));
    search_box->LogEvent(NTPLoggingEventType::NTP_CUSTOMIZE_SHORTCUT_UPDATE);
  }
}

// static
void NewTabPageBindings::UndoCustomLinkAction() {
  if (!ntp_tiles::IsCustomLinksEnabled())
    return;
  SearchBox* search_box = GetSearchBoxForCurrentContext();
  if (!search_box)
    return;
  search_box->UndoCustomLinkAction();
  search_box->LogEvent(NTPLoggingEventType::NTP_CUSTOMIZE_SHORTCUT_UNDO);
}

// static
void NewTabPageBindings::ResetCustomLinks() {
  if (!ntp_tiles::IsCustomLinksEnabled())
    return;
  SearchBox* search_box = GetSearchBoxForCurrentContext();
  if (!search_box)
    return;
  search_box->ResetCustomLinks();
  search_box->LogEvent(NTPLoggingEventType::NTP_CUSTOMIZE_SHORTCUT_RESTORE_ALL);
}

// static
std::string NewTabPageBindings::FixupAndValidateUrl(const std::string& url) {
  SearchBox* search_box = GetSearchBoxForCurrentContext();
  if (!search_box || !HasOrigin(GURL(chrome::kChromeSearchMostVisitedUrl)))
    return std::string();
  return search_box->FixupAndValidateUrl(url);
}

// static
void NewTabPageBindings::LogEvent(int event) {
  SearchBox* search_box = GetSearchBoxForCurrentContext();
  if (!search_box) {
    return;
  }
  if (event <= NTP_EVENT_TYPE_LAST)
    search_box->LogEvent(static_cast<NTPLoggingEventType>(event));
}

// static
void NewTabPageBindings::LogMostVisitedImpression(
    int position,
    int tile_title_source,
    int tile_source,
    int tile_type,
    v8::Local<v8::Value> data_generation_time) {
  SearchBox* search_box = GetSearchBoxForCurrentContext();
  if (!search_box || !HasOrigin(GURL(chrome::kChromeSearchMostVisitedUrl)))
    return;

  if (tile_title_source <= static_cast<int>(ntp_tiles::TileTitleSource::LAST) &&
      tile_source <= static_cast<int>(ntp_tiles::TileSource::LAST) &&
      tile_type <= ntp_tiles::TileVisualType::TILE_TYPE_MAX) {
    const ntp_tiles::NTPTileImpression impression(
        position, static_cast<ntp_tiles::TileSource>(tile_source),
        static_cast<ntp_tiles::TileTitleSource>(tile_title_source),
        static_cast<ntp_tiles::TileVisualType>(tile_type),
        favicon_base::IconType::kInvalid,
        ConvertDateValueToTime(*data_generation_time),
        /*url_for_rappor=*/GURL());
    search_box->LogMostVisitedImpression(impression);
  }
}

// static
void NewTabPageBindings::LogMostVisitedNavigation(
    int position,
    int tile_title_source,
    int tile_source,
    int tile_type,
    v8::Local<v8::Value> data_generation_time) {
  SearchBox* search_box = GetSearchBoxForCurrentContext();
  if (!search_box || !HasOrigin(GURL(chrome::kChromeSearchMostVisitedUrl)))
    return;

  if (tile_title_source <= static_cast<int>(ntp_tiles::TileTitleSource::LAST) &&
      tile_source <= static_cast<int>(ntp_tiles::TileSource::LAST) &&
      tile_type <= ntp_tiles::TileVisualType::TILE_TYPE_MAX) {
    const ntp_tiles::NTPTileImpression impression(
        position, static_cast<ntp_tiles::TileSource>(tile_source),
        static_cast<ntp_tiles::TileTitleSource>(tile_title_source),
        static_cast<ntp_tiles::TileVisualType>(tile_type),
        favicon_base::IconType::kInvalid,
        ConvertDateValueToTime(*data_generation_time),
        /*url_for_rappor=*/GURL());
    search_box->LogMostVisitedNavigation(impression);
  }
}

// static
void NewTabPageBindings::SetCustomBackgroundURL(
    const std::string& background_url) {
  SearchBox* search_box = GetSearchBoxForCurrentContext();
  GURL url(background_url);
  search_box->SetCustomBackgroundURL(url);
}

// static
void NewTabPageBindings::SetCustomBackgroundURLWithAttributions(
    const std::string& background_url,
    const std::string& attribution_line_1,
    const std::string& attribution_line_2,
    const std::string& attribution_action_url) {
  SearchBox* search_box = GetSearchBoxForCurrentContext();
  search_box->SetCustomBackgroundURLWithAttributions(
      GURL(background_url), attribution_line_1, attribution_line_2,
      GURL(attribution_action_url));
  // Captures saving the background by double-clicking, or clicking 'Done'.
  search_box->LogEvent(
      NTPLoggingEventType::NTP_CUSTOMIZE_CHROME_BACKGROUND_DONE);
}

// static
void NewTabPageBindings::SelectLocalBackgroundImage() {
  SearchBox* search_box = GetSearchBoxForCurrentContext();
  search_box->SelectLocalBackgroundImage();
}

}  // namespace

// static
void SearchBoxExtension::Install(blink::WebLocalFrame* frame) {
  v8::Isolate* isolate = blink::MainThreadIsolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = frame->MainWorldScriptContext();
  if (context.IsEmpty())
    return;

  v8::Context::Scope context_scope(context);

  gin::Handle<SearchBoxBindings> searchbox_controller =
      gin::CreateHandle(isolate, new SearchBoxBindings());
  if (searchbox_controller.IsEmpty())
    return;

  gin::Handle<NewTabPageBindings> newtabpage_controller =
      gin::CreateHandle(isolate, new NewTabPageBindings());
  if (newtabpage_controller.IsEmpty())
    return;

  v8::Handle<v8::Object> chrome =
      content::GetOrCreateChromeObject(isolate, context->Global());
  v8::Local<v8::Object> embedded_search = v8::Object::New(isolate);
  embedded_search
      ->Set(context, gin::StringToV8(isolate, "searchBox"),
            searchbox_controller.ToV8())
      .ToChecked();
  embedded_search
      ->Set(context, gin::StringToV8(isolate, "newTabPage"),
            newtabpage_controller.ToV8())
      .ToChecked();
  chrome
      ->Set(context, gin::StringToSymbol(isolate, "embeddedSearch"),
            embedded_search)
      .ToChecked();
}

// static
void SearchBoxExtension::DispatchChromeIdentityCheckResult(
    blink::WebLocalFrame* frame,
    const base::string16& identity,
    bool identity_match) {
  std::string escaped_identity = base::GetQuotedJSONString(identity);
  blink::WebString script(blink::WebString::FromUTF8(base::StringPrintf(
      kDispatchChromeIdentityCheckResult, escaped_identity.c_str(),
      identity_match ? "true" : "false")));
  Dispatch(frame, script);
}

// static
void SearchBoxExtension::DispatchFocusChange(blink::WebLocalFrame* frame) {
  Dispatch(frame, kDispatchFocusChangedScript);
}

// static
void SearchBoxExtension::DispatchHistorySyncCheckResult(
    blink::WebLocalFrame* frame,
    bool sync_history) {
  blink::WebString script(blink::WebString::FromUTF8(base::StringPrintf(
      kDispatchHistorySyncCheckResult, sync_history ? "true" : "false")));
  Dispatch(frame, script);
}

// static
void SearchBoxExtension::DispatchAddCustomLinkResult(
    blink::WebLocalFrame* frame,
    bool success) {
  blink::WebString script(blink::WebString::FromUTF8(base::StringPrintf(
      kDispatchAddCustomLinkResult, success ? "true" : "false")));
  Dispatch(frame, script);
}

// static
void SearchBoxExtension::DispatchUpdateCustomLinkResult(
    blink::WebLocalFrame* frame,
    bool success) {
  blink::WebString script(blink::WebString::FromUTF8(base::StringPrintf(
      kDispatchUpdateCustomLinkResult, success ? "true" : "false")));
  Dispatch(frame, script);
}

// static
void SearchBoxExtension::DispatchDeleteCustomLinkResult(
    blink::WebLocalFrame* frame,
    bool success) {
  blink::WebString script(blink::WebString::FromUTF8(base::StringPrintf(
      kDispatchDeleteCustomLinkResult, success ? "true" : "false")));
  Dispatch(frame, script);
}

// static
void SearchBoxExtension::DispatchDoesUrlResolveResult(
    blink::WebLocalFrame* frame,
    bool resolves) {
  blink::WebString script(blink::WebString::FromUTF8(base::StringPrintf(
      kDispatchDoesUrlResolveResultScript, resolves ? "true" : "false")));
  Dispatch(frame, script);
}

// static
void SearchBoxExtension::DispatchInputCancel(blink::WebLocalFrame* frame) {
  Dispatch(frame, kDispatchInputCancelScript);
}

// static
void SearchBoxExtension::DispatchInputStart(blink::WebLocalFrame* frame) {
  Dispatch(frame, kDispatchInputStartScript);
}

// static
void SearchBoxExtension::DispatchKeyCaptureChange(blink::WebLocalFrame* frame) {
  Dispatch(frame, kDispatchKeyCaptureChangeScript);
}

// static
void SearchBoxExtension::DispatchMostVisitedChanged(
    blink::WebLocalFrame* frame) {
  Dispatch(frame, kDispatchMostVisitedChangedScript);
}

// static
void SearchBoxExtension::DispatchThemeChange(blink::WebLocalFrame* frame) {
  Dispatch(frame, kDispatchThemeChangeEventScript);
}
