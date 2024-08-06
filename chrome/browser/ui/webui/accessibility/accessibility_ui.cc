// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ui/webui/accessibility/accessibility_ui.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/containers/fixed_flat_map.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_writer.h"
#include "base/notreached.h"
#include "base/strings/escape.h"
#include "base/strings/pattern.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/accessibility_resources.h"
#include "chrome/grit/accessibility_resources_map.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/ax_inspect_factory.h"
#include "content/public/browser/browser_accessibility_state.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/favicon_status.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_iterator.h"
#include "content/public/browser/scoped_accessibility_mode.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/ax_updates_and_events.h"
#include "ui/accessibility/platform/ax_platform_node.h"
#include "ui/accessibility/platform/ax_platform_node_delegate.h"
#include "ui/accessibility/platform/inspect/ax_tree_formatter.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/views/accessibility/view_accessibility.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "ui/views/accessibility/widget_ax_tree_id_map.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#endif

static const char kTargetsDataFile[] = "targets-data.json";

static const char kAccessibilityModeField[] = "a11yMode";
static const char kBrowsersField[] = "browsers";
static const char kEnabledField[] = "enabled";
static const char kErrorField[] = "error";
static const char kEventLogsField[] = "eventLogs";
static const char kFaviconUrlField[] = "faviconUrl";
static const char kFlagNameField[] = "flagName";
static const char kStringNameField[] = "stringName";
static const char kModeIdField[] = "modeId";
static const char kNameField[] = "name";
static const char kPagesField[] = "pages";
static const char kPidField[] = "pid";
static const char kProcessIdField[] = "processId";
static const char kRequestTypeField[] = "requestType";
static const char kRoutingIdField[] = "routingId";
static const char kSessionIdField[] = "sessionId";
static const char kShouldRequestTreeField[] = "shouldRequestTree";
static const char kSupportedApiTypesField[] = "supportedApiTypes";
static const char kStartField[] = "start";
static const char kTreeField[] = "tree";
static const char kTypeField[] = "type";
static const char kUrlField[] = "url";
static const char kValueField[] = "value";
static const char kWidgetsField[] = "widgets";
static const char kApiTypeField[] = "apiType";

#if defined(USE_AURA) && !BUILDFLAG(IS_CHROMEOS_ASH)
static const char kWidgetIdField[] = "widgetId";
static const char kWidget[] = "widget";
#endif

// Global flags
static const char kBrowser[] = "browser";
static const char kCopyTree[] = "copyTree";
static const char kHTML[] = "html";
static const char kLocked[] = "locked";
static const char kNative[] = "native";
static const char kPage[] = "page";
static const char kPDFPrinting[] = "pdfPrinting";
static const char kScreenReader[] = "screenreader";
static const char kShowOrRefreshTree[] = "showOrRefreshTree";
static const char kText[] = "text";
static const char kViewsAccessibility[] = "viewsAccessibility";
static const char kWeb[] = "web";

// Possible global flag values
static const char kDisabled[] = "disabled";
static const char kOff[] = "off";
static const char kOn[] = "on";

using ui::AXPropertyFilter;

