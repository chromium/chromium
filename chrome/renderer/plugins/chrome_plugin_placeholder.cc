// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/plugins/chrome_plugin_placeholder.h"

#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/prerender_messages.h"
#include "chrome/common/render_messages.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/renderer_resources.h"
#include "chrome/renderer/chrome_content_renderer_client.h"
#include "chrome/renderer/content_settings_agent_impl.h"
#include "chrome/renderer/custom_menu_commands.h"
#include "chrome/renderer/plugins/plugin_preroller.h"
#include "chrome/renderer/plugins/plugin_uma.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/context_menu_params.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"
#include "gin/object_template_builder.h"
#include "ipc/ipc_sync_channel.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"
#include "third_party/blink/public/common/page/page_zoom.h"
#include "third_party/blink/public/platform/url_conversion.h"
#include "third_party/blink/public/platform/web_input_event.h"
#include "third_party/blink/public/platform/web_mouse_event.h"
#include "third_party/blink/public/web/web_document.h"
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
using content::RenderView;

namespace {
const ChromePluginPlaceholder* g_last_active_menu = nullptr;
}  // namespace

gin::WrapperInfo ChromePluginPlaceholder::kWrapperInfo = {
    gin::kEmbedderNativeGin};

ChromePluginPlaceholder::ChromePluginPlaceholder(
    content::RenderFrame* render_frame,
    const blink::WebPluginParams& params,
    const std::string& html_data,
    const base::string16& title)
    : plugins::LoadablePluginPlaceholder(render_frame, params, html_data),
      status_(chrome::mojom::PluginStatus::kAllowed),
      title_(title),
      context_menu_request_id_(0) {
  RenderThread::Get()->AddObserver(this);
}

ChromePluginPlaceholder::~ChromePluginPlaceholder() {
  RenderThread::Get()->RemoveObserver(this);
  if (context_menu_request_id_ && render_frame())
    render_frame()->CancelContextMenu(context_menu_request_id_);
}

mojo::PendingRemote<chrome::mojom::PluginRenderer>
ChromePluginPlaceholder::BindPluginRenderer() {
  return plugin_renderer_receiver_.BindNewPipeAndPassRemote();
}

// TODO(bauerb): Move this method to NonLoadablePluginPlaceholder?
// static
ChromePluginPlaceholder* ChromePluginPlaceholder::CreateLoadableMissingPlugin(
    content::RenderFrame* render_frame,
    const blink::WebPluginParams& params) {
  const base::StringPiece template_html(
      ui::ResourceBundle::GetSharedInstance().GetRawDataResource(
          IDR_BLOCKED_PLUGIN_HTML));

  base::DictionaryValue values;
  values.SetString("name", "");
  values.SetString("message",
                   l10n_util::GetStringUTF8(IDS_PLUGIN_NOT_SUPPORTED));

  std::string html_data = webui::GetI18nTemplateHtml(template_html, &values);

  // Will destroy itself when its WebViewPlugin is going away.
  return new ChromePluginPlaceholder(render_frame, params, html_data,
                                     params.mime_type.Utf16());
}

// static
ChromePluginPlaceholder* ChromePluginPlaceholder::CreateBlockedPlugin(
    content::RenderFrame* render_frame,
    const blink::WebPluginParams& params,
    const content::WebPluginInfo& info,
    const std::string& identifier,
    const base::string16& name,
    int template_id,
    const base::string16& message,
    const PowerSaverInfo& power_saver_info) {
  base::DictionaryValue values;
  values.SetString("message", message);
  values.SetString("name", name);
  values.SetString("hide", l10n_util::GetStringUTF8(IDS_PLUGIN_HIDE));
  values.SetString(
      "pluginType",
      render_frame->IsMainFrame() &&
              render_frame->GetWebFrame()->GetDocument().IsPluginDocument()
          ? "document"
          : "embedded");

  if (!power_saver_info.poster_attribute.empty()) {
    values.SetString("poster", power_saver_info.poster_attribute);
    values.SetString("baseurl", power_saver_info.base_url.spec());

    if (!power_saver_info.custom_poster_size.IsEmpty()) {
      float zoom_factor = blink::PageZoomLevelToZoomFactor(
          render_frame->GetWebFrame()->View()->ZoomLevel());
      int width =
          roundf(power_saver_info.custom_poster_size.width() / zoom_factor);
      int height =
          roundf(power_saver_info.custom_poster_size.height() / zoom_factor);
      values.SetString("visibleWidth", base::NumberToString(width) + "px");
      values.SetString("visibleHeight", base::NumberToString(height) + "px");
    } else {
      // Need to populate these to please $i18n{...} replacement mechanism.
      // 'undefined' is used on purpose as an invalid value for width and
      // height, which is ignored by CSS.
      values.SetString("visibleWidth", "undefined");
      values.SetString("visibleHeight", "undefined");
    }
  }

  const base::StringPiece template_html(
      ui::ResourceBundle::GetSharedInstance().GetRawDataResource(template_id));

  DCHECK(!template_html.empty()) << "unable to load template. ID: "
                                 << template_id;
  std::string html_data = webui::GetI18nTemplateHtml(template_html, &values);

  // |blocked_plugin| will destroy itself when its WebViewPlugin is going away.
  ChromePluginPlaceholder* blocked_plugin =
      new ChromePluginPlaceholder(render_frame, params, html_data, name);

  if (!power_saver_info.poster_attribute.empty())
    blocked_plugin->BlockForPowerSaverPoster();
  blocked_plugin->SetPluginInfo(info);
  blocked_plugin->SetIdentifier(identifier);

  blocked_plugin->set_power_saver_enabled(power_saver_info.power_saver_enabled);
  blocked_plugin->set_blocked_for_background_tab(
      power_saver_info.blocked_for_background_tab);

  return blocked_plugin;
}

