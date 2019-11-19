// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/extensions/api/tabs/tabs_api.h"

#include <stddef.h>
#include <algorithm>
#include <limits>
#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ref_counted_memory.h"
#include "base/metrics/histogram_macros.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/pattern.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chromecast/browser/cast_browser_process.h"
#include "chromecast/browser/cast_web_contents.h"
#include "chromecast/browser/extensions/api/tabs/tabs_constants.h"
#include "chromecast/service/cast_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "extensions/browser/extension_api_frame_id_map.h"
#include "extensions/browser/extension_zoom_request_client.h"
#include "extensions/common/api/extension_types.h"
#include "extensions/common/constants.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension.h"
#include "extensions/common/host_id.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/message_bundle.h"
#include "extensions/common/permissions/permissions_data.h"
#include "third_party/blink/public/common/page/page_zoom.h"
#include "ui/base/models/list_selection_model.h"
#include "ui/base/ui_base_types.h"

using content::BrowserThread;
using content::NavigationController;
using content::NavigationEntry;
using content::OpenURLParams;
using content::Referrer;
using content::WebContents;
using zoom::ZoomController;

using chromecast::CastWebContents;

namespace extensions {

namespace keys = tabs_constants;
using api::extension_types::InjectDetails;

namespace cast {
namespace api {

namespace {

// Cast only has one window, arbitrarily assign it to ID 0.
constexpr int kCastWindowId = 0;

class ExtensionTabUtil {
 public:
  enum PopulateTabBehavior {
    kPopulateTabs,
    kDontPopulateTabs,
  };
};

template <typename T>
class ApiParameterExtractor {
 public:
  explicit ApiParameterExtractor(T* params) : params_(params) {}
  ~ApiParameterExtractor() {}

  bool populate_tabs() {
    if (params_->get_info.get() && params_->get_info->populate.get())
      return *params_->get_info->populate;
    return false;
  }