namespace {

base::Value::Dict BuildTargetDescriptor(
    const GURL& url,
    const std::string& name,
    const GURL& favicon_url,
    int process_id,
    int routing_id,
    ui::AXMode accessibility_mode,
    base::ProcessHandle handle = base::kNullProcessHandle) {
  base::Value::Dict target_data;
  target_data.Set(kProcessIdField, process_id);
  target_data.Set(kRoutingIdField, routing_id);
  target_data.Set(kUrlField, url.spec());
  target_data.Set(kNameField, base::EscapeForHTML(name));
  target_data.Set(kPidField, static_cast<int>(base::GetProcId(handle)));
  target_data.Set(kFaviconUrlField, favicon_url.spec());
  target_data.Set(kAccessibilityModeField,
                  static_cast<int>(accessibility_mode.flags()));
  target_data.Set(kTypeField, kPage);
  return target_data;
}

base::Value::Dict BuildTargetDescriptor(content::RenderViewHost* rvh) {
  TRACE_EVENT1("accessibility", "BuildTargetDescriptor", "render_view_host",
               rvh);
  content::WebContents* web_contents =
      content::WebContents::FromRenderViewHost(rvh);
  ui::AXMode accessibility_mode;

  std::string title;
  GURL url;
  GURL favicon_url;
  if (web_contents) {
    // TODO(nasko): Fix the following code to use a consistent set of data
    // across the URL, title, and favicon.
    url = web_contents->GetURL();
    title = base::UTF16ToUTF8(web_contents->GetTitle());
    content::NavigationController& controller = web_contents->GetController();
    content::NavigationEntry* entry = controller.GetVisibleEntry();
    if (entry != nullptr && entry->GetURL().is_valid()) {
      gfx::Image favicon_image = entry->GetFavicon().image;
      if (!favicon_image.IsEmpty()) {
        const SkBitmap* favicon_bitmap = favicon_image.ToSkBitmap();
        favicon_url = GURL(webui::GetBitmapDataUrl(*favicon_bitmap));
      }
    }
    accessibility_mode = web_contents->GetAccessibilityMode();
  }

  return BuildTargetDescriptor(url, title, favicon_url,
                               rvh->GetProcess()->GetID(), rvh->GetRoutingID(),
                               accessibility_mode);
}

#if !BUILDFLAG(IS_ANDROID)
base::Value::Dict BuildTargetDescriptor(Browser* browser) {
  base::Value::Dict target_data;
  target_data.Set(kSessionIdField, browser->session_id().id());
  target_data.Set(kNameField, browser->GetWindowTitleForCurrentTab(false));
  target_data.Set(kTypeField, kBrowser);
  return target_data;
}
#endif  // !BUILDFLAG(IS_ANDROID)

#if defined(USE_AURA) && !BUILDFLAG(IS_CHROMEOS_ASH)
base::Value::Dict BuildTargetDescriptor(views::Widget* widget) {
  base::Value::Dict widget_data;
  widget_data.Set(kNameField, widget->widget_delegate()->GetWindowTitle());
  widget_data.Set(kTypeField, kWidget);

  // Use the Widget's root view ViewAccessibility's unique ID for lookup.
  int id = widget->GetRootView()->GetViewAccessibility().GetUniqueId();
  widget_data.Set(kWidgetIdField, id);
  return widget_data;
}
#endif  // defined(USE_AURA) && !BUILDFLAG(IS_CHROMEOS_ASH)

bool ShouldHandleAccessibilityRequestCallback(const std::string& path) {
  return path == kTargetsDataFile;
}

void HandleAccessibilityRequestCallback(
    content::BrowserContext* current_context,
    const std::string& path,
    content::WebUIDataSource::GotDataCallback callback) {
  DCHECK(ShouldHandleAccessibilityRequestCallback(path));

  base::Value::Dict data;
  PrefService* pref = Profile::FromBrowserContext(current_context)->GetPrefs();
  ui::AXMode mode =
      content::BrowserAccessibilityState::GetInstance()->GetAccessibilityMode();
  bool is_native_enabled = content::BrowserAccessibilityState::GetInstance()
                               ->IsRendererAccessibilityEnabled();
  bool native = mode.has_mode(ui::AXMode::kNativeAPIs);
  bool web = mode.has_mode(ui::AXMode::kWebContents);
  bool text = mode.has_mode(ui::AXMode::kInlineTextBoxes);
  bool screenreader = mode.has_mode(ui::AXMode::kScreenReader);
  bool html = mode.has_mode(ui::AXMode::kHTML);
  bool pdf_printing = mode.has_mode(ui::AXMode::kPDFPrinting);

  // The "native" and "web" flags are disabled if
  // --disable-renderer-accessibility is set.
  data.Set(kNative, is_native_enabled ? (native ? kOn : kOff) : kDisabled);
  data.Set(kWeb, is_native_enabled ? (web ? kOn : kOff) : kDisabled);

  // The "text", "screenreader" and "html" flags are only
  // meaningful if "web" is enabled.
  bool is_web_enabled = is_native_enabled && web;
  data.Set(kText, is_web_enabled ? (text ? kOn : kOff) : kDisabled);
  data.Set(kScreenReader,
           is_web_enabled ? (screenreader ? kOn : kOff) : kDisabled);
  data.Set(kHTML, is_web_enabled ? (html ? kOn : kOff) : kDisabled);

  // The "pdfPrinting" flag is independent of the others.
  data.Set(kPDFPrinting, pdf_printing ? kOn : kOff);

  // The "Top Level Widgets" section is only relevant if views accessibility is
  // enabled.
  data.Set(kViewsAccessibility, features::IsAccessibilityTreeForViewsEnabled());

  std::string pref_api_type =
      pref->GetString(prefs::kShownAccessibilityApiType);
  bool pref_api_type_supported = false;

  std::vector<ui::AXApiType::Type> supported_api_types =
      content::AXInspectFactory::SupportedApis();
  base::Value::List supported_api_list;
  supported_api_list.reserve(supported_api_types.size());
  for (ui::AXApiType::Type type : supported_api_types) {
    supported_api_list.Append(std::string_view(type));
    if (type == ui::AXApiType::From(pref_api_type)) {
      pref_api_type_supported = true;
    }
  }
  data.Set(kSupportedApiTypesField, std::move(supported_api_list));

  // If the saved API type is not supported, use the default platform formatter
  // API type.
  if (!pref_api_type_supported) {
    pref_api_type = std::string_view(
        content::AXInspectFactory::DefaultPlatformFormatterType());
  }
  data.Set(kApiTypeField, pref_api_type);

  bool is_mode_locked = !content::BrowserAccessibilityState::GetInstance()
                             ->IsAXModeChangeAllowed();
  data.Set(kLocked, is_mode_locked ? kOn : kOff);

  base::Value::List page_list;
  std::unique_ptr<content::RenderWidgetHostIterator> widget_iter(
      content::RenderWidgetHost::GetRenderWidgetHosts());

  while (content::RenderWidgetHost* widget = widget_iter->GetNextHost()) {
    // Ignore processes that don't have a connection, such as crashed tabs.
    if (!widget->GetProcess()->IsInitializedAndNotDead()) {
      continue;
    }
    content::RenderViewHost* rvh = content::RenderViewHost::From(widget);
    if (!rvh) {
      continue;
    }
    content::WebContents* web_contents =
        content::WebContents::FromRenderViewHost(rvh);
    content::WebContentsDelegate* delegate = web_contents->GetDelegate();
    if (!delegate) {
      continue;
    }
    if (web_contents->GetPrimaryMainFrame()->GetRenderViewHost() != rvh) {
      continue;
    }
    // Ignore views that are never user-visible, like background pages.
    if (delegate->IsNeverComposited(web_contents)) {
      continue;
    }
    content::BrowserContext* context = rvh->GetProcess()->GetBrowserContext();
    if (context != current_context) {
      continue;
    }

    base::Value::Dict descriptor = BuildTargetDescriptor(rvh);
    descriptor.Set(kNative, is_native_enabled);
    descriptor.Set(kScreenReader, is_web_enabled && screenreader);
    descriptor.Set(kWeb, is_web_enabled);
    page_list.Append(std::move(descriptor));
  }
  data.Set(kPagesField, std::move(page_list));

  base::Value::List browser_list;
#if !BUILDFLAG(IS_ANDROID)
  for (Browser* browser : *BrowserList::GetInstance()) {
    browser_list.Append(BuildTargetDescriptor(browser));
  }
#endif  // !BUILDFLAG(IS_ANDROID)
  data.Set(kBrowsersField, std::move(browser_list));

  base::Value::List widgets_list;
#if defined(USE_AURA) && !BUILDFLAG(IS_CHROMEOS_ASH)
  if (features::IsAccessibilityTreeForViewsEnabled()) {
    views::WidgetAXTreeIDMap& manager_map =
        views::WidgetAXTreeIDMap::GetInstance();
    const std::vector<views::Widget*> widgets = manager_map.GetWidgets();
    for (views::Widget* widget : widgets) {
      widgets_list.Append(BuildTargetDescriptor(widget));
    }
  }
#endif  // defined(USE_AURA) && !BUILDFLAG(IS_CHROMEOS_ASH)
  data.Set(kWidgetsField, std::move(widgets_list));

  std::string json_string;
  base::JSONWriter::Write(data, &json_string);

  std::move(callback).Run(
      base::MakeRefCounted<base::RefCountedString>(std::move(json_string)));
}

std::string RecursiveDumpAXPlatformNodeAsString(
    ui::AXPlatformNode* node,
    int indent,
    const std::vector<AXPropertyFilter>& property_filters) {
  if (!node) {
    return "";
  }
  std::string str(2 * indent, '+');
  std::string line = node->GetDelegate()->GetData().ToString();
  std::vector<std::string> attributes = base::SplitString(
      line, " ", base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  for (std::string attribute : attributes) {
    if (ui::AXTreeFormatter::MatchesPropertyFilters(property_filters, attribute,
                                                    false)) {
      str += attribute + " ";
    }
  }
  str += "\n";
  for (size_t i = 0; i < node->GetDelegate()->GetChildCount(); i++) {
    gfx::NativeViewAccessible child = node->GetDelegate()->ChildAtIndex(i);
    ui::AXPlatformNode* child_node =
        ui::AXPlatformNode::FromNativeViewAccessible(child);
    str += RecursiveDumpAXPlatformNodeAsString(child_node, indent + 1,
                                               property_filters);
  }
  return str;
}

// Add property filters to the property_filters vector for the given property
// filter type. The attributes are passed in as a string with each attribute
// separated by a space.
void AddPropertyFilters(std::vector<AXPropertyFilter>& property_filters,
                        const std::string& attributes,
                        AXPropertyFilter::Type type) {
  for (const std::string& attribute : base::SplitString(
           attributes, " ", base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY)) {
    property_filters.emplace_back(attribute, type);
  }
}

bool IsValidJSValue(const std::string* str) {
  return str && str->length() < 5000U;
}

const std::string& CheckJSValue(const std::string* str) {
  CHECK(IsValidJSValue(str));
  return *str;
}

}  // namespace

AccessibilityUIConfig::AccessibilityUIConfig()
    : DefaultWebUIConfig(content::kChromeUIScheme,
                         chrome::kChromeUIAccessibilityHost) {}

AccessibilityUIConfig::~AccessibilityUIConfig() = default;

AccessibilityUI::AccessibilityUI(content::WebUI* web_ui)
    : WebUIController(web_ui) {
  auto* const browser_context = web_ui->GetWebContents()->GetBrowserContext();
  // Set up the chrome://accessibility source.
  content::WebUIDataSource* html_source =
      content::WebUIDataSource::CreateAndAdd(
          browser_context, chrome::kChromeUIAccessibilityHost);

  // Add required resources.
  html_source->UseStringsJs();
  html_source->AddResourcePaths(
      base::make_span(kAccessibilityResources, kAccessibilityResourcesSize));
  html_source->SetDefaultResource(IDR_ACCESSIBILITY_ACCESSIBILITY_HTML);
  html_source->SetRequestFilter(
      base::BindRepeating(&ShouldHandleAccessibilityRequestCallback),
      base::BindRepeating(&HandleAccessibilityRequestCallback,
                          browser_context));
  html_source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::TrustedTypes,
      "trusted-types parse-html-subset sanitize-inner-html;");

