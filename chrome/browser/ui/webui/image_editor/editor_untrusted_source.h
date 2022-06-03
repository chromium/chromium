// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_IMAGE_EDITOR_EDITOR_UNTRUSTED_SOURCE_H_
#define CHROME_BROWSER_UI_WEBUI_IMAGE_EDITOR_EDITOR_UNTRUSTED_SOURCE_H_

#include <string>

#include "content/public/browser/url_data_source.h"

class Profile;

// Serves chrome-untrusted://image-editor/* sources which can return
// user-generated content from outside the chromium codebase. The
// chrome-untrusted://image-editor/* sources can only be embedded in
// the chrome://image-editor by using an <iframe>.
//
class EditorUntrustedSource : public content::URLDataSource {
 public:
  explicit EditorUntrustedSource(Profile* profile);
  ~EditorUntrustedSource() override;
  EditorUntrustedSource(const EditorUntrustedSource&) = delete;
  EditorUntrustedSource& operator=(const EditorUntrustedSource&) = delete;

  // content::URLDataSource:
  std::string GetContentSecurityPolicy(
      network::mojom::CSPDirectiveName directive) override;
  std::string GetSource() override;
  void StartDataRequest(
      const GURL& url,
      const content::WebContents::Getter& wc_getter,
      content::URLDataSource::GotDataCallback callback) override;
  std::string GetMimeType(const std::string& path) override;
  bool AllowCaching() override;
  bool ShouldReplaceExistingSource() override;
  bool ShouldServeMimeTypeAsContentTypeHeader() override;
  bool ShouldServiceRequest(const GURL& url,
                            content::BrowserContext* browser_context,
                            int render_process_id) override;
  bool ShouldDenyXFrameOptions() override;
};

#endif  // CHROME_BROWSER_UI_WEBUI_IMAGE_EDITOR_EDITOR_UNTRUSTED_SOURCE_H_
