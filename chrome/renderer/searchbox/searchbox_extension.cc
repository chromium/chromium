// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/searchbox/searchbox_extension.h"

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <string_view>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/i18n/rtl.h"
#include "base/json/json_writer.h"
#include "base/json/string_escape.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/common/search/instant_types.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/renderer_resources.h"
#include "chrome/renderer/searchbox/searchbox.h"
#include "components/crx_file/id_util.h"
#include "components/ntp_tiles/features.h"
#include "components/ntp_tiles/ntp_tile_impression.h"
#include "components/ntp_tiles/tile_source.h"
#include "components/ntp_tiles/tile_visual_type.h"
#include "content/public/common/url_constants.h"
#include "content/public/renderer/chrome_object_extensions_utils.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"
#include "gin/data_object_builder.h"
#include "gin/handle.h"
#include "gin/object_template_builder.h"
#include "gin/wrappable.h"
#include "third_party/blink/public/common/page/page_zoom.h"
#include "third_party/blink/public/platform/scheduler/web_agent_group_scheduler.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_frame_widget.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_script_source.h"
#include "third_party/blink/public/web/web_view.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/window_open_disposition.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/text_constants.h"
#include "ui/gfx/text_elider.h"
#include "url/gurl.h"
#include "url/url_constants.h"
#include "v8/include/v8.h"