  web_ui->AddMessageHandler(std::make_unique<AccessibilityUIMessageHandler>());
}

AccessibilityUI::~AccessibilityUI() = default;

AccessibilityUIObserver::AccessibilityUIObserver(
    content::WebContents* web_contents,
    std::vector<std::string>* event_logs)
    : content::WebContentsObserver(web_contents), event_logs_(event_logs) {}

AccessibilityUIObserver::~AccessibilityUIObserver() = default;

void AccessibilityUIObserver::AccessibilityEventReceived(
    const ui::AXUpdatesAndEvents& details) {
  for (const ui::AXEvent& event : details.events) {
    event_logs_->push_back(event.ToString());
  }
}

AccessibilityUIMessageHandler::PageAccessibilityMode::PageAccessibilityMode(
    base::WeakPtr<content::WebContents> web_contents,
    std::unique_ptr<content::ScopedAccessibilityMode> accessibility_mode)
    : web_contents(std::move(web_contents)),
      accessibility_mode(std::move(accessibility_mode)) {}

AccessibilityUIMessageHandler::PageAccessibilityMode::PageAccessibilityMode(
    PageAccessibilityMode&& other) noexcept = default;

AccessibilityUIMessageHandler::PageAccessibilityMode::~PageAccessibilityMode() =
    default;

