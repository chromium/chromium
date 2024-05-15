// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/plugins/chrome_plugin_placeholder.h"

#include <memory>
#include <set>
#include <utility>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/common/buildflags.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/renderer_resources.h"
#include "chrome/renderer/chrome_content_renderer_client.h"
#include "chrome/renderer/custom_menu_commands.h"
#include "components/content_settings/renderer/content_settings_agent_impl.h"
#include "components/no_state_prefetch/renderer/prerender_observer_list.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/common/content_switches.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"
#include "gin/object_template_builder.h"
#include "ipc/ipc_sync_channel.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"
#include "third_party/blink/public/common/context_menu_data/untrustworthy_context_menu_params.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"
#include "third_party/blink/public/common/page/page_zoom.h"
#include "third_party/blink/public/platform/url_conversion.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_frame_widget.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_plugin_container.h"
#include "third_party/blink/public/web/web_script_source.h"
#include "third_party/blink/public/web/web_view.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/webui/jstemplate_builder.h"
#include "ui/gfx/geometry/size.h"
#include "url/origin.h"
#include "url/url_util.h"

using base::UserMetricsAction;
using content::RenderThread;

namespace {
const ChromePluginPlaceholder* g_last_active_menu = nullptr;

const char kPlaceholderSetKey[] = "kPlaceholderSetKey";

class PlaceholderSet : public base::SupportsUserData::Data {
 public:
  ~PlaceholderSet() override = default;

  static PlaceholderSet* Get(content::RenderFrame* render_frame) {
    DCHECK(render_frame);
    return static_cast<PlaceholderSet*>(
        render_frame->GetUserData(kPlaceholderSetKey));
  }

  static PlaceholderSet* GetOrCreate(content::RenderFrame* render_frame) {
    PlaceholderSet* set = Get(render_frame);
    if (!set) {
      set = new PlaceholderSet();
      render_frame->SetUserData(kPlaceholderSetKey, base::WrapUnique(set));
    }
    return set;
  }

  std::set<raw_ptr<ChromePluginPlaceholder, SetExperimental>>& placeholders() {
    return placeholders_;
  }

 private:
  PlaceholderSet() = default;

  std::set<raw_ptr<ChromePluginPlaceholder, SetExperimental>> placeholders_;
};

}  // namespace

gin::WrapperInfo ChromePluginPlaceholder::kWrapperInfo = {
    gin::kEmbedderNativeGin};

ChromePluginPlaceholder::ChromePluginPlaceholder(
    content::RenderFrame* render_frame,
    const blink::WebPluginParams& params,
    const std::u16string& title)
    : plugins::LoadablePluginPlaceholder(render_frame, params),
      status_(chrome::mojom::PluginStatus::kAllowed),
      title_(title) {
  RenderThread::Get()->AddObserver(this);
  prerender::PrerenderObserverList::AddObserverForFrame(render_frame, this);

  // Keep track of all placeholders associated with |render_frame|.
  PlaceholderSet::GetOrCreate(render_frame)->placeholders().insert(this);
}

ChromePluginPlaceholder::~ChromePluginPlaceholder() {
  RenderThread::Get()->RemoveObserver(this);

  // The render frame may already be gone.
  if (render_frame()) {
    PlaceholderSet* set = PlaceholderSet::Get(render_frame());
    if (set)
      set->placeholders().erase(this);

    prerender::PrerenderObserverList::RemoveObserverForFrame(render_frame(),
                                                             this);
  }
}

// TODO(bauerb): Move this method to NonLoadablePluginPlaceholder?
// static
ChromePluginPlaceholder* ChromePluginPlaceholder::CreateLoadableMissingPlugin(
    content::RenderFrame* render_frame,
    const blink::WebPluginParams& params) {
  std::string template_html =
      ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
          IDR_BLOCKED_PLUGIN_HTML);

  base::Value::Dict values;
  values.Set("name", "");
  values.Set("message", l10n_util::GetStringUTF8(IDS_PLUGIN_NOT_SUPPORTED));

  std::string html_data = webui::GetI18nTemplateHtml(template_html, values);

  // Will destroy itself when its WebViewPlugin is going away.
  auto* placeholder = new ChromePluginPlaceholder(render_frame, params,
                                                  params.mime_type.Utf16());
  placeholder->Init(html_data);
  return placeholder;
}

