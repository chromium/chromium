// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_DISCARDS_DISCARDS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_DISCARDS_DISCARDS_UI_H_

#include <memory>

#include "base/macros.h"
#include "chrome/browser/ui/webui/discards/discards.mojom.h"
#include "services/resource_coordinator/public/mojom/webui_graph_dump.mojom.h"
#include "ui/webui/mojo_web_ui_controller.h"

namespace resource_coordinator {
class LocalSiteCharacteristicsDataStoreInspector;
}  // namespace resource_coordinator

// Controller for chrome://discards. Corresponding resources are in
// file://chrome/browser/resources/discards.
class DiscardsUI : public ui::MojoWebUIController {
 public:
  explicit DiscardsUI(content::WebUI* web_ui);
  ~DiscardsUI() override;

 private:
  void BindDiscardsDetailsProvider(
      mojom::DiscardsDetailsProviderRequest request);
  void BindWebUIGraphDumpProvider(
      resource_coordinator::mojom::WebUIGraphDumpRequest request);

  std::unique_ptr<mojom::DiscardsDetailsProvider> ui_handler_;
  resource_coordinator::LocalSiteCharacteristicsDataStoreInspector*
      data_store_inspector_;

  DISALLOW_COPY_AND_ASSIGN(DiscardsUI);
};

#endif  // CHROME_BROWSER_UI_WEBUI_DISCARDS_DISCARDS_UI_H_
