// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_PLUGINS_PDF_PLUGIN_PLACEHOLDER_H_
#define CHROME_RENDERER_PLUGINS_PDF_PLUGIN_PLACEHOLDER_H_

#include "components/plugins/renderer/plugin_placeholder.h"

// Placeholder that allows users to click to download a PDF for when
// plugins are disabled and the PDF fails to load.
// TODO(amberwon): Flesh out the class more to download an embedded PDF when the
// PDF plugin is disabled or unavailable.
class PDFPluginPlaceholder final : public plugins::PluginPlaceholderBase,
                                   public gin::Wrappable<PDFPluginPlaceholder> {
 public:
  static gin::WrapperInfo kWrapperInfo;

  // Returned placeholder is owned by the associated plugin, which can be
  // retrieved with PluginPlaceholderBase::plugin().
  static PDFPluginPlaceholder* CreatePDFPlaceholder(
      content::RenderFrame* render_frame,
      const blink::WebPluginParams& params);

 private:
  PDFPluginPlaceholder(content::RenderFrame* render_frame,
                       const blink::WebPluginParams& params);
  ~PDFPluginPlaceholder() final;

  // WebViewPlugin::Delegate methods:
  v8::Local<v8::Value> GetV8Handle(v8::Isolate* isolate) final;

  gin::ObjectTemplateBuilder GetObjectTemplateBuilder(
      v8::Isolate* isolate) final;

  void OpenPDFCallback();
};

#endif  // CHROME_RENDERER_PLUGINS_PDF_PLUGIN_PLACEHOLDER_H_
