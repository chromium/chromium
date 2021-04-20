// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/machine_learning/machine_learning_internals_ui.h"

#include <memory>
#include <utility>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/chromeos/machine_learning/machine_learning_internals_page_handler.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/dev_ui_browser_resources.h"
#include "content/public/browser/web_ui_data_source.h"

namespace chromeos {
namespace machine_learning {

MachineLearningInternalsUI::MachineLearningInternalsUI(
    content::WebUI* const web_ui)
    : ui::MojoWebUIController(web_ui) {
  content::WebUIDataSource* const source = content::WebUIDataSource::Create(
      chrome::kChromeUIMachineLearningInternalsHost);

  const std::map<int, std::string> resource_paths = {
      {IDR_MACHINE_LEARNING_INTERNALS_GRAPH_EXECUTOR_MOJO_JS,
       "chromeos/services/machine_learning/public/mojom/"
       "graph_executor.mojom-lite.js"},

      {IDR_MACHINE_LEARNING_INTERNALS_HANDWRITING_RECOGNIZER_MOJO_JS,
       "chromeos/services/machine_learning/public/mojom/"
       "handwriting_recognizer.mojom-lite.js"},

      {IDR_MACHINE_LEARNING_INTERNALS_JS, "machine_learning_internals.js"},

      {IDR_MACHINE_LEARNING_INTERNALS_MACHINE_LEARNING_SERVICE_MOJO_JS,
       "chromeos/services/machine_learning/public/mojom/"
       "machine_learning_service.mojom-lite.js"},

      {IDR_MACHINE_LEARNING_INTERNALS_MODEL_MOJO_JS,
       "chromeos/services/machine_learning/public/mojom/model.mojom-lite.js"},

      {IDR_MACHINE_LEARNING_INTERNALS_PAGE_HANDLER_MOJO_JS,
       "chrome/browser/ui/webui/chromeos/machine_learning/"
       "machine_learning_internals_page_handler.mojom-lite.js"},
      {IDR_MACHINE_LEARNING_INTERNALS_SODA_MOJO_JS,
       "chromeos/services/machine_learning/public/mojom/"
       "soda.mojom-lite.js"},

      {IDR_MACHINE_LEARNING_INTERNALS_TENSOR_MOJO_JS,
       "chromeos/services/machine_learning/public/mojom/tensor.mojom-lite.js"},

      {IDR_MACHINE_LEARNING_INTERNALS_TEST_MODEL_TAB_JS, "test_model_tab.js"},

      {IDR_MACHINE_LEARNING_INTERNALS_TIME_MOJO_JS,
       "mojo/public/mojom/base/time.mojom-lite.js"},

      {IDR_MACHINE_LEARNING_INTERNALS_UTILS_JS,
       "machine_learning_internals_utils.js"},

      {IDR_MACHINE_LEARNING_INTERNALS_WEB_PLATFORM_HANDWRITING_MOJO_JS,
       "chromeos/services/machine_learning/public/mojom/"
       "web_platform_handwriting.mojom-lite.js"},

      {IDR_UI_GEOMETRY_MOJOM_LITE_JS,
       "ui/gfx/geometry/mojom/geometry.mojom-lite.js"},
  };

  for (const auto& path : resource_paths) {
    source->AddResourcePath(path.second, path.first);
  }

  source->SetDefaultResource(IDR_MACHINE_LEARNING_INTERNALS_HTML);
  content::WebUIDataSource::Add(Profile::FromWebUI(web_ui), source);
}

WEB_UI_CONTROLLER_TYPE_IMPL(MachineLearningInternalsUI)

MachineLearningInternalsUI::~MachineLearningInternalsUI() = default;

void MachineLearningInternalsUI::BindInterface(
    mojo::PendingReceiver<mojom::PageHandler> receiver) {
  page_handler_ = std::make_unique<MachineLearningInternalsPageHandler>(
      std::move(receiver));
}

}  // namespace machine_learning
}  // namespace chromeos
