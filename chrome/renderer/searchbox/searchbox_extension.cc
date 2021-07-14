// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/searchbox/searchbox_extension.h"

#include <stddef.h>
#include <stdint.h>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/i18n/rtl.h"
#include "base/json/json_writer.h"
#include "base/json/string_escape.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/common/search/generated_colors_info.h"
#include "chrome/common/search/instant_types.h"
#include "chrome/common/search/ntp_logging_events.h"
#include "chrome/common/search/selected_colors_info.h"
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
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/public/web/blink.h"
#include "third_party/blink/public/web/web_document.h"
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

const char kCSSBackgroundImageFormat[] = "-webkit-image-set("
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

const char kThemeAttributionFormat[] = "-webkit-image-set("
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
    int render_frame_id,
    InstantRestrictedID restricted_id) {
  return gin::DataObjectBuilder(isolate)
      .Set("rid", restricted_id)
      .Set("faviconUrl",
           base::StringPrintf("chrome-search://favicon/size/16@%fx/%d/%d",
                              device_pixel_ratio, render_frame_id,
                              restricted_id))
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
  builder.Set("renderViewId", render_view_id)
      .Set("rid", restricted_id)
      .Set("tileTitleSource", static_cast<int>(mv_item.title_source))
      .Set("tileSource", static_cast<int>(mv_item.source))
      .Set("title", title)
      .Set("domain", mv_item.url.host())
      .Set("direction", base::StringPiece(direction))
      .Set("url", mv_item.url.spec());

  // If the suggestion already has a favicon, we populate the element with it.
  if (!mv_item.favicon.spec().empty())
    builder.Set("faviconUrl", mv_item.favicon.spec());

  return builder.Build();
}

