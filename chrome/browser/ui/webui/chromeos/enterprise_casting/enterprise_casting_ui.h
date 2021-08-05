// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_ENTERPRISE_CASTING_ENTERPRISE_CASTING_UI_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_ENTERPRISE_CASTING_ENTERPRISE_CASTING_UI_H_

#include "chrome/browser/ui/webui/chromeos/enterprise_casting/enterprise_casting.mojom.h"
#include "chrome/browser/ui/webui/chromeos/enterprise_casting/enterprise_casting_handler.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/webui/mojo_web_ui_controller.h"

namespace chromeos {

// The WebUI controller for chrome://enterprise-casting.
class EnterpriseCastingUI
    : public ui::MojoWebUIController,
      public enterprise_casting::mojom::PageHandlerFactory {
 public:
  explicit EnterpriseCastingUI(content::WebUI* web_ui);
  ~EnterpriseCastingUI() override;

  EnterpriseCastingUI(const EnterpriseCastingUI&) = delete;
  EnterpriseCastingUI& operator=(const EnterpriseCastingUI&) = delete;

  void BindInterface(
      mojo::PendingReceiver<enterprise_casting::mojom::PageHandlerFactory>
          receiver);

 private:
  // launcher_internals::mojom::PageHandlerFactory:
  void CreatePageHandler(
      mojo::PendingRemote<enterprise_casting::mojom::Page> page,
      mojo::PendingReceiver<enterprise_casting::mojom::PageHandler>
          page_handler) override;

  std::unique_ptr<EnterpriseCastingHandler> page_handler_;
  mojo::Receiver<enterprise_casting::mojom::PageHandlerFactory>
      factory_receiver_{this};

  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_ENTERPRISE_CASTING_ENTERPRISE_CASTING_UI_H_