namespace {

const char kCSSBackgroundImageFormat[] =
    "image-set("
    "url(chrome-search://theme/IDR_THEME_NTP_BACKGROUND?%s) 1x, "
    "url(chrome-search://theme/IDR_THEME_NTP_BACKGROUND@2x?%s) 2x)";

const char kCSSBackgroundPositionCenter[] = "center";
const char kCSSBackgroundPositionLeft[] = "left";
const char kCSSBackgroundPositionTop[] = "top";
const char kCSSBackgroundPositionRight[] = "right";
const char kCSSBackgroundPositionBottom[] = "bottom";

const char kCSSBackgroundRepeatNo[] = "no-repeat";
const char kCSSBackgroundRepeatX[] = "repeat-x";
const char kCSSBackgroundRepeatY[] = "repeat-y";
const char kCSSBackgroundRepeat[] = "repeat";

const char kThemeAttributionFormat[] =
    "image-set("
    "url(chrome-search://theme/IDR_THEME_NTP_ATTRIBUTION?%s) 1x, "
    "url(chrome-search://theme/IDR_THEME_NTP_ATTRIBUTION@2x?%s) 2x)";

const char kLTRHtmlTextDirection[] = "ltr";
const char kRTLHtmlTextDirection[] = "rtl";

void Dispatch(blink::WebLocalFrame* frame, const blink::WebString& script) {
  if (!frame)
    return;
  frame->ExecuteScript(blink::WebScriptSource(script));
}

// Populates a Javascript MostVisitedItem object for returning from
// newTabPage.mostVisited. This does not include private data such as "url" or
// "title".
v8::Local<v8::Object> GenerateMostVisitedItem(
    v8::Isolate* isolate,
    float device_pixel_ratio,
    const blink::LocalFrameToken& frame_token,
    InstantRestrictedID restricted_id) {
  return gin::DataObjectBuilder(isolate)
      .Set("rid", restricted_id)
      .Set("faviconUrl",
           base::StringPrintf("chrome-search://favicon/size/16@%fx/%s/%d",
                              device_pixel_ratio,
                              frame_token.ToString().c_str(), restricted_id))
      .Build();
}

// Populates a Javascript MostVisitedItem object appropriate for returning from
// newTabPage.getMostVisitedItemData.
// NOTE: Includes private data such as "url" and "title", so this should not be
// returned to the host page (via newTabPage.mostVisited). It is only accessible
// to most-visited iframes via getMostVisitedItemData.
v8::Local<v8::Object> GenerateMostVisitedItemData(
    v8::Isolate* isolate,
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
  if (base::i18n::GetFirstStrongCharacterDirection(mv_item.title) ==
      base::i18n::RIGHT_TO_LEFT) {
    direction = kRTLHtmlTextDirection;
  } else {
    direction = kLTRHtmlTextDirection;
  }

  std::string title = base::UTF16ToUTF8(mv_item.title);
  if (title.empty())
    title = mv_item.url.spec();

  gin::DataObjectBuilder builder(isolate);
  builder.Set("title", title)
      .Set("direction", std::string_view(direction))
      .Set("url", mv_item.url.spec());

  // If the suggestion already has a favicon, we populate the element with it.
  if (!mv_item.favicon.spec().empty())
    builder.Set("faviconUrl", mv_item.favicon.spec());

  return builder.Build();
}

std::optional<int> CoerceToInt(v8::Isolate* isolate, v8::Value* value) {
  DCHECK(value);
  v8::MaybeLocal<v8::Int32> maybe_int =
      value->ToInt32(isolate->GetCurrentContext());
  if (maybe_int.IsEmpty())
    return std::nullopt;
  return maybe_int.ToLocalChecked()->Value();
}

// Returns an array with the RGBA color components.
v8::Local<v8::Value> SkColorToArray(v8::Isolate* isolate,
                                    const SkColor& color) {
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::Local<v8::Array> color_array = v8::Array::New(isolate, 4);
  color_array
      ->CreateDataProperty(context, 0,
                           v8::Int32::New(isolate, SkColorGetR(color)))
      .Check();
  color_array
      ->CreateDataProperty(context, 1,
                           v8::Int32::New(isolate, SkColorGetG(color)))
      .Check();
  color_array
      ->CreateDataProperty(context, 2,
                           v8::Int32::New(isolate, SkColorGetB(color)))
      .Check();
  color_array
      ->CreateDataProperty(context, 3,
                           v8::Int32::New(isolate, SkColorGetA(color)))
      .Check();
  return color_array;
}

v8::Local<v8::Object> GenerateNtpTheme(v8::Isolate* isolate,
                                       const NtpTheme& theme) {
  gin::DataObjectBuilder builder(isolate);

  // True if the theme is the system default and no custom theme has been
  // applied.
  // Value is always valid.
  builder.Set("usingDefaultTheme", theme.using_default_theme);

  // Theme color for background as an array with the RGBA components in order.
  // Value is always valid.
  builder.Set("backgroundColorRgba",
              SkColorToArray(isolate, theme.background_color));

  // Theme color for text as an array with the RGBA components in order.
  // Value is always valid.
  builder.Set("textColorRgba", SkColorToArray(isolate, theme.text_color));

  // Theme color for light text as an array with the RGBA components in order.
  // Value is always valid.
  builder.Set("textColorLightRgba",
              SkColorToArray(isolate, theme.text_color_light));

  // The theme alternate logo value indicates same color when TRUE and a
  // colorful one when FALSE.
  builder.Set("alternateLogo", theme.logo_alternate);

  // The theme background image url is of format kCSSBackgroundImageFormat
  // where both instances of "%s" are replaced with the id that identifies the
  // theme.
  // This is the CSS "background-image" format.
  // Value is only valid if there's a custom theme background image.
  if (theme.has_theme_image) {
    builder.Set("imageUrl", base::StringPrintf(kCSSBackgroundImageFormat,
                                               theme.theme_id.c_str(),
                                               theme.theme_id.c_str()));

    // The theme background image horizontal alignment is one of "left",
    // "right", "center".
    // This is the horizontal component of the CSS "background-position" format.
    // Value is only valid if |imageUrl| is not empty.
    std::string alignment = kCSSBackgroundPositionCenter;
    if (theme.image_horizontal_alignment == THEME_BKGRND_IMAGE_ALIGN_LEFT) {
      alignment = kCSSBackgroundPositionLeft;
    } else if (theme.image_horizontal_alignment ==
               THEME_BKGRND_IMAGE_ALIGN_RIGHT) {
      alignment = kCSSBackgroundPositionRight;
    }
    builder.Set("imageHorizontalAlignment", alignment);

    // The theme background image vertical alignment is one of "top", "bottom",
    // "center".
    // This is the vertical component of the CSS "background-position" format.
    // Value is only valid if |image_url| is not empty.
    if (theme.image_vertical_alignment == THEME_BKGRND_IMAGE_ALIGN_TOP) {
      alignment = kCSSBackgroundPositionTop;
    } else if (theme.image_vertical_alignment ==
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
    switch (theme.image_tiling) {
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
    if (theme.has_attribution) {
      builder.Set("attributionUrl", base::StringPrintf(kThemeAttributionFormat,
                                                       theme.theme_id.c_str(),
                                                       theme.theme_id.c_str()));
    }
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

  SearchBoxBindings(const SearchBoxBindings&) = delete;
  SearchBoxBindings& operator=(const SearchBoxBindings&) = delete;

  ~SearchBoxBindings() override;

 private:
  // gin::Wrappable.
  gin::ObjectTemplateBuilder GetObjectTemplateBuilder(
      v8::Isolate* isolate) final;

  // Handlers for JS properties.
  static bool IsFocused();
  static bool IsKeyCaptureEnabled();

  // Handlers for JS functions.
  static void StartCapturingKeyStrokes();
  static void StopCapturingKeyStrokes();
};

gin::WrapperInfo SearchBoxBindings::kWrapperInfo = {gin::kEmbedderNativeGin};

SearchBoxBindings::SearchBoxBindings() = default;

SearchBoxBindings::~SearchBoxBindings() = default;

gin::ObjectTemplateBuilder SearchBoxBindings::GetObjectTemplateBuilder(
    v8::Isolate* isolate) {
  return gin::Wrappable<SearchBoxBindings>::GetObjectTemplateBuilder(isolate)
      .SetProperty("rtl", &base::i18n::IsRTL)
      .SetProperty("isFocused", &SearchBoxBindings::IsFocused)
      .SetProperty("isKeyCaptureEnabled",
                   &SearchBoxBindings::IsKeyCaptureEnabled)
      .SetMethod("startCapturingKeyStrokes",
                 &SearchBoxBindings::StartCapturingKeyStrokes)
      .SetMethod("stopCapturingKeyStrokes",
                 &SearchBoxBindings::StopCapturingKeyStrokes);
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

  NewTabPageBindings(const NewTabPageBindings&) = delete;
  NewTabPageBindings& operator=(const NewTabPageBindings&) = delete;

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
  static v8::Local<v8::Value> GetNtpTheme(v8::Isolate* isolate);

  // Handlers for JS functions visible to all NTPs.
  static void DeleteMostVisitedItem(v8::Isolate* isolate,
                                    v8::Local<v8::Value> rid);
  static void UndoAllMostVisitedDeletions();
  static void UndoMostVisitedDeletion(v8::Isolate* isolate,
                                      v8::Local<v8::Value> rid);

  // Handlers for JS functions visible only to the most visited iframe, the edit
  // custom links iframe, and/or the local NTP.
  static v8::Local<v8::Value> GetMostVisitedItemData(v8::Isolate* isolate,
                                                     int rid);
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
      .SetProperty("ntpTheme", &NewTabPageBindings::GetNtpTheme)
      // TODO(crbug.com/40656475): remove "themeBackgroundInfo" legacy
      // name when we're sure no third-party NTP needs it.
      .SetProperty("themeBackgroundInfo", &NewTabPageBindings::GetNtpTheme)
      .SetMethod("deleteMostVisitedItem",
                 &NewTabPageBindings::DeleteMostVisitedItem)
      .SetMethod("undoAllMostVisitedDeletions",
                 &NewTabPageBindings::UndoAllMostVisitedDeletions)
      .SetMethod("undoMostVisitedDeletion",
                 &NewTabPageBindings::UndoMostVisitedDeletion)
      .SetMethod("getMostVisitedItemData",
                 &NewTabPageBindings::GetMostVisitedItemData);
}

// static
bool NewTabPageBindings::HasOrigin(const GURL& origin) {
  blink::WebLocalFrame* frame = blink::WebLocalFrame::FrameForCurrentContext();
  if (!frame)
    return false;
  GURL url(frame->GetDocument().Url());
  return url.DeprecatedGetOriginAsURL() == origin.DeprecatedGetOriginAsURL();
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

  content::RenderFrame* render_frame = GetMainRenderFrameForCurrentContext();

  // This corresponds to "window.devicePixelRatio" in JavaScript.
  float zoom_factor = blink::ZoomLevelToZoomFactor(
      (render_frame->GetWebFrame())->FrameWidget()->GetZoomLevel());
  float device_pixel_ratio = render_frame->GetDeviceScaleFactor() * zoom_factor;

  auto frame_token = render_frame->GetWebFrame()->GetLocalFrameToken();

  std::vector<InstantMostVisitedItemIDPair> instant_mv_items;
  search_box->GetMostVisitedItems(&instant_mv_items);
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::Local<v8::Object> v8_mv_items =
      v8::Array::New(isolate, instant_mv_items.size());
  for (size_t i = 0; i < instant_mv_items.size(); ++i) {
    InstantRestrictedID rid = instant_mv_items[i].first;
    v8_mv_items
        ->CreateDataProperty(context, i,
                             GenerateMostVisitedItem(
                                 isolate, device_pixel_ratio, frame_token, rid))
        .Check();
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
v8::Local<v8::Value> NewTabPageBindings::GetNtpTheme(v8::Isolate* isolate) {
  const SearchBox* search_box = GetSearchBoxForCurrentContext();
  if (!search_box)
    return v8::Null(isolate);
  const NtpTheme* theme = search_box->GetNtpTheme();
  if (!theme)
    return v8::Null(isolate);
  return GenerateNtpTheme(isolate, *theme);
}

// static
void NewTabPageBindings::DeleteMostVisitedItem(v8::Isolate* isolate,
                                               v8::Local<v8::Value> rid_value) {
  // Manually convert to integer, so that the string "\"1\"" is also accepted.
  std::optional<int> rid = CoerceToInt(isolate, *rid_value);
  if (!rid.has_value())
    return;
  SearchBox* search_box = GetSearchBoxForCurrentContext();
  if (!search_box)
    return;

  search_box->DeleteMostVisitedItem(*rid);
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
  std::optional<int> rid = CoerceToInt(isolate, *rid_value);
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

  return GenerateMostVisitedItemData(isolate, rid, item);
}

}  // namespace

// static
void SearchBoxExtension::Install(blink::WebLocalFrame* frame) {
  v8::Isolate* isolate = frame->GetAgentGroupScheduler()->Isolate();
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

  v8::Local<v8::Object> chrome =
      content::GetOrCreateChromeObject(isolate, context);
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
void SearchBoxExtension::DispatchFocusChange(blink::WebLocalFrame* frame) {
  Dispatch(frame, kDispatchFocusChangedScript);
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
