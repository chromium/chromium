// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SEGMENTATION_INTERNALS_SEGMENTATION_INTERNALS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_SEGMENTATION_INTERNALS_SEGMENTATION_INTERNALS_UI_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/webui/segmentation_internals/segmentation_internals.mojom.h"  // nogncheck
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/webui_config.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "ui/webui/mojo_web_ui_controller.h"

class Profile;
class SegmentationInternalsPageHandlerImpl;
class SegmentationInternalsUI;

class SegmentationInternalsUIConfig
    : public content::DefaultWebUIConfig<SegmentationInternalsUI> {
 public:
  SegmentationInternalsUIConfig()
      : DefaultWebUIConfig(content::kChromeUIScheme,
                           chrome::kChromeUISegmentationInternalsHost) {}
};

// The WebUI controller for chrome://segmentation-internals.
class SegmentationInternalsUI
    : public ui::MojoWebUIController,
      public segmentation_internals::mojom::PageHandlerFactory {
 public:
  explicit SegmentationInternalsUI(content::WebUI* web_ui);
  ~SegmentationInternalsUI() override;

  SegmentationInternalsUI(const SegmentationInternalsUI&) = delete;
  SegmentationInternalsUI& operator=(const SegmentationInternalsUI&) = delete;

  void BindInterface(
      mojo::PendingReceiver<segmentation_internals::mojom::PageHandlerFactory>
          receiver);

 private:
  // segmentation_internals::mojom::PageHandlerFactory impls.
  void CreatePageHandler(
      mojo::PendingRemote<segmentation_internals::mojom::Page> page,
      mojo::PendingReceiver<segmentation_internals::mojom::PageHandler>
          receiver) override;

  raw_ptr<Profile> profile_;
  std::unique_ptr<SegmentationInternalsPageHandlerImpl>
      segmentation_internals_page_handler_;
  mojo::Receiver<segmentation_internals::mojom::PageHandlerFactory>
      segmentation_internals_page_factory_receiver_{this};

  WEB_UI_CONTROLLER_TYPE_DECL();
};

#endif  // CHROME_BROWSER_UI_WEBUI_SEGMENTATION_INTERNALS_SEGMENTATION_INTERNALS_UI_H_
