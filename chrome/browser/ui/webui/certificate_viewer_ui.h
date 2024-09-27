// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CERTIFICATE_VIEWER_UI_H_
#define CHROME_BROWSER_UI_WEBUI_CERTIFICATE_VIEWER_UI_H_

#include "chrome/browser/ui/webui/constrained_web_dialog_ui.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"

class CertificateViewerUI;

class CertificateViewerUIConfig
    : public content::DefaultWebUIConfig<CertificateViewerUI> {
 public:
  CertificateViewerUIConfig()
      : DefaultWebUIConfig(content::kChromeUIScheme,
                           chrome::kChromeUICertificateViewerHost) {}
};

// The WebUI for chrome://view-cert
class CertificateViewerUI : public ConstrainedWebDialogUI {
 public:
  explicit CertificateViewerUI(content::WebUI* web_ui);

  CertificateViewerUI(const CertificateViewerUI&) = delete;
  CertificateViewerUI& operator=(const CertificateViewerUI&) = delete;

  ~CertificateViewerUI() override;
};

#endif  // CHROME_BROWSER_UI_WEBUI_CERTIFICATE_VIEWER_UI_H_
