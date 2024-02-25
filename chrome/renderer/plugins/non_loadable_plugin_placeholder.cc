// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/plugins/non_loadable_plugin_placeholder.h"

#include "base/files/file_path.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/common/plugin.mojom.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/renderer_resources.h"
#include "components/plugins/renderer/plugin_placeholder.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/renderer/render_frame.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/strings/grit/blink_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/webui/jstemplate_builder.h"

namespace {

plugins::PluginPlaceholder* CreateNonLoadablePlaceholderHelper(
    content::RenderFrame* render_frame,
    const blink::WebPluginParams& params,
    const std::string& message) {
  std::string template_html =
      ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
          IDR_BLOCKED_PLUGIN_HTML);

  base::Value::Dict values;
  values.Set("name", "");
  values.Set("message", message);

  std::string html_data = webui::GetI18nTemplateHtml(template_html, values);

  // PluginPlaceholder will destroy itself when its WebViewPlugin is going away.
  return plugins::PluginPlaceholder::Create(render_frame, params, html_data);
}

}  // namespace

// static
plugins::PluginPlaceholder*
NonLoadablePluginPlaceholder::CreateNotSupportedPlugin(
    content::RenderFrame* render_frame,
    const blink::WebPluginParams& params) {
  return CreateNonLoadablePlaceholderHelper(
      render_frame, params, l10n_util::GetStringUTF8(IDS_PLUGIN_NOT_SUPPORTED));
}

// static
plugins::PluginPlaceholder*
NonLoadablePluginPlaceholder::CreateFlashDeprecatedPlaceholder(
    content::RenderFrame* render_frame,
    const blink::WebPluginParams& params) {
  return CreateNonLoadablePlaceholderHelper(
      render_frame, params,
      l10n_util::GetStringFUTF8(IDS_PLUGIN_DEPRECATED, u"Adobe Flash Player"));
}

#if BUILDFLAG(ENABLE_PLUGINS)
// static
plugins::PluginPlaceholder* NonLoadablePluginPlaceholder::CreateErrorPlugin(
    content::RenderFrame* render_frame,
    const base::FilePath& file_path) {
  base::Value::Dict values;
  values.Set("name", "");
  values.Set("message",
             l10n_util::GetStringUTF8(IDS_PLUGIN_INITIALIZATION_ERROR));

  std::string template_html =
      ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
          IDR_BLOCKED_PLUGIN_HTML);
  std::string html_data = webui::GetI18nTemplateHtml(template_html, values);

  blink::WebPluginParams params;
  // PluginPlaceholder will destroy itself when its WebViewPlugin is going away.
  plugins::PluginPlaceholder* plugin =
      plugins::PluginPlaceholder::Create(render_frame, params, html_data);

  mojo::AssociatedRemote<chrome::mojom::PluginHost> plugin_host;
  render_frame->GetRemoteAssociatedInterfaces()->GetInterface(&plugin_host);
  plugin_host->CouldNotLoadPlugin(file_path);

  return plugin;
}
#endif  // BUILDFLAG(ENABLE_PLUGINS)
