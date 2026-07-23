// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NOTEBOOKS_INTERNALS_WEBUI_NOTEBOOKS_INTERNALS_UI_H_
#define COMPONENTS_NOTEBOOKS_INTERNALS_WEBUI_NOTEBOOKS_INTERNALS_UI_H_

#include "components/notebooks/public/notebooks_constants.h"
#include "content/public/browser/internal_webui_config.h"
#include "ui/webui/mojo_web_ui_controller.h"

namespace content {
class WebUI;
}  // namespace content

namespace notebooks {

// The WebUIController for chrome://notebooks-internals.
// TODO(b/478016257): Remove this WebUI once Notebooks is launched.
class NotebooksInternalsUI : public ui::MojoWebUIController {
 public:
  explicit NotebooksInternalsUI(content::WebUI* web_ui);
  NotebooksInternalsUI(const NotebooksInternalsUI&) = delete;
  NotebooksInternalsUI& operator=(const NotebooksInternalsUI&) = delete;
  ~NotebooksInternalsUI() override;

 private:
  WEB_UI_CONTROLLER_TYPE_DECL();
};

class NotebooksInternalsUIConfig
    : public content::DefaultInternalWebUIConfig<NotebooksInternalsUI> {
 public:
  NotebooksInternalsUIConfig()
      : DefaultInternalWebUIConfig(kChromeUINotebooksInternalsHost) {}
};

}  // namespace notebooks

#endif  // COMPONENTS_NOTEBOOKS_INTERNALS_WEBUI_NOTEBOOKS_INTERNALS_UI_H_