AccessibilityUIMessageHandler::AccessibilityUIMessageHandler()
    : process_accessibility_mode_(
          content::BrowserAccessibilityState::GetInstance()
              ->CreateScopedModeForProcess(ui::AXMode())) {}

AccessibilityUIMessageHandler::~AccessibilityUIMessageHandler() {
  if (!observer_) {
    return;
  }
  content::WebContents* web_contents = observer_->web_contents();
  if (!web_contents) {
    return;
  }
  StopRecording(web_contents);
}

void AccessibilityUIMessageHandler::RegisterMessages() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  web_ui()->RegisterMessageCallback(
      "toggleAccessibility",
      base::BindRepeating(
          &AccessibilityUIMessageHandler::ToggleAccessibilityForWebContents,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "setGlobalFlag",
      base::BindRepeating(&AccessibilityUIMessageHandler::SetGlobalFlag,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "setGlobalString",
      base::BindRepeating(&AccessibilityUIMessageHandler::SetGlobalString,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "requestWebContentsTree",
      base::BindRepeating(
          &AccessibilityUIMessageHandler::RequestWebContentsTree,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "requestNativeUITree",
      base::BindRepeating(&AccessibilityUIMessageHandler::RequestNativeUITree,
                          base::Unretained(this)));

#if defined(USE_AURA) && !BUILDFLAG(IS_CHROMEOS_ASH)
  web_ui()->RegisterMessageCallback(
      "requestWidgetsTree",
      base::BindRepeating(&AccessibilityUIMessageHandler::RequestWidgetsTree,
                          base::Unretained(this)));
#endif

  web_ui()->RegisterMessageCallback(
      "requestAccessibilityEvents",
      base::BindRepeating(
          &AccessibilityUIMessageHandler::RequestAccessibilityEvents,
          base::Unretained(this)));
}

void AccessibilityUIMessageHandler::SetAccessibilityModeForWebContents(
    content::WebContents* web_contents,
    ui::AXMode mode) {
  // Erase any items in the container for WebContentses that have since been
  // destroyed.
  std::erase_if(page_accessibility_modes_, [](const auto& item) {
    return item.second.web_contents.WasInvalidated();
  });

  // Create/replace a ScopedAccessibilityMode targeting `web_contents`.
  page_accessibility_modes_.insert_or_assign(
      web_contents,
      PageAccessibilityMode(
          web_contents->GetWeakPtr(),
          content::BrowserAccessibilityState::GetInstance()
              ->CreateScopedModeForWebContents(web_contents, mode)));
}

void AccessibilityUIMessageHandler::ToggleAccessibilityForWebContents(
    const base::Value::List& args) {
  const base::Value::Dict& data = args[0].GetDict();

  int process_id = *data.FindInt(kProcessIdField);
  int routing_id = *data.FindInt(kRoutingIdField);
  int mode = *data.FindInt(kModeIdField);
  bool should_request_tree = *data.FindBool(kShouldRequestTreeField);

  AllowJavascript();
  content::RenderViewHost* rvh =
      content::RenderViewHost::FromID(process_id, routing_id);
  if (!rvh) {
    return;
  }
  content::WebContents* web_contents =
      content::WebContents::FromRenderViewHost(rvh);
  ui::AXMode current_mode = web_contents->GetAccessibilityMode();

  if (mode & ui::AXMode::kNativeAPIs) {
    current_mode.set_mode(ui::AXMode::kNativeAPIs, true);
  }

  if (mode & ui::AXMode::kWebContents) {
    current_mode.set_mode(ui::AXMode::kWebContents, true);
  }

  if (mode & ui::AXMode::kInlineTextBoxes) {
    current_mode.set_mode(ui::AXMode::kInlineTextBoxes, true);
  }

  if (mode & ui::AXMode::kScreenReader) {
    current_mode.set_mode(ui::AXMode::kScreenReader, true);
  }

  if (mode & ui::AXMode::kHTML) {
    current_mode.set_mode(ui::AXMode::kHTML, true);
  }

  SetAccessibilityModeForWebContents(web_contents, current_mode);

  if (should_request_tree) {
    base::Value::Dict request_data;
    request_data.Set(kProcessIdField, process_id);
    request_data.Set(kRoutingIdField, routing_id);
    request_data.Set(kRequestTypeField, kShowOrRefreshTree);
    base::Value::List request_args;
    request_args.Append(std::move(request_data));
    RequestWebContentsTree(request_args);
  } else {
    // Call accessibility.showOrRefreshTree without a 'tree' field so the row's
    // accessibility mode buttons are updated.
    AllowJavascript();
    base::Value::Dict new_mode = BuildTargetDescriptor(rvh);
    FireWebUIListener("showOrRefreshTree", new_mode);
  }
}

void AccessibilityUIMessageHandler::SetGlobalFlag(
    const base::Value::List& args) {
  const base::Value::Dict& data = args[0].GetDict();
  const std::string flag_name = CheckJSValue(data.FindString(kFlagNameField));
  bool enabled = *data.FindBool(kEnabledField);

  AllowJavascript();
  if (flag_name == kLocked) {
    content::BrowserAccessibilityState::GetInstance()->SetAXModeChangeAllowed(
        !enabled);
    return;
  }

  ui::AXMode new_mode;
  if (flag_name == kNative) {
    new_mode = ui::AXMode::kNativeAPIs;
  } else if (flag_name == kWeb) {
    new_mode = ui::AXMode::kWebContents;
  } else if (flag_name == kText) {
    new_mode = ui::AXMode::kInlineTextBoxes;
  } else if (flag_name == kScreenReader) {
    new_mode = ui::AXMode::kScreenReader;
  } else if (flag_name == kHTML) {
    new_mode = ui::AXMode::kHTML;
  } else {
    NOTREACHED_IN_MIGRATION();
    return;
  }

  // It doesn't make sense to enable one of the flags that depends on
  // web contents without enabling web contents accessibility too.
  if (enabled && (new_mode.has_mode(ui::AXMode::kInlineTextBoxes) ||
                  new_mode.has_mode(ui::AXMode::kScreenReader) ||
                  new_mode.has_mode(ui::AXMode::kHTML))) {
    new_mode.set_mode(ui::AXMode::kWebContents, true);
  }

  // Similarly if you disable web accessibility we should remove all
  // flags that depend on it.
  if (!enabled && new_mode.has_mode(ui::AXMode::kWebContents)) {
    new_mode.set_mode(ui::AXMode::kInlineTextBoxes, true);
    new_mode.set_mode(ui::AXMode::kScreenReader, true);
    new_mode.set_mode(ui::AXMode::kHTML, true);
  }

  ui::AXMode mode = process_accessibility_mode_->mode();
  mode.set_mode(new_mode.flags(), enabled);
  process_accessibility_mode_ =
      content::BrowserAccessibilityState::GetInstance()
          ->CreateScopedModeForProcess(mode);

  // It's possible that the user is trying to remove a global flag that was set
  // outside of chrome://accessibility. Modify the process-wide state
  // accordingly. Note that this change will persist beyond the lifetime of
  // chrome://accessibility.
  if (!enabled) {
    content::BrowserAccessibilityState::GetInstance()
        ->RemoveAccessibilityModeFlags(new_mode);
  }
}

void AccessibilityUIMessageHandler::SetGlobalString(
    const base::Value::List& args) {
  const base::Value::Dict& data = args[0].GetDict();

  const std::string string_name =
      CheckJSValue(data.FindString(kStringNameField));
  const std::string value = CheckJSValue(data.FindString(kValueField));

  if (string_name == kApiTypeField) {
    PrefService* pref = Profile::FromWebUI(web_ui())->GetPrefs();
    pref->SetString(prefs::kShownAccessibilityApiType, value);
  }
}

void AccessibilityUIMessageHandler::GetRequestTypeAndFilters(
    const base::Value::Dict& data,
    std::string& request_type,
    std::string& allow,
    std::string& allow_empty,
    std::string& deny) {
  request_type = CheckJSValue(data.FindString(kRequestTypeField));
  CHECK(request_type == kShowOrRefreshTree || request_type == kCopyTree);
  allow = CheckJSValue(data.FindStringByDottedPath("filters.allow"));
  allow_empty = CheckJSValue(data.FindStringByDottedPath("filters.allowEmpty"));
  deny = CheckJSValue(data.FindStringByDottedPath("filters.deny"));
}

void AccessibilityUIMessageHandler::RequestWebContentsTree(
    const base::Value::List& args) {
  const base::Value::Dict& data = args[0].GetDict();

  std::string request_type, allow, allow_empty, deny;
  GetRequestTypeAndFilters(data, request_type, allow, allow_empty, deny);

  int process_id = *data.FindInt(kProcessIdField);
  int routing_id = *data.FindInt(kRoutingIdField);

  AllowJavascript();
  content::RenderViewHost* rvh =
      content::RenderViewHost::FromID(process_id, routing_id);
  if (!rvh) {
    base::Value::Dict result;
    result.Set(kProcessIdField, process_id);
    result.Set(kRoutingIdField, routing_id);
    result.Set(kErrorField, "Renderer no longer exists.");
    FireWebUIListener(request_type, result);
    return;
  }

  base::Value::Dict result(BuildTargetDescriptor(rvh));
  content::WebContents* web_contents =
      content::WebContents::FromRenderViewHost(rvh);
  // No matter the state of the current web_contents, we want to force the mode
  // because we are about to show the accessibility tree
  SetAccessibilityModeForWebContents(web_contents, ui::kAXModeComplete);

  std::vector<AXPropertyFilter> property_filters;
  AddPropertyFilters(property_filters, allow, AXPropertyFilter::ALLOW);
  AddPropertyFilters(property_filters, allow_empty,
                     AXPropertyFilter::ALLOW_EMPTY);
  AddPropertyFilters(property_filters, deny, AXPropertyFilter::DENY);

  PrefService* pref = Profile::FromWebUI(web_ui())->GetPrefs();
  ui::AXApiType::Type api_type =
      ui::AXApiType::From(pref->GetString(prefs::kShownAccessibilityApiType));
  std::string accessibility_contents =
      web_contents->DumpAccessibilityTree(api_type, property_filters);
  result.Set(kTreeField, accessibility_contents);
  FireWebUIListener(request_type, result);
}

void AccessibilityUIMessageHandler::RequestNativeUITree(
    const base::Value::List& args) {
  const base::Value::Dict& data = args[0].GetDict();

  std::string request_type, allow, allow_empty, deny;
  GetRequestTypeAndFilters(data, request_type, allow, allow_empty, deny);

  int session_id = *data.FindInt(kSessionIdField);

  AllowJavascript();

#if !BUILDFLAG(IS_ANDROID)
  std::vector<AXPropertyFilter> property_filters;
  AddPropertyFilters(property_filters, allow, AXPropertyFilter::ALLOW);
  AddPropertyFilters(property_filters, allow_empty,
                     AXPropertyFilter::ALLOW_EMPTY);
  AddPropertyFilters(property_filters, deny, AXPropertyFilter::DENY);

  for (Browser* browser : *BrowserList::GetInstance()) {
    if (browser->session_id().id() == session_id) {
      base::Value::Dict result = BuildTargetDescriptor(browser);
      gfx::NativeWindow native_window = browser->window()->GetNativeWindow();
      ui::AXPlatformNode* node =
          ui::AXPlatformNode::FromNativeWindow(native_window);
      result.Set(kTreeField, RecursiveDumpAXPlatformNodeAsString(
                                 node, 0, property_filters));
      FireWebUIListener(request_type, result);
      return;
    }
  }
#endif  // !BUILDFLAG(IS_ANDROID)
  // No browser with the specified |session_id| was found.
  base::Value::Dict result;
  result.Set(kSessionIdField, session_id);
  result.Set(kTypeField, kBrowser);
  result.Set(kErrorField, "Browser no longer exists.");
  FireWebUIListener(request_type, result);
}

void AccessibilityUIMessageHandler::RequestWidgetsTree(
    const base::Value::List& args) {
#if defined(USE_AURA) && !BUILDFLAG(IS_CHROMEOS_ASH)
  const base::Value::Dict& data = args[0].GetDict();

  std::string request_type, allow, allow_empty, deny;
  GetRequestTypeAndFilters(data, request_type, allow, allow_empty, deny);

  std::vector<AXPropertyFilter> property_filters;
  AddPropertyFilters(property_filters, allow, AXPropertyFilter::ALLOW);
  AddPropertyFilters(property_filters, allow_empty,
                     AXPropertyFilter::ALLOW_EMPTY);
  AddPropertyFilters(property_filters, deny, AXPropertyFilter::DENY);

  if (features::IsAccessibilityTreeForViewsEnabled()) {
    int widget_id = *data.FindInt(kWidgetIdField);
    views::WidgetAXTreeIDMap& manager_map =
        views::WidgetAXTreeIDMap::GetInstance();
    const std::vector<views::Widget*> widgets = manager_map.GetWidgets();
    for (views::Widget* widget : widgets) {
      int current_id =
          widget->GetRootView()->GetViewAccessibility().GetUniqueId();
      if (current_id == widget_id) {
        ui::AXTreeID tree_id = manager_map.GetWidgetTreeID(widget);
        DCHECK_NE(tree_id, ui::AXTreeIDUnknown());
        std::unique_ptr<ui::AXTreeFormatter> formatter(
            content::AXInspectFactory::CreateBlinkFormatter());
        std::string tree_dump =
            formatter->DumpInternalAccessibilityTree(tree_id, property_filters);

        base::Value::Dict result(BuildTargetDescriptor(widget));
        result.Set(kTreeField, tree_dump);
        AllowJavascript();
        FireWebUIListener(request_type, result);
        return;
      }
    }
  }

  base::Value::Dict result;
  result.Set(kTypeField, kWidget);
  result.Set(kErrorField, "Window no longer exists.");
  AllowJavascript();
  FireWebUIListener(request_type, result);
#endif  // defined(USE_AURA) && !BUILDFLAG(IS_CHROMEOS_ASH)
}

void AccessibilityUIMessageHandler::Callback(const std::string& str) {
  event_logs_.push_back(str);
}

void AccessibilityUIMessageHandler::StopRecording(
    content::WebContents* web_contents) {
  web_contents->RecordAccessibilityEvents(
      GetRecordingApiType(), /*start_recording=*/false, std::nullopt);
  observer_.reset(nullptr);
}

ui::AXApiType::Type AccessibilityUIMessageHandler::GetRecordingApiType() {
  PrefService* pref = Profile::FromWebUI(web_ui())->GetPrefs();
  const std::vector<ui::AXApiType::Type> supported_types =
      content::AXInspectFactory::SupportedApis();
  ui::AXApiType::Type api_type =
      ui::AXApiType::From(pref->GetString(prefs::kShownAccessibilityApiType));
  // Check to see if it is in the supported types list.
  if (std::find(supported_types.begin(), supported_types.end(), api_type) ==
      supported_types.end()) {
    api_type = content::AXInspectFactory::DefaultPlatformRecorderType();
  }

  // Special cases for recording.
  if (api_type == ui::AXApiType::kWinUIA) {
    // The UIA event recorder currently does not work outside of tests,
    // so use the platform default if the tree view is set to UIA.
    // TODO: Remove this once the UIA event recorder is fixed. See:
    // https://crbug.com/325316128
    api_type = content::AXInspectFactory::DefaultPlatformRecorderType();
  }
  if (api_type == ui::AXApiType::kBlink) {
    // kBlink is not a supported recording type, so use the platform default if
    // the tree view is set to kBlink.
    api_type = content::AXInspectFactory::DefaultPlatformRecorderType();
  }
  return api_type;
}

void AccessibilityUIMessageHandler::RequestAccessibilityEvents(
    const base::Value::List& args) {
  const base::Value::Dict& data = args[0].GetDict();

  int process_id = *data.FindInt(kProcessIdField);
  int routing_id = *data.FindInt(kRoutingIdField);
  bool start_recording = *data.FindBool(kStartField);

  AllowJavascript();

  content::RenderViewHost* rvh =
      content::RenderViewHost::FromID(process_id, routing_id);
  if (!rvh) {
    return;
  }

  base::Value::Dict result = BuildTargetDescriptor(rvh);
  content::WebContents* web_contents =
      content::WebContents::FromRenderViewHost(rvh);
  if (start_recording) {
    if (observer_) {
      return;
    }
    web_contents->RecordAccessibilityEvents(
        GetRecordingApiType(), /*start_recording=*/true,
        base::BindRepeating(&AccessibilityUIMessageHandler::Callback,
                            weak_ptr_factory_.GetWeakPtr()));
    observer_ =
        std::make_unique<AccessibilityUIObserver>(web_contents, &event_logs_);
  } else {
    StopRecording(web_contents);

    std::string event_logs_str;
    for (std::string log : event_logs_) {
      event_logs_str += log;
      event_logs_str += "\n";
    }
    result.Set(kEventLogsField, event_logs_str);
    event_logs_.clear();

    FireWebUIListener("startOrStopEvents", result);
  }
}

// static
void AccessibilityUIMessageHandler::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  const std::string_view default_api_type =
      std::string_view(ui::AXApiType::Type(ui::AXApiType::kBlink));
  registry->RegisterStringPref(prefs::kShownAccessibilityApiType,
                               std::string(default_api_type));
}