absl::optional<int> CoerceToInt(v8::Isolate* isolate, v8::Value* value) {
  DCHECK(value);
  v8::MaybeLocal<v8::Int32> maybe_int =
      value->ToInt32(isolate->GetCurrentContext());
  if (maybe_int.IsEmpty())
    return absl::nullopt;
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

// Converts given array to SkColor and returns whether the conversion is
// successful.
bool ArrayToSkColor(v8::Isolate* isolate,
                    v8::Local<v8::Array> color,
                    SkColor* color_result) {
  if (color->Length() != 4)
    return false;

  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::Local<v8::Value> r_value;
  v8::Local<v8::Value> g_value;
  v8::Local<v8::Value> b_value;
  v8::Local<v8::Value> a_value;

  if (!color->Get(context, 0).ToLocal(&r_value) ||
      !color->Get(context, 1).ToLocal(&g_value) ||
      !color->Get(context, 2).ToLocal(&b_value) ||
      !color->Get(context, 3).ToLocal(&a_value))
    return false;

  absl::optional<int> r = CoerceToInt(isolate, *r_value);
  absl::optional<int> g = CoerceToInt(isolate, *g_value);
  absl::optional<int> b = CoerceToInt(isolate, *b_value);
  absl::optional<int> a = CoerceToInt(isolate, *a_value);

  if (!r.has_value() || !g.has_value() || !b.has_value() || !a.has_value())
    return false;

  if (*a > 255 || *r > 255 || *g > 255 || *b > 255)
    return false;

  *color_result = SkColorSetARGB(*a, *r, *g, *b);
  return true;
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

  builder.Set("themeId", theme.theme_id);
  builder.Set("themeName", theme.theme_name);

  builder.Set("customBackgroundDisabledByPolicy",
              theme.custom_background_disabled_by_policy);
  builder.Set("customBackgroundConfigured",
              !theme.custom_background_url.is_empty());

  // If a custom background has been set provide the relevant information to the
  // page.
  if (!theme.custom_background_url.is_empty()) {
    builder.Set("imageUrl", theme.custom_background_url.spec());
    builder.Set("attributionActionUrl",
                theme.custom_background_attribution_action_url.spec());
    builder.Set("attribution1", theme.custom_background_attribution_line_1);
    builder.Set("attribution2", theme.custom_background_attribution_line_2);
    builder.Set("collectionId", theme.collection_id);
    // Clear the theme attribution url, as it shouldn't be shown when
    // a custom background is set.
    builder.Set("attributionUrl", std::string());
  }

  // Set fields for themeing NTP elements.
  builder.Set("isNtpBackgroundDark", !color_utils::IsDark(theme.text_color));
  builder.Set("useTitleContainer", theme.has_theme_image);

  // TODO(gayane): Rename icon color to shortcut color in JS for consitancy.
  builder.Set("iconBackgroundColor",
              SkColorToArray(isolate, theme.shortcut_color));
  builder.Set("useWhiteAddIcon", color_utils::IsDark(theme.shortcut_color));

  builder.Set("logoColor", SkColorToArray(isolate, theme.logo_color));

  builder.Set("colorId", theme.color_id);
  if (theme.color_id != -1) {
    builder.Set("colorDark", SkColorToArray(isolate, theme.color_dark));
    builder.Set("colorLight", SkColorToArray(isolate, theme.color_light));
    builder.Set("colorPicked", SkColorToArray(isolate, theme.color_picked));
  }

  gin::DataObjectBuilder search_box(isolate);
  search_box.Set("bg", SkColorToArray(isolate, theme.search_box.bg));
  search_box.Set("icon", SkColorToArray(isolate, theme.search_box.icon));
  search_box.Set("iconSelected",
                 SkColorToArray(isolate, theme.search_box.icon_selected));
  search_box.Set("placeholder",
                 SkColorToArray(isolate, theme.search_box.placeholder));
  search_box.Set("resultsBg",
                 SkColorToArray(isolate, theme.search_box.results_bg));
  search_box.Set("resultsBgHovered",
                 SkColorToArray(isolate, theme.search_box.results_bg_hovered));
  search_box.Set("resultsBgSelected",
                 SkColorToArray(isolate, theme.search_box.results_bg_selected));
  search_box.Set("resultsDim",
                 SkColorToArray(isolate, theme.search_box.results_dim));
  search_box.Set(
      "resultsDimSelected",
      SkColorToArray(isolate, theme.search_box.results_dim_selected));
  search_box.Set("resultsText",
                 SkColorToArray(isolate, theme.search_box.results_text));
  search_box.Set(
      "resultsTextSelected",
      SkColorToArray(isolate, theme.search_box.results_text_selected));
  search_box.Set("resultsUrl",
                 SkColorToArray(isolate, theme.search_box.results_url));
  search_box.Set(
      "resultsUrlSelected",
      SkColorToArray(isolate, theme.search_box.results_url_selected));
  search_box.Set("text", SkColorToArray(isolate, theme.search_box.text));
  builder.Set("searchBox", search_box.Build());

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

static const char kDispatchLocalBackgroundSelectedScript[] =
    "if (window.chrome &&"
    "    window.chrome.embeddedSearch &&"
    "    window.chrome.embeddedSearch.newTabPage &&"
    "    window.chrome.embeddedSearch.newTabPage.onlocalbackgroundselected &&"
    "    typeof "
    "window.chrome.embeddedSearch.newTabPage.onlocalbackgroundselected =="
    "        'function') {"
    "  "
    "window.chrome.embeddedSearch.newTabPage."
    "onlocalbackgroundselected();"
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
  static bool IsFocused();
  static bool IsKeyCaptureEnabled();

  // Handlers for JS functions.
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
  static void LogEvent(int event);
  static void LogSuggestionEventWithValue(int event, int data);
  static void LogMostVisitedImpression(int position,
                                       int tile_title_source,
                                       int tile_source,
                                       int tile_type);
  static void LogMostVisitedNavigation(int position,
                                       int tile_title_source,
                                       int tile_source,
                                       int tile_type);
  static void ResetCustomBackgroundInfo();
  static void SetCustomBackgroundInfo(const std::string& background_url,
                                      const std::string& attribution_line_1,
                                      const std::string& attribution_line_2,
                                      const std::string& attributionActionUrl,
                                      const std::string& collection_id);
  static void SelectLocalBackgroundImage();
  static void BlocklistSearchSuggestion(int task_version, int task_id);
  static void BlocklistSearchSuggestionWithHash(int task_version,
                                                int task_id,
                                                const std::string& hash);
  static void SearchSuggestionSelected(int task_version,
                                       int task_id,
                                       const std::string& hash);
  static void OptOutOfSearchSuggestions();
  static void UseDefaultTheme();
  static void ApplyDefaultTheme();
  static void ApplyAutogeneratedTheme(v8::Isolate* isolate,
                                      int id,
                                      v8::Local<v8::Value> color);
  static void RevertThemeChanges();
  static void ConfirmThemeChanges();
  static void BlocklistPromo(const std::string& promo_id);
  static v8::Local<v8::Value> GetColorsInfo(v8::Isolate* isolate);

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
      .SetProperty("ntpTheme", &NewTabPageBindings::GetNtpTheme)
      // TODO(https://crbug.com/1020450): remove "themeBackgroundInfo" legacy
      // name when we're sure no third-party NTP needs it.
      .SetProperty("themeBackgroundInfo", &NewTabPageBindings::GetNtpTheme)
      .SetMethod("deleteMostVisitedItem",
                 &NewTabPageBindings::DeleteMostVisitedItem)
      .SetMethod("undoAllMostVisitedDeletions",
                 &NewTabPageBindings::UndoAllMostVisitedDeletions)
      .SetMethod("undoMostVisitedDeletion",
                 &NewTabPageBindings::UndoMostVisitedDeletion)
      .SetMethod("getMostVisitedItemData",
                 &NewTabPageBindings::GetMostVisitedItemData)
      .SetMethod("logEvent", &NewTabPageBindings::LogEvent)
      .SetMethod("logSuggestionEventWithValue",
                 &NewTabPageBindings::LogSuggestionEventWithValue)
      .SetMethod("logMostVisitedImpression",
                 &NewTabPageBindings::LogMostVisitedImpression)
      .SetMethod("logMostVisitedNavigation",
                 &NewTabPageBindings::LogMostVisitedNavigation)
      .SetMethod("resetBackgroundInfo",
                 &NewTabPageBindings::ResetCustomBackgroundInfo)
      .SetMethod("setBackgroundInfo",
                 &NewTabPageBindings::SetCustomBackgroundInfo)
      .SetMethod("selectLocalBackgroundImage",
                 &NewTabPageBindings::SelectLocalBackgroundImage)
      // These methods have been renamed to match BlocklistSearchSuggestion*
      // below, but are kept until JavaScript calls can be migrated.
      // TODO: Remove the following two additions per guidance in b/179534247
      .SetMethod("blacklistSearchSuggestion",
                 &NewTabPageBindings::BlocklistSearchSuggestion)
      .SetMethod("blacklistSearchSuggestionWithHash",
                 &NewTabPageBindings::BlocklistSearchSuggestionWithHash)
      .SetMethod("blocklistSearchSuggestion",
                 &NewTabPageBindings::BlocklistSearchSuggestion)
      .SetMethod("blocklistSearchSuggestionWithHash",
                 &NewTabPageBindings::BlocklistSearchSuggestionWithHash)
      .SetMethod("searchSuggestionSelected",
                 &NewTabPageBindings::SearchSuggestionSelected)
      .SetMethod("optOutOfSearchSuggestions",
                 &NewTabPageBindings::OptOutOfSearchSuggestions)
      .SetMethod("useDefaultTheme", &NewTabPageBindings::UseDefaultTheme)
      .SetMethod("applyDefaultTheme", &NewTabPageBindings::ApplyDefaultTheme)
      .SetMethod("applyAutogeneratedTheme",
                 &NewTabPageBindings::ApplyAutogeneratedTheme)
      .SetMethod("revertThemeChanges", &NewTabPageBindings::RevertThemeChanges)
      .SetMethod("confirmThemeChanges",
                 &NewTabPageBindings::ConfirmThemeChanges)
      .SetMethod("getColorsInfo", &NewTabPageBindings::GetColorsInfo)
      .SetMethod("blocklistPromo", &NewTabPageBindings::BlocklistPromo);
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

  content::RenderFrame* render_frame = GetMainRenderFrameForCurrentContext();

  // This corresponds to "window.devicePixelRatio" in JavaScript.
  float zoom_factor =
      blink::PageZoomLevelToZoomFactor(render_frame->GetWebView()->ZoomLevel());
  float device_pixel_ratio = render_frame->GetDeviceScaleFactor() * zoom_factor;

  int render_frame_id = render_frame->GetRoutingID();

  std::vector<InstantMostVisitedItemIDPair> instant_mv_items;
  search_box->GetMostVisitedItems(&instant_mv_items);
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::Local<v8::Object> v8_mv_items =
      v8::Array::New(isolate, instant_mv_items.size());
  for (size_t i = 0; i < instant_mv_items.size(); ++i) {
    InstantRestrictedID rid = instant_mv_items[i].first;
    v8_mv_items
        ->CreateDataProperty(
            context, i,
            GenerateMostVisitedItem(isolate, device_pixel_ratio,
                                    render_frame_id, rid))
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
  absl::optional<int> rid = CoerceToInt(isolate, *rid_value);
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
  absl::optional<int> rid = CoerceToInt(isolate, *rid_value);
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

  int render_frame_id = GetMainRenderFrameForCurrentContext()->GetRoutingID();
  return GenerateMostVisitedItemData(isolate, render_frame_id, rid, item);
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
void NewTabPageBindings::LogSuggestionEventWithValue(int event, int data) {
  SearchBox* search_box = GetSearchBoxForCurrentContext();
  if (!search_box) {
    return;
  }
  if (event <= static_cast<int>(NTPSuggestionsLoggingEventType::kMaxValue)) {
    search_box->LogSuggestionEventWithValue(
        static_cast<NTPSuggestionsLoggingEventType>(event), data);
  }
}

// static
void NewTabPageBindings::LogMostVisitedImpression(int position,
                                                  int tile_title_source,
                                                  int tile_source,
                                                  int tile_type) {
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
        /*url_for_rappor=*/GURL());
    search_box->LogMostVisitedImpression(impression);
  }
}

// static
void NewTabPageBindings::LogMostVisitedNavigation(int position,
                                                  int tile_title_source,
                                                  int tile_source,
                                                  int tile_type) {
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
        /*url_for_rappor=*/GURL());
    search_box->LogMostVisitedNavigation(impression);
  }
}