void ChromePluginPlaceholder::SetStatus(chrome::mojom::PluginStatus status) {
  status_ = status;
}

bool ChromePluginPlaceholder::OnMessageReceived(const IPC::Message& message) {
  // We don't swallow these messages because multiple blocked plugins and other
  // objects have an interest in them.
  IPC_BEGIN_MESSAGE_MAP(ChromePluginPlaceholder, message)
    IPC_MESSAGE_HANDLER(PrerenderMsg_SetIsPrerendering, OnSetPrerenderMode)
    IPC_MESSAGE_HANDLER(ChromeViewMsg_LoadBlockedPlugins, OnLoadBlockedPlugins)
  IPC_END_MESSAGE_MAP()

  return false;
}

void ChromePluginPlaceholder::ShowPermissionBubbleCallback() {
  mojo::AssociatedRemote<chrome::mojom::PluginHost> plugin_host;
  render_frame()->GetRemoteAssociatedInterfaces()->GetInterface(
      plugin_host.BindNewEndpointAndPassReceiver());
  plugin_host->ShowFlashPermissionBubble();
}

void ChromePluginPlaceholder::FinishedDownloading() {
  SetMessage(l10n_util::GetStringFUTF16(IDS_PLUGIN_UPDATING, plugin_name_));
}

void ChromePluginPlaceholder::UpdateDownloading() {
  SetMessage(l10n_util::GetStringFUTF16(IDS_PLUGIN_DOWNLOADING, plugin_name_));
}

void ChromePluginPlaceholder::UpdateSuccess() {
  PluginListChanged();
}

void ChromePluginPlaceholder::UpdateFailure() {
  SetMessage(l10n_util::GetStringFUTF16(IDS_PLUGIN_DOWNLOAD_ERROR_SHORT,
                                        plugin_name_));
}

void ChromePluginPlaceholder::OnSetPrerenderMode(
    prerender::PrerenderMode mode,
    const std::string& histogram_prefix) {
  OnSetIsPrerendering(mode != prerender::NO_PRERENDER);
}

void ChromePluginPlaceholder::PluginListChanged() {
  if (!render_frame() || !plugin())
    return;

  chrome::mojom::PluginInfoPtr plugin_info = chrome::mojom::PluginInfo::New();
  std::string mime_type(GetPluginParams().mime_type.Utf8());

  ChromeContentRendererClient::GetPluginInfoHost()->GetPluginInfo(
      routing_id(), GURL(GetPluginParams().url),
      render_frame()->GetWebFrame()->Top()->GetSecurityOrigin(), mime_type,
      &plugin_info);
  if (plugin_info->status == status_)
    return;
  blink::WebPlugin* new_plugin = ChromeContentRendererClient::CreatePlugin(
      render_frame(), GetPluginParams(), *plugin_info);
  ReplacePlugin(new_plugin);
  if (!new_plugin) {
    PluginUMAReporter::GetInstance()->ReportPluginMissing(
        GetPluginParams().mime_type.Utf8(), GURL(GetPluginParams().url));
  }
}

void ChromePluginPlaceholder::OnMenuAction(int request_id, unsigned action) {
  DCHECK_EQ(context_menu_request_id_, request_id);
  if (g_last_active_menu != this)
    return;
  switch (action) {
    case MENU_COMMAND_PLUGIN_RUN: {
      RenderThread::Get()->RecordAction(UserMetricsAction("Plugin_Load_Menu"));
      MarkPluginEssential(
          content::PluginInstanceThrottler::UNTHROTTLE_METHOD_BY_CLICK);
      LoadPlugin();
      break;
    }
    case MENU_COMMAND_PLUGIN_HIDE: {
      RenderThread::Get()->RecordAction(UserMetricsAction("Plugin_Hide_Menu"));
      HidePlugin();
      break;
    }
    case MENU_COMMAND_ENABLE_FLASH: {
      ShowPermissionBubbleCallback();
      break;
    }
    default:
      NOTREACHED();
  }
}

void ChromePluginPlaceholder::OnMenuClosed(int request_id) {
  DCHECK_EQ(context_menu_request_id_, request_id);
  context_menu_request_id_ = 0;
}