// static
ChromePluginPlaceholder* ChromePluginPlaceholder::CreateBlockedPlugin(
    content::RenderFrame* render_frame,
    const blink::WebPluginParams& params,
    const content::WebPluginInfo& info,
    const std::string& identifier,
    const std::u16string& name,
    int template_id,
    const std::u16string& message) {
  base::Value::Dict values;
  values.Set("message", message);
  values.Set("name", name);
  values.Set("hide", l10n_util::GetStringUTF8(IDS_PLUGIN_HIDE));
  values.Set(
      "pluginType",
      render_frame->IsMainFrame() &&
              render_frame->GetWebFrame()->GetDocument().IsPluginDocument()
          ? "document"
          : "embedded");

  std::string template_html =
      ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
          template_id);

  DCHECK(!template_html.empty()) << "unable to load template. ID: "
                                 << template_id;
  std::string html_data = webui::GetI18nTemplateHtml(template_html, values);

  // |blocked_plugin| will destroy itself when its WebViewPlugin is going away.
  ChromePluginPlaceholder* blocked_plugin =
      new ChromePluginPlaceholder(render_frame, params, name);
  blocked_plugin->Init(html_data);
  blocked_plugin->SetPluginInfo(info);
  blocked_plugin->SetIdentifier(identifier);

  return blocked_plugin;
}

// static
void ChromePluginPlaceholder::ForEach(
    content::RenderFrame* render_frame,
    const base::RepeatingCallback<void(ChromePluginPlaceholder*)>& callback) {
  PlaceholderSet* set = PlaceholderSet::Get(render_frame);
  if (set) {
    for (ChromePluginPlaceholder* placeholder : set->placeholders()) {
      callback.Run(placeholder);
    }
  }
}

void ChromePluginPlaceholder::SetStatus(chrome::mojom::PluginStatus status) {
  status_ = status;
}

void ChromePluginPlaceholder::SetIsPrerendering(bool is_prerendering) {
  OnSetIsPrerendering(is_prerendering);
}

void ChromePluginPlaceholder::PluginListChanged() {
  if (!render_frame() || !plugin())
    return;

  chrome::mojom::PluginInfoPtr plugin_info = chrome::mojom::PluginInfo::New();
  std::string mime_type(GetPluginParams().mime_type.Utf8());

  mojo::AssociatedRemote<chrome::mojom::PluginInfoHost> plugin_info_host;
  render_frame()->GetRemoteAssociatedInterfaces()->GetInterface(
      &plugin_info_host);
  plugin_info_host->GetPluginInfo(
      GetPluginParams().url,
      render_frame()->GetWebFrame()->Top()->GetSecurityOrigin(), mime_type,
      &plugin_info);
  if (plugin_info->status == status_)
    return;
  blink::WebPlugin* new_plugin = ChromeContentRendererClient::CreatePlugin(
      render_frame(), GetPluginParams(), *plugin_info);
  ReplacePlugin(new_plugin);
}

v8::Local<v8::Value> ChromePluginPlaceholder::GetV8Handle(
    v8::Isolate* isolate) {
  return gin::CreateHandle(isolate, this).ToV8();
}

