// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/plugins/non_loadable_plugin_placeholder.h"

#include "base/values.h"
#include "chrome/common/plugin.mojom.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/renderer_resources.h"
#include "components/plugins/renderer/plugin_placeholder.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/renderer/render_frame.h"
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