v8::Local<v8::Value> ChromePluginPlaceholder::GetV8Handle(
    v8::Isolate* isolate) {
  return gin::CreateHandle(isolate, this).ToV8();
}

void ChromePluginPlaceholder::ShowContextMenu(
    const blink::WebMouseEvent& event) {
  if (context_menu_request_id_)
    return;  // Don't allow nested context menu requests.
  if (!render_frame())
    return;

  content::ContextMenuParams params;

  if (!title_.empty()) {
    content::MenuItem name_item;
    name_item.label = title_;
    params.custom_items.push_back(name_item);

    content::MenuItem separator_item;
    separator_item.type = content::MenuItem::SEPARATOR;
    params.custom_items.push_back(separator_item);
  }

  bool flash_hidden =
      status_ == chrome::mojom::PluginStatus::kFlashHiddenPreferHtml;
  if (!GetPluginInfo().path.value().empty() && !flash_hidden) {
    content::MenuItem run_item;
    run_item.action = MENU_COMMAND_PLUGIN_RUN;
    // Disable this menu item if the plugin is blocked by policy.
    run_item.enabled = LoadingAllowed();
    run_item.label = l10n_util::GetStringUTF16(IDS_CONTENT_CONTEXT_PLUGIN_RUN);
    params.custom_items.push_back(run_item);
  }

  if (flash_hidden) {
    content::MenuItem enable_flash_item;
    enable_flash_item.action = MENU_COMMAND_ENABLE_FLASH;
    enable_flash_item.enabled = true;
    enable_flash_item.label =
        l10n_util::GetStringUTF16(IDS_CONTENT_CONTEXT_ENABLE_FLASH);
    params.custom_items.push_back(enable_flash_item);
  }

  content::MenuItem hide_item;
  hide_item.action = MENU_COMMAND_PLUGIN_HIDE;
  bool is_main_frame_plugin_document =
      render_frame()->IsMainFrame() &&
      render_frame()->GetWebFrame()->GetDocument().IsPluginDocument();
  hide_item.enabled = !is_main_frame_plugin_document;
  hide_item.label = l10n_util::GetStringUTF16(IDS_CONTENT_CONTEXT_PLUGIN_HIDE);
  params.custom_items.push_back(hide_item);

  blink::WebPoint point(event.PositionInWidget().x, event.PositionInWidget().y);
  if (plugin() && plugin()->Container())
    point = plugin()->Container()->LocalToRootFramePoint(point);

  params.x = point.x;
  params.y = point.y;

  context_menu_request_id_ = render_frame()->ShowContextMenu(this, params);
  g_last_active_menu = this;
}

blink::WebPlugin* ChromePluginPlaceholder::CreatePlugin() {
  std::unique_ptr<content::PluginInstanceThrottler> throttler;
  // If the plugin has already been marked essential in its placeholder form,
  // we shouldn't create a new throttler and start the process all over again.
  if (power_saver_enabled()) {
    throttler = content::PluginInstanceThrottler::Create(
        heuristic_run_before_ ? content::RenderFrame::DONT_RECORD_DECISION
                              : content::RenderFrame::RECORD_DECISION);
    // PluginPreroller manages its own lifetime.
    new PluginPreroller(render_frame(), GetPluginParams(), GetPluginInfo(),
                        GetIdentifier(), title_,
                        l10n_util::GetStringFUTF16(IDS_PLUGIN_BLOCKED, title_),
                        throttler.get());
  }
  return render_frame()->CreatePlugin(GetPluginInfo(), GetPluginParams(),
                                      std::move(throttler));
}

void ChromePluginPlaceholder::OnBlockedContent(
    content::RenderFrame::PeripheralContentStatus status,
    bool is_same_origin) {
  DCHECK(render_frame());

  if (status ==
      content::RenderFrame::PeripheralContentStatus::CONTENT_STATUS_TINY) {
    ContentSettingsAgentImpl::Get(render_frame())
        ->DidBlockContentType(ContentSettingsType::PLUGINS);
  }

  std::string message = base::StringPrintf(
      is_same_origin ? "Same-origin plugin content from %s must have a visible "
                       "size larger than 6 x 6 pixels, or it will be blocked. "
                       "Invisible content is always blocked."
                     : "Cross-origin plugin content from %s must have a "
                       "visible size larger than 400 x 300 pixels, or it will "
                       "be blocked. Invisible content is always blocked.",
      GetPluginParams().url.GetString().Utf8().c_str());
  render_frame()->AddMessageToConsole(blink::mojom::ConsoleMessageLevel::kInfo,
                                      message);
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
              &ChromePluginPlaceholder::DidFinishLoadingCallback)
          .SetMethod("showPermissionBubble",
                     &ChromePluginPlaceholder::ShowPermissionBubbleCallback);

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnablePluginPlaceholderTesting)) {
    builder.SetMethod<void (ChromePluginPlaceholder::*)()>(
        "notifyPlaceholderReadyForTesting",
        &ChromePluginPlaceholder::NotifyPlaceholderReadyForTestingCallback);
  }

  return builder;
}