// static
void NewTabPageBindings::ResetCustomBackgroundInfo() {
  SetCustomBackgroundInfo(std::string(), std::string(), std::string(),
                          std::string(), std::string());
}

// static
void NewTabPageBindings::SetCustomBackgroundInfo(
    const std::string& background_url,
    const std::string& attribution_line_1,
    const std::string& attribution_line_2,
    const std::string& attribution_action_url,
    const std::string& collection_id) {
  SearchBox* search_box = GetSearchBoxForCurrentContext();
  search_box->SetCustomBackgroundInfo(
      GURL(background_url), attribution_line_1, attribution_line_2,
      GURL(attribution_action_url), collection_id);
  // Captures different events that occur when a background selection is made
  // and 'Done' is clicked on the dialog.
  if (!collection_id.empty()) {
    search_box->LogEvent(
        NTPLoggingEventType::NTP_BACKGROUND_DAILY_REFRESH_ENABLED);
  } else if (background_url.empty()) {
    search_box->LogEvent(
        NTPLoggingEventType::NTP_CUSTOMIZE_RESTORE_BACKGROUND_CLICKED);
    search_box->LogEvent(NTPLoggingEventType::NTP_BACKGROUND_IMAGE_RESET);
  } else {
    search_box->LogEvent(
        NTPLoggingEventType::NTP_CUSTOMIZE_CHROME_BACKGROUND_DONE);
    search_box->LogEvent(NTPLoggingEventType::NTP_BACKGROUND_IMAGE_SET);
  }
}