 private:
  T* params_;
};

template <typename T>
void AssignOptionalValue(const std::unique_ptr<T>& source,
                         std::unique_ptr<T>& destination) {
  if (source.get()) {
    destination.reset(new T(*source));
  }
}

std::unique_ptr<api::tabs::MutedInfo> CreateMutedInfo(
    content::WebContents* contents) {
  DCHECK(contents);
  std::unique_ptr<api::tabs::MutedInfo> info(new api::tabs::MutedInfo);
  info->muted = contents->IsAudioMuted();
  return info;
}

std::unique_ptr<api::tabs::Tab> CreateTabObject(
    const CastWebContents* cast_web_contents,
    const Extension* extension,
    int tab_index) {
  WebContents* contents = cast_web_contents->web_contents();
  bool is_loading = contents->IsLoading();
  auto tab_object = std::make_unique<api::tabs::Tab>();
  tab_object->id = std::make_unique<int>(cast_web_contents->tab_id());
  tab_object->index = tab_index;
  tab_object->window_id = kCastWindowId;
  tab_object->status = std::make_unique<std::string>(
      is_loading ? keys::kStatusValueLoading : keys::kStatusValueComplete);
  tab_object->active = true;
  tab_object->selected = true;
  tab_object->highlighted = true;
  tab_object->pinned = true;
  tab_object->audible = std::make_unique<bool>(contents->IsCurrentlyAudible());
  tab_object->discarded = false;
  tab_object->auto_discardable = true;
  tab_object->muted_info = CreateMutedInfo(contents);
  tab_object->incognito = contents->GetBrowserContext()->IsOffTheRecord();
  gfx::Size contents_size = contents->GetContainerBounds().size();
  tab_object->width = std::make_unique<int>(contents_size.width());
  tab_object->height = std::make_unique<int>(contents_size.height());

  tab_object->url = std::make_unique<std::string>(contents->GetURL().spec());
  tab_object->title =
      std::make_unique<std::string>(base::UTF16ToUTF8(contents->GetTitle()));

  return tab_object;
}

std::unique_ptr<base::ListValue> CreateTabList(
    const std::vector<CastWebContents*>& webviews,
    const Extension* extension) {
  std::unique_ptr<base::ListValue> tab_list(new base::ListValue());
  for (size_t i = 0; i < webviews.size(); i++) {
    tab_list->Append(CreateTabObject(webviews[i], extension, i)->ToValue());
  }
  return tab_list;
}

const std::vector<CastWebContents*>& GetTabList() {
  return chromecast::CastWebContents::GetAll();
}

int GetActiveWebContentsIndex() {
  return 0;
}

const CastWebContents* GetWebViewForIndex(int index) {
  auto& tabs = GetTabList();
  if (index >= 0 && index < static_cast<int>(tabs.size()))
    return tabs[index];
  return nullptr;
}

const CastWebContents* GetWebViewForTab(int tab_id, int* tab_index = nullptr) {
  if (tab_id == -1) {
    // Return the active tab
    int index = GetActiveWebContentsIndex();
    if (tab_index)
      *tab_index = index;
    return GetWebViewForIndex(index);
  }

  auto& tabs = GetTabList();
  for (size_t i = 0; i < tabs.size(); i++) {
    if (tabs[i]->tab_id() == tab_id) {
      if (tab_index)
        *tab_index = static_cast<int>(i);
      return tabs[i];
    }
  }
  return nullptr;
}

std::unique_ptr<api::tabs::Tab> CreateTabObject(WebContents* contents,
                                                const Extension* extension) {
  auto& tabs = GetTabList();
  for (size_t i = 0; i < tabs.size(); i++) {
    if (tabs[i]->web_contents() == contents) {
      return CreateTabObject(tabs[i], extension, static_cast<int>(i));
    }
  }

  return nullptr;
}

int GetActiveWebContentsID() {
  const CastWebContents* contents =
      GetWebViewForIndex(GetActiveWebContentsIndex());
  return contents ? contents->tab_id() : -1;
}

int GetID(const std::unique_ptr<int>& id) {
  if (id.get())
    return *id.get();
  return -1;
}

std::unique_ptr<base::DictionaryValue> CreateWindowValueForExtension(
    content::BrowserContext* browser_context,
    const Extension* extension,
    ExtensionTabUtil::PopulateTabBehavior populate_tab_behavior) {
  auto result = std::make_unique<base::DictionaryValue>();

  result->SetInteger(keys::kIdKey, 0);
  result->SetString(keys::kWindowTypeKey, "normal");
  result->SetBoolean(keys::kFocusedKey, true);
  result->SetBoolean(keys::kIncognitoKey, browser_context->IsOffTheRecord());
  result->SetBoolean(keys::kAlwaysOnTopKey, true);
  result->SetString(keys::kShowStateKey, "locked-fullscreen");

  gfx::Rect bounds(0, 0, 640, 480);
  result->SetInteger(keys::kLeftKey, bounds.x());
  result->SetInteger(keys::kTopKey, bounds.y());
  result->SetInteger(keys::kWidthKey, bounds.width());
  result->SetInteger(keys::kHeightKey, bounds.height());

  if (populate_tab_behavior == ExtensionTabUtil::kPopulateTabs)
    result->Set(keys::kTabsKey, CreateTabList(GetTabList(), extension));

  return result;
}

}  // namespace

void ZoomModeToZoomSettings(ZoomController::ZoomMode zoom_mode,
                            api::tabs::ZoomSettings* zoom_settings) {
  DCHECK(zoom_settings);
  switch (zoom_mode) {
    case ZoomController::ZOOM_MODE_DEFAULT:
      zoom_settings->mode = api::tabs::ZOOM_SETTINGS_MODE_AUTOMATIC;
      zoom_settings->scope = api::tabs::ZOOM_SETTINGS_SCOPE_PER_ORIGIN;
      break;
    case ZoomController::ZOOM_MODE_ISOLATED:
      zoom_settings->mode = api::tabs::ZOOM_SETTINGS_MODE_AUTOMATIC;
      zoom_settings->scope = api::tabs::ZOOM_SETTINGS_SCOPE_PER_TAB;
      break;
    case ZoomController::ZOOM_MODE_MANUAL:
      zoom_settings->mode = api::tabs::ZOOM_SETTINGS_MODE_MANUAL;
      zoom_settings->scope = api::tabs::ZOOM_SETTINGS_SCOPE_PER_TAB;
      break;
    case ZoomController::ZOOM_MODE_DISABLED:
      zoom_settings->mode = api::tabs::ZOOM_SETTINGS_MODE_DISABLED;
      zoom_settings->scope = api::tabs::ZOOM_SETTINGS_SCOPE_PER_TAB;
      break;
  }
}

// Windows ---------------------------------------------------------------------
ExtensionFunction::ResponseAction WindowsGetFunction::Run() {
  std::unique_ptr<windows::Get::Params> params(
      windows::Get::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  if (params->window_id != kCastWindowId) {
    return RespondNow(Error("No window with that ID"));
  }

  ApiParameterExtractor<windows::Get::Params> extractor(params.get());

  ExtensionTabUtil::PopulateTabBehavior populate_tab_behavior =
      extractor.populate_tabs() ? ExtensionTabUtil::kPopulateTabs
                                : ExtensionTabUtil::kDontPopulateTabs;
  std::unique_ptr<base::DictionaryValue> windows =
      CreateWindowValueForExtension(browser_context(), extension(),
                                    populate_tab_behavior);
  return RespondNow(OneArgument(std::move(windows)));
}

ExtensionFunction::ResponseAction WindowsGetCurrentFunction::Run() {
  std::unique_ptr<windows::GetCurrent::Params> params(
      windows::GetCurrent::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  ApiParameterExtractor<windows::GetCurrent::Params> extractor(params.get());

  ExtensionTabUtil::PopulateTabBehavior populate_tab_behavior =
      extractor.populate_tabs() ? ExtensionTabUtil::kPopulateTabs
                                : ExtensionTabUtil::kDontPopulateTabs;
  std::unique_ptr<base::DictionaryValue> windows =
      CreateWindowValueForExtension(browser_context(), extension(),
                                    populate_tab_behavior);
  return RespondNow(OneArgument(std::move(windows)));
}

ExtensionFunction::ResponseAction WindowsGetLastFocusedFunction::Run() {
  std::unique_ptr<windows::GetCurrent::Params> params(
      windows::GetCurrent::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  ApiParameterExtractor<windows::GetCurrent::Params> extractor(params.get());

  ExtensionTabUtil::PopulateTabBehavior populate_tab_behavior =
      extractor.populate_tabs() ? ExtensionTabUtil::kPopulateTabs
                                : ExtensionTabUtil::kDontPopulateTabs;
  std::unique_ptr<base::DictionaryValue> windows =
      CreateWindowValueForExtension(browser_context(), extension(),
                                    populate_tab_behavior);
  return RespondNow(OneArgument(std::move(windows)));
}

ExtensionFunction::ResponseAction WindowsGetAllFunction::Run() {
  std::unique_ptr<windows::GetAll::Params> params(
      windows::GetAll::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  ApiParameterExtractor<windows::GetAll::Params> extractor(params.get());
  std::unique_ptr<base::ListValue> window_list(new base::ListValue());
  ExtensionTabUtil::PopulateTabBehavior populate_tab_behavior =
      extractor.populate_tabs() ? ExtensionTabUtil::kPopulateTabs
                                : ExtensionTabUtil::kDontPopulateTabs;
  window_list->Append(CreateWindowValueForExtension(
      browser_context(), extension(), populate_tab_behavior));

  return RespondNow(OneArgument(std::move(window_list)));
}

ExtensionFunction::ResponseAction WindowsCreateFunction::Run() {
  std::unique_ptr<windows::Create::Params> params(
      windows::Create::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);
  NOTIMPLEMENTED();
  return RespondNow(Error("Cannot create windows"));
}

ExtensionFunction::ResponseAction WindowsUpdateFunction::Run() {
  std::unique_ptr<windows::Update::Params> params(
      windows::Update::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  if (params->window_id != kCastWindowId) {
    return RespondNow(Error(ErrorUtils::FormatErrorMessage(
        keys::kWindowNotFoundError, base::NumberToString(params->window_id))));
  }

  return RespondNow(OneArgument(CreateWindowValueForExtension(
      browser_context(), extension(), ExtensionTabUtil::kDontPopulateTabs)));
}

ExtensionFunction::ResponseAction WindowsRemoveFunction::Run() {
  std::unique_ptr<windows::Remove::Params> params(
      windows::Remove::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);
  NOTIMPLEMENTED();
  return RespondNow(Error("Cannot remove windows"));
}

// Tabs ------------------------------------------------------------------------
ExtensionFunction::ResponseAction TabsGetSelectedFunction::Run() {
  // windowId defaults to "current" window.
  int window_id = kCastWindowId;

  std::unique_ptr<tabs::GetSelected::Params> params(
      tabs::GetSelected::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  if (params->window_id.get())
    window_id = *params->window_id;

  if (window_id != kCastWindowId) {
    return RespondNow(Error(ErrorUtils::FormatErrorMessage(
        keys::kWindowNotFoundError, base::NumberToString(window_id))));
  }

  int index = GetActiveWebContentsIndex();
  const CastWebContents* contents = GetWebViewForIndex(index);
  if (!contents)
    return RespondNow(Error(keys::kNoSelectedTabError));
  return RespondNow(ArgumentList(tabs::Get::Results::Create(
      *CreateTabObject(contents, extension(), index))));
}

ExtensionFunction::ResponseAction TabsGetAllInWindowFunction::Run() {
  std::unique_ptr<tabs::GetAllInWindow::Params> params(
      tabs::GetAllInWindow::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  // windowId defaults to "current" window.
  int window_id = kCastWindowId;
  if (params->window_id.get())
    window_id = *params->window_id;
  if (window_id != kCastWindowId)
    return RespondNow(Error(ErrorUtils::FormatErrorMessage(
        keys::kWindowNotFoundError, base::NumberToString(window_id))));

  return RespondNow(OneArgument(CreateTabList(GetTabList(), extension())));
}

ExtensionFunction::ResponseAction TabsQueryFunction::Run() {
  std::unique_ptr<tabs::Query::Params> params(
      tabs::Query::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  URLPatternSet url_patterns;
  if (params->query_info.url.get()) {
    std::vector<std::string> url_pattern_strings;
    if (params->query_info.url->as_string)
      url_pattern_strings.push_back(*params->query_info.url->as_string);
    else if (params->query_info.url->as_strings)
      url_pattern_strings.swap(*params->query_info.url->as_strings);
    // It is o.k. to use URLPattern::SCHEME_ALL here because this function does
    // not grant access to the content of the tabs, only to seeing their URLs
    // and meta data.
    std::string error;
    if (!url_patterns.Populate(url_pattern_strings, URLPattern::SCHEME_ALL,
                               true, &error)) {
      return RespondNow(Error(error));
    }
  }

  int window_id = kCastWindowId;
  if (params->query_info.window_id.get())
    window_id = *params->query_info.window_id;
  if (window_id != kCastWindowId) {
    return RespondNow(OneArgument(std::make_unique<base::ListValue>()));
  }

  std::string window_type;
  if (params->query_info.window_type != tabs::WINDOW_TYPE_NONE) {
    window_type = tabs::ToString(params->query_info.window_type);
    if (window_type != "normal")
      return RespondNow(OneArgument(std::make_unique<base::ListValue>()));
  }

  // For now, pretend that all tabs will match the query.
  // TODO(achaulk): make this actually execute the query.
  return RespondNow(OneArgument(CreateTabList(GetTabList(), extension())));
}

ExtensionFunction::ResponseAction TabsCreateFunction::Run() {
  std::unique_ptr<tabs::Create::Params> params(
      tabs::Create::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  NOTIMPLEMENTED();
  return RespondNow(Error("Cannot create tabs"));
}

ExtensionFunction::ResponseAction TabsDuplicateFunction::Run() {
  std::unique_ptr<tabs::Duplicate::Params> params(
      tabs::Duplicate::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  NOTIMPLEMENTED();
  return RespondNow(Error("Cannot duplicate tabs"));
}

ExtensionFunction::ResponseAction TabsGetFunction::Run() {
  std::unique_ptr<tabs::Get::Params> params(tabs::Get::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  int tab_id = params->tab_id;

  int tab_index;
  const CastWebContents* contents = GetWebViewForTab(tab_id, &tab_index);
  if (!contents) {
    return RespondNow(Error(ErrorUtils::FormatErrorMessage(
        keys::kTabNotFoundError, base::NumberToString(tab_id))));
  }

  return RespondNow(ArgumentList(tabs::Get::Results::Create(
      *CreateTabObject(contents, extension(), tab_index))));
}

ExtensionFunction::ResponseAction TabsGetCurrentFunction::Run() {
  DCHECK(dispatcher());

  // Return the caller, if it's a tab. If not the result isn't an error but an
  // empty tab (hence returning true).
  int index = GetActiveWebContentsIndex();
  const CastWebContents* active = GetWebViewForIndex(index);
  WebContents* caller_contents = GetSenderWebContents();
  std::unique_ptr<base::ListValue> results;
  if (caller_contents && caller_contents == active->web_contents()) {
    results = tabs::Get::Results::Create(
        *CreateTabObject(active, extension(), index));
  }
  return RespondNow(results ? ArgumentList(std::move(results)) : NoArguments());
}

ExtensionFunction::ResponseAction TabsHighlightFunction::Run() {
  std::unique_ptr<tabs::Highlight::Params> params(
      tabs::Highlight::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  // Get the window id from the params; default to current window if omitted.
  int window_id = kCastWindowId;
  if (params->highlight_info.window_id.get())
    window_id = *params->highlight_info.window_id;
  if (window_id != kCastWindowId) {
    return RespondNow(Error(ErrorUtils::FormatErrorMessage(
        keys::kWindowNotFoundError, base::NumberToString(window_id))));
  }

  int active_index = GetActiveWebContentsIndex();

  ui::ListSelectionModel selection;
  std::string error;

  const std::vector<CastWebContents*>& tabs = GetTabList();

  if (params->highlight_info.tabs.as_integers) {
    std::vector<int>& tab_indices = *params->highlight_info.tabs.as_integers;
    // Create a new selection model as we read the list of tab indices.
    for (size_t i = 0; i < tab_indices.size(); ++i) {
      if (!HighlightTab(tabs, &selection, &active_index, tab_indices[i],
                        &error)) {
        return RespondNow(Error(error));
      }
    }
  } else {
    EXTENSION_FUNCTION_VALIDATE(params->highlight_info.tabs.as_integer);
    if (!HighlightTab(tabs, &selection, &active_index,
                      *params->highlight_info.tabs.as_integer, &error)) {
      return RespondNow(Error(error));
    }
  }

  // Make sure they actually specified tabs to select.
  if (selection.empty())
    return RespondNow(Error(keys::kNoHighlightedTabError));

  selection.set_active(active_index);
  // TODO(achaulk): figure out what tab focus means for cast.
  NOTIMPLEMENTED() << "not changing tab focus";
  return RespondNow(OneArgument(CreateWindowValueForExtension(
      browser_context(), extension(), ExtensionTabUtil::kPopulateTabs)));
}

bool TabsHighlightFunction::HighlightTab(
    const std::vector<CastWebContents*>& tabs,
    ui::ListSelectionModel* selection,
    int* active_index,
    int index,
    std::string* error) {
  // Make sure the index is in range.
  if (index >= 0 && index < static_cast<int>(tabs.size())) {
    *error = ErrorUtils::FormatErrorMessage(keys::kTabIndexNotFoundError,
                                            base::NumberToString(index));
    return false;
  }

  // By default, we make the first tab in the list active.
  if (*active_index == -1)
    *active_index = index;

  selection->AddIndexToSelection(index);
  return true;
}

TabsUpdateFunction::TabsUpdateFunction() : web_contents_(NULL) {}

ExtensionFunction::ResponseAction TabsUpdateFunction::Run() {
  std::unique_ptr<tabs::Update::Params> params(
      tabs::Update::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  int tab_id = GetID(params->tab_id);
  const CastWebContents* contents = GetWebViewForTab(tab_id);
  if (!contents) {
    return RespondNow(Error(ErrorUtils::FormatErrorMessage(
        keys::kTabNotFoundError, base::NumberToString(tab_id))));
  }
  web_contents_ = contents->web_contents();

  // Navigate the tab to a new location if the url is different.
  bool is_async = false;
  if (params->update_properties.url.get()) {
    std::string updated_url = *params->update_properties.url;
    std::string error;
    if (!UpdateURL(updated_url, tab_id, &is_async, &error))
      return RespondNow(Error(error));
  }

  bool active = false;
  // TODO(rafaelw): Setting |active| from js doesn't make much sense.
  // Move tab selection management up to window.
  if (params->update_properties.selected.get())
    active = *params->update_properties.selected;

  // The 'active' property has replaced 'selected'.
  if (params->update_properties.active.get())
    active = *params->update_properties.active;

  if (active) {
    NOTIMPLEMENTED() << "active";
  }

  if (params->update_properties.highlighted.get()) {
    NOTIMPLEMENTED() << "highlighted";
  }

  if (params->update_properties.pinned.get()) {
    NOTIMPLEMENTED() << "pinned";
  }

  if (params->update_properties.muted.get()) {
    NOTIMPLEMENTED() << "muted";
  }

  if (params->update_properties.auto_discardable.get()) {
    NOTIMPLEMENTED() << "auto-discardable";
  }

  if (!is_async) {
    return RespondNow(ArgumentList(tabs::Get::Results::Create(
        *CreateTabObject(web_contents_, extension()))));
  }

  return RespondLater();
}

bool TabsUpdateFunction::UpdateURL(const std::string& url_string,
                                   int tab_id,
                                   bool* is_async,
                                   std::string* error) {
  GURL url = GURL(url_string);
  if (!url.is_valid())
    url = extension()->GetResourceURL(url_string);

  if (!url.is_valid()) {
    *error = ErrorUtils::FormatErrorMessage(keys::kInvalidUrlError, url_string);
    return false;
  }

  // JavaScript URLs can do the same kinds of things as cross-origin XHR, so
  // we need to check host permissions before allowing them.
  if (url.SchemeIs(url::kJavaScriptScheme)) {
    if (!extension()->permissions_data()->CanAccessPage(web_contents_->GetURL(),
                                                        tab_id, error)) {
      return false;
    }

    NOTIMPLEMENTED() << "javascript: URLs not implemented";
    return false;
  }

  bool use_renderer_initiated = false;

  NavigationController::LoadURLParams load_params(url);
  load_params.is_renderer_initiated = use_renderer_initiated;
  web_contents_->GetController().LoadURLWithParams(load_params);

  // The URL of a tab contents never actually changes to a JavaScript URL, so
  // this check only makes sense in other cases.
  if (!url.SchemeIs(url::kJavaScriptScheme)) {
    // The URL should be present in the pending entry, though it may not be
    // visible in the omnibox until it commits.
    DCHECK_EQ(
        url, web_contents_->GetController().GetPendingEntry()->GetVirtualURL());
  }

  return true;
}

void TabsUpdateFunction::OnExecuteCodeFinished(
    const std::string& error,
    const GURL& url,
    const base::ListValue& script_result) {
  if (!error.empty())
    Respond(Error(error));
  else
    Respond(ArgumentList(tabs::Get::Results::Create(
        *CreateTabObject(web_contents_, extension()))));
}

ExtensionFunction::ResponseAction TabsMoveFunction::Run() {
  std::unique_ptr<tabs::Move::Params> params(
      tabs::Move::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  return RespondNow(Error("Can't move tabs."));
}

ExtensionFunction::ResponseAction TabsReloadFunction::Run() {
  std::unique_ptr<tabs::Reload::Params> params(
      tabs::Reload::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  bool bypass_cache = false;
  if (params->reload_properties.get() &&
      params->reload_properties->bypass_cache.get()) {
    bypass_cache = *params->reload_properties->bypass_cache;
  }

  int tab_id = GetID(params->tab_id);
  const CastWebContents* contents = GetWebViewForTab(tab_id);
  if (!contents) {
    return RespondNow(Error(ErrorUtils::FormatErrorMessage(
        keys::kTabNotFoundError, base::NumberToString(tab_id))));
  }

  contents->web_contents()->GetController().Reload(
      bypass_cache ? content::ReloadType::BYPASSING_CACHE
                   : content::ReloadType::NORMAL,
      true);

  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction TabsRemoveFunction::Run() {
  std::unique_ptr<tabs::Remove::Params> params(
      tabs::Remove::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  return RespondNow(Error("Can't remove tabs."));
}

TabsCaptureVisibleTabFunction::TabsCaptureVisibleTabFunction() {}

ExtensionFunction::ResponseAction TabsCaptureVisibleTabFunction::Run() {
  return RespondNow(Error("Cannot capture tab"));
}

ExtensionFunction::ResponseAction TabsDetectLanguageFunction::Run() {
  std::unique_ptr<tabs::DetectLanguage::Params> params(
      tabs::DetectLanguage::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  return RespondNow(Error(keys::kNotImplementedError));
}

ExecuteCodeInTabFunction::ExecuteCodeInTabFunction() : execute_tab_id_(-1) {}

ExecuteCodeInTabFunction::~ExecuteCodeInTabFunction() {}

ExecuteCodeFunction::InitResult ExecuteCodeInTabFunction::Init() {
  if (init_result_)
    return init_result_.value();

  // |tab_id| is optional so it's ok if it's not there.
  int tab_id = -1;
  if (args_->GetInteger(0, &tab_id) && tab_id < 0)
    return set_init_result(VALIDATION_FAILURE);

  // |details| are not optional.
  base::DictionaryValue* details_value = NULL;
  if (!args_->GetDictionary(1, &details_value))
    return set_init_result(VALIDATION_FAILURE);
  std::unique_ptr<InjectDetails> details(new InjectDetails());
  if (!InjectDetails::Populate(*details_value, details.get()))
    return set_init_result(VALIDATION_FAILURE);

  // If the tab ID wasn't given then it needs to be converted to the
  // currently active tab's ID.
  if (tab_id == -1) {
    tab_id = GetActiveWebContentsID();
  }

  execute_tab_id_ = tab_id;
  details_ = std::move(details);
  set_host_id(HostID(HostID::EXTENSIONS, extension()->id()));
  return set_init_result(SUCCESS);
}

bool ExecuteCodeInTabFunction::CanExecuteScriptOnPage(std::string* error) {
  const CastWebContents* webview = GetWebViewForTab(execute_tab_id_);
  if (!webview) {
    *error = ErrorUtils::FormatErrorMessage(
        keys::kTabIndexNotFoundError, base::NumberToString(execute_tab_id_));
    return false;
  }

  content::WebContents* contents = webview->web_contents();

  int frame_id = details_->frame_id ? *details_->frame_id
                                    : ExtensionApiFrameIdMap::kTopFrameId;
  content::RenderFrameHost* rfh =
      ExtensionApiFrameIdMap::GetRenderFrameHostById(contents, frame_id);
  if (!rfh) {
    *error = ErrorUtils::FormatErrorMessage(
        keys::kFrameNotFoundError, base::NumberToString(frame_id),
        base::NumberToString(execute_tab_id_));
    return false;
  }

  // Content scripts declared in manifest.json can access frames at about:-URLs
  // if the extension has permission to access the frame's origin, so also allow
  // programmatic content scripts at about:-URLs for allowed origins.
  GURL effective_document_url(rfh->GetLastCommittedURL());
  bool is_about_url = effective_document_url.SchemeIs(url::kAboutScheme);
  if (is_about_url && details_->match_about_blank &&
      *details_->match_about_blank) {
    effective_document_url = GURL(rfh->GetLastCommittedOrigin().Serialize());
  }

  if (!effective_document_url.is_valid()) {
    // Unknown URL, e.g. because no load was committed yet. Allow for now, the
    // renderer will check again and fail the injection if needed.
    return true;
  }

  // NOTE: This can give the wrong answer due to race conditions, but it is OK,
  // we check again in the renderer.
  if (!extension()->permissions_data()->CanAccessPage(effective_document_url,
                                                      execute_tab_id_, error)) {
    if (is_about_url &&
        extension()->permissions_data()->active_permissions().HasAPIPermission(
            APIPermission::kTab)) {
      *error = ErrorUtils::FormatErrorMessage(
          manifest_errors::kCannotAccessAboutUrl,
          rfh->GetLastCommittedURL().spec(),
          rfh->GetLastCommittedOrigin().Serialize());
    }
    return false;
  }

  return true;
}

ScriptExecutor* ExecuteCodeInTabFunction::GetScriptExecutor(
    std::string* error) {
  const CastWebContents* contents = GetWebViewForTab(execute_tab_id_);

  if (!contents)
    return nullptr;

  // TODO(achaulk): create a ScriptExecutor if necessary.
  return nullptr;
}

bool ExecuteCodeInTabFunction::IsWebView() const {
  return false;
}

const GURL& ExecuteCodeInTabFunction::GetWebViewSrc() const {
  return GURL::EmptyGURL();
}

bool TabsExecuteScriptFunction::ShouldInsertCSS() const {
  return false;
}

bool TabsInsertCSSFunction::ShouldInsertCSS() const {
  return true;
}

ExtensionFunction::ResponseAction TabsSetZoomFunction::Run() {
  std::unique_ptr<tabs::SetZoom::Params> params(
      tabs::SetZoom::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);
  std::string error;

  int tab_id = GetID(params->tab_id);
  const CastWebContents* contents = GetWebViewForTab(tab_id);
  if (!contents) {
    error = ErrorUtils::FormatErrorMessage(keys::kTabNotFoundError,
                                           base::NumberToString(tab_id));
    return RespondNow(Error(error));
  }

  WebContents* web_contents = contents->web_contents();
  GURL url(web_contents->GetVisibleURL());
  if (extension()->permissions_data()->IsRestrictedUrl(url, &error))
    return RespondNow(Error(error));

  ZoomController* zoom_controller =
      ZoomController::FromWebContents(web_contents);
  double zoom_level =
      params->zoom_factor > 0
          ? blink::PageZoomFactorToZoomLevel(params->zoom_factor)
          : zoom_controller->GetDefaultZoomLevel();

  scoped_refptr<ExtensionZoomRequestClient> client(
      new ExtensionZoomRequestClient(extension()));
  if (!zoom_controller->SetZoomLevelByClient(zoom_level, client)) {
    // Tried to zoom a tab in disabled mode.
    return RespondNow(Error(keys::kCannotZoomDisabledTabError));
  }

  return RespondNow(ArgumentList(nullptr));
}

ExtensionFunction::ResponseAction TabsGetZoomFunction::Run() {
  std::unique_ptr<tabs::GetZoom::Params> params(
      tabs::GetZoom::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  int tab_id = GetID(params->tab_id);
  const CastWebContents* contents = GetWebViewForTab(tab_id);
  if (!contents) {
    return RespondNow(Error(ErrorUtils::FormatErrorMessage(
        keys::kTabNotFoundError, base::NumberToString(tab_id))));
  }

  WebContents* web_contents = contents->web_contents();
  double zoom_level =
      ZoomController::FromWebContents(web_contents)->GetZoomLevel();
  double zoom_factor = blink::PageZoomLevelToZoomFactor(zoom_level);
  return RespondNow(ArgumentList(tabs::GetZoom::Results::Create(zoom_factor)));
}

ExtensionFunction::ResponseAction TabsSetZoomSettingsFunction::Run() {
  using api::tabs::ZoomSettings;

  std::unique_ptr<tabs::SetZoomSettings::Params> params(
      tabs::SetZoomSettings::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  int tab_id = GetID(params->tab_id);
  const CastWebContents* contents = GetWebViewForTab(tab_id);
  if (!contents) {
    return RespondNow(Error(ErrorUtils::FormatErrorMessage(
        keys::kTabNotFoundError, base::NumberToString(tab_id))));
  }

  WebContents* web_contents = contents->web_contents();
  GURL url(web_contents->GetVisibleURL());
  std::string error;
  if (extension()->permissions_data()->IsRestrictedUrl(url, &error))
    return RespondNow(Error(error));

  // "per-origin" scope is only available in "automatic" mode.
  if (params->zoom_settings.scope == tabs::ZOOM_SETTINGS_SCOPE_PER_ORIGIN &&
      params->zoom_settings.mode != tabs::ZOOM_SETTINGS_MODE_AUTOMATIC &&
      params->zoom_settings.mode != tabs::ZOOM_SETTINGS_MODE_NONE) {
    return RespondNow(Error(keys::kPerOriginOnlyInAutomaticError));
  }

  // Determine the correct internal zoom mode to set |web_contents| to from the
  // user-specified |zoom_settings|.
  ZoomController::ZoomMode zoom_mode = ZoomController::ZOOM_MODE_DEFAULT;
  switch (params->zoom_settings.mode) {
    case tabs::ZOOM_SETTINGS_MODE_NONE:
    case tabs::ZOOM_SETTINGS_MODE_AUTOMATIC:
      switch (params->zoom_settings.scope) {
        case tabs::ZOOM_SETTINGS_SCOPE_NONE:
        case tabs::ZOOM_SETTINGS_SCOPE_PER_ORIGIN:
          zoom_mode = ZoomController::ZOOM_MODE_DEFAULT;
          break;
        case tabs::ZOOM_SETTINGS_SCOPE_PER_TAB:
          zoom_mode = ZoomController::ZOOM_MODE_ISOLATED;
      }
      break;
    case tabs::ZOOM_SETTINGS_MODE_MANUAL:
      zoom_mode = ZoomController::ZOOM_MODE_MANUAL;
      break;
    case tabs::ZOOM_SETTINGS_MODE_DISABLED:
      zoom_mode = ZoomController::ZOOM_MODE_DISABLED;
  }

  ZoomController::FromWebContents(web_contents)->SetZoomMode(zoom_mode);

  return RespondNow(ArgumentList(nullptr));
}

ExtensionFunction::ResponseAction TabsGetZoomSettingsFunction::Run() {
  std::unique_ptr<tabs::GetZoomSettings::Params> params(
      tabs::GetZoomSettings::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);
  std::string error;

  int tab_id = GetID(params->tab_id);
  const CastWebContents* contents = GetWebViewForTab(tab_id);
  if (!contents) {
    return RespondNow(Error(ErrorUtils::FormatErrorMessage(
        keys::kTabNotFoundError, base::NumberToString(tab_id))));
  }

  WebContents* web_contents = contents->web_contents();
  ZoomController* zoom_controller =
      ZoomController::FromWebContents(web_contents);

  ZoomController::ZoomMode zoom_mode = zoom_controller->zoom_mode();
  api::tabs::ZoomSettings zoom_settings;
  ZoomModeToZoomSettings(zoom_mode, &zoom_settings);
  zoom_settings.default_zoom_factor.reset(
      new double(blink::PageZoomLevelToZoomFactor(
          zoom_controller->GetDefaultZoomLevel())));

  return RespondNow(
      ArgumentList(api::tabs::GetZoomSettings::Results::Create(zoom_settings)));
}

ExtensionFunction::ResponseAction TabsDiscardFunction::Run() {
  std::unique_ptr<tabs::Discard::Params> params(
      tabs::Discard::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);
  NOTIMPLEMENTED();
  return RespondNow(Error("Cannot discard tabs"));
}

TabsDiscardFunction::TabsDiscardFunction() {}
TabsDiscardFunction::~TabsDiscardFunction() {}

}  // namespace api
}  // namespace cast
}  // namespace extensions
