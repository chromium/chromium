// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_DEFAULT_BROWSER_VISUAL_GUIDED_SETTER_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_DEFAULT_BROWSER_VISUAL_GUIDED_SETTER_PAGE_HANDLER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/webui/default_browser/visual_guided_setter.mojom.h"
#include "chrome/browser/ui/webui/default_browser/visual_guided_setter_controller_win.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/gfx/geometry/rect.h"

namespace content {
class WebContents;
}  // namespace content

// Browser-process endpoint for the
// chrome://default-browser-visual-guided-setter WebUI. Implements the Mojo
// PageHandler.
class VisualGuidedSetterPageHandler
    : public visual_guided_setter::mojom::PageHandler {
 public:
  VisualGuidedSetterPageHandler(
      mojo::PendingReceiver<visual_guided_setter::mojom::PageHandler> receiver,
      mojo::PendingRemote<visual_guided_setter::mojom::Page> page,
      content::WebContents* web_contents);
  VisualGuidedSetterPageHandler(const VisualGuidedSetterPageHandler&) = delete;
  VisualGuidedSetterPageHandler& operator=(
      const VisualGuidedSetterPageHandler&) = delete;
  ~VisualGuidedSetterPageHandler() override;

  // visual_guided_setter::mojom::PageHandler:
  void SetAnchorRect(const gfx::Rect& rect) override;
  void OpenSettings() override;

 private:
  void OnOpenSettingsResult(bool succeeded);
  void OnErrorStateChanged(bool has_error);

  raw_ptr<content::WebContents> web_contents_;

  std::unique_ptr<VisualGuidedSetterControllerWin> controller_;

  mojo::Receiver<visual_guided_setter::mojom::PageHandler> receiver_;
  mojo::Remote<visual_guided_setter::mojom::Page> page_;

  base::WeakPtrFactory<VisualGuidedSetterPageHandler> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_DEFAULT_BROWSER_VISUAL_GUIDED_SETTER_PAGE_HANDLER_H_