// static
void NewTabPageBindings::SelectLocalBackgroundImage() {
  SearchBox* search_box = GetSearchBoxForCurrentContext();
  search_box->SelectLocalBackgroundImage();
}

// static
void NewTabPageBindings::BlocklistSearchSuggestion(const int task_version,
                                                   const int task_id) {
  SearchBox* search_box = GetSearchBoxForCurrentContext();
  if (!search_box)
    return;
  search_box->BlocklistSearchSuggestion(task_version, task_id);
}

// static
void NewTabPageBindings::BlocklistSearchSuggestionWithHash(
    int task_version,
    int task_id,
    const std::string& hash) {
  if (hash.length() != 4) {
    return;
  }

  std::vector<uint8_t> data(hash.begin(), hash.end());
  SearchBox* search_box = GetSearchBoxForCurrentContext();
  if (!search_box)
    return;
  search_box->BlocklistSearchSuggestionWithHash(task_version, task_id, data);
}

// static
void NewTabPageBindings::SearchSuggestionSelected(int task_version,
                                                  int task_id,
                                                  const std::string& hash) {
  if (hash.length() > 4) {
    return;
  }

  std::vector<uint8_t> data(hash.begin(), hash.end());
  SearchBox* search_box = GetSearchBoxForCurrentContext();
  if (!search_box)
    return;
  search_box->SearchSuggestionSelected(task_version, task_id, data);
}