void ChromePluginPlaceholder::ShowContextMenu(
    const blink::WebMouseEvent& event) {
  if (context_menu_client_receiver_.is_bound())
    return;  // Don't allow nested context menu requests.

  if (!render_frame())
    return;

  blink::UntrustworthyContextMenuParams params;

  if (!title_.empty()) {
    auto name_item = blink::mojom::CustomContextMenuItem::New();
    name_item->label = title_;
    params.custom_items.push_back(std::move(name_item));

    auto separator_item = blink::mojom::CustomContextMenuItem::New();
    separator_item->type = blink::mojom::CustomContextMenuItemType::kSeparator;
    params.custom_items.push_back(std::move(separator_item));
  }

  if (!GetPluginInfo().path.value().empty()) {
    auto run_item = blink::mojom::CustomContextMenuItem::New();
    run_item->action = MENU_COMMAND_PLUGIN_RUN;
    // Disable this menu item if the plugin is blocked by policy.
    run_item->enabled = LoadingAllowed();
    run_item->label = l10n_util::GetStringUTF16(IDS_CONTENT_CONTEXT_PLUGIN_RUN);
    params.custom_items.push_back(std::move(run_item));
  }

  auto hide_item = blink::mojom::CustomContextMenuItem::New();
  hide_item->action = MENU_COMMAND_PLUGIN_HIDE;
  bool is_main_frame_plugin_document =
      render_frame()->IsMainFrame() &&
      render_frame()->GetWebFrame()->GetDocument().IsPluginDocument();
  hide_item->enabled = !is_main_frame_plugin_document;
  hide_item->label = l10n_util::GetStringUTF16(IDS_CONTENT_CONTEXT_PLUGIN_HIDE);
  params.custom_items.push_back(std::move(hide_item));

  gfx::Point point =
      gfx::Point(event.PositionInWidget().x(), event.PositionInWidget().y());
  if (plugin() && plugin()->Container())
    point = plugin()->Container()->LocalToRootFramePoint(point);

  // TODO(crbug.com/40699157): This essentially is a floor of the coordinates.
  // Determine if rounding is more appropriate.
  gfx::Rect position_in_dips =
      render_frame()
          ->GetWebFrame()
          ->LocalRoot()
          ->FrameWidget()
          ->BlinkSpaceToEnclosedDIPs(gfx::Rect(point.x(), point.y(), 0, 0));

  params.x = position_in_dips.x();
  params.y = position_in_dips.y();

  render_frame()->GetWebFrame()->ShowContextMenuFromExternal(
      params, context_menu_client_receiver_.BindNewEndpointAndPassRemote());

  g_last_active_menu = this;
}

void ChromePluginPlaceholder::CustomContextMenuAction(uint32_t action) {
  if (g_last_active_menu != this)
    return;
  switch (action) {
    case MENU_COMMAND_PLUGIN_RUN: {
      RenderThread::Get()->RecordAction(UserMetricsAction("Plugin_Load_Menu"));
      LoadPlugin();
      break;
    }
    case MENU_COMMAND_PLUGIN_HIDE: {
      RenderThread::Get()->RecordAction(UserMetricsAction("Plugin_Hide_Menu"));
      HidePlugin();
      break;
    }
    default:
      NOTREACHED_IN_MIGRATION();
  }
}

void ChromePluginPlaceholder::ContextMenuClosed(const GURL&) {
  context_menu_client_receiver_.reset();
  render_frame()->GetWebFrame()->View()->DidCloseContextMenu();
}

blink::WebPlugin* ChromePluginPlaceholder::CreatePlugin() {
  return render_frame()->CreatePlugin(GetPluginInfo(), GetPluginParams());
}

gin::ObjectTemplateBuilder ChromePluginPlaceholder::GetObjectTemplateBuilder(
    v8::Isolate* isolate) {
  gin::ObjectTemplateBuilder builder =
      gin::Wrappable<ChromePluginPlaceholder>::GetObjectTemplateBuilder(isolate)
          .SetMethod<void (ChromePluginPlaceholder::*)()>(
              "hide", &ChromePluginPlaceholder::HideCallback)
          .SetMethod<void (ChromePluginPlaceholder::*)()>(
              "load", &ChromePluginPlaceholder::LoadCallback)
          .SetMethod<void (ChromePluginPlaceholder::*)()>(
              "didFinishLoading",
              &ChromePluginPlaceholder::DidFinishLoadingCallback);

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnablePluginPlaceholderTesting)) {
    builder.SetMethod<void (ChromePluginPlaceholder::*)()>(
        "notifyPlaceholderReadyForTesting",
        &ChromePluginPlaceholder::NotifyPlaceholderReadyForTestingCallback);
  }

  return builder;
}