// static
void NewTabPageBindings::OptOutOfSearchSuggestions() {
  SearchBox* search_box = GetSearchBoxForCurrentContext();
  if (!search_box)
    return;
  search_box->OptOutOfSearchSuggestions();
}

// static
void NewTabPageBindings::ApplyDefaultTheme() {
  SearchBox* search_box = GetSearchBoxForCurrentContext();
  if (!search_box)
    return;
  search_box->ApplyDefaultTheme();
}

// static
void NewTabPageBindings::UseDefaultTheme() {
  SearchBox* search_box = GetSearchBoxForCurrentContext();
  if (!search_box)
    return;
  search_box->ApplyDefaultTheme();
  search_box->ConfirmThemeChanges();
}

// static
void NewTabPageBindings::ApplyAutogeneratedTheme(v8::Isolate* isolate,
                                                 int id,
                                                 v8::Local<v8::Value> value) {
  SearchBox* search_box = GetSearchBoxForCurrentContext();
  if (!search_box || !value->IsArray())
    return;
  SkColor color;
  if (!ArrayToSkColor(isolate, value.As<v8::Array>(), &color))
    return;
  search_box->ApplyAutogeneratedTheme(color);
}

// static
void NewTabPageBindings::RevertThemeChanges() {
  SearchBox* search_box = GetSearchBoxForCurrentContext();
  if (!search_box)
    return;
  search_box->RevertThemeChanges();
}

// static
void NewTabPageBindings::ConfirmThemeChanges() {
  SearchBox* search_box = GetSearchBoxForCurrentContext();
  if (!search_box)
    return;
  search_box->ConfirmThemeChanges();
}

v8::Local<v8::Value> NewTabPageBindings::GetColorsInfo(v8::Isolate* isolate) {
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::Local<v8::Object> v8_colors =
      v8::Array::New(isolate, chrome_colors::kNumColorsInfo);
  int i = 0;
  for (chrome_colors::ColorInfo color_info :
       chrome_colors::kGeneratedColorsInfo) {
    v8::Local<v8::Object> v8_color_info =
        gin::DataObjectBuilder(isolate)
            .Set("id", color_info.id)
            .Set("color", SkColorToArray(isolate, color_info.color))
            .Set("label", l10n_util::GetStringUTF16(color_info.label_id))
            .Set("icon", std::string(color_info.icon_data))
            .Build();
    v8_colors->CreateDataProperty(context, i++, v8_color_info).Check();
  }
  return v8_colors;
}

void NewTabPageBindings::BlocklistPromo(const std::string& promo_id) {
  SearchBox* search_box = GetSearchBoxForCurrentContext();
  if (!search_box)
    return;
  search_box->BlocklistPromo(promo_id);
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

// static
void SearchBoxExtension::DispatchLocalBackgroundSelected(
    blink::WebLocalFrame* frame) {
  Dispatch(frame, kDispatchLocalBackgroundSelectedScript);
}
