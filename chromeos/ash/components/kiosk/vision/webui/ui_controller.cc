// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chromeos/ash/components/kiosk/vision/webui/ui_controller.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "chromeos/ash/components/grit/kiosk_vision_internals_resources.h"
#include "chromeos/ash/components/grit/kiosk_vision_internals_resources_map.h"
#include "chromeos/ash/components/kiosk/vision/internals_page_processor.h"
#include "chromeos/ash/components/kiosk/vision/webui/constants.h"
#include "chromeos/ash/components/kiosk/vision/webui/kiosk_vision_internals.mojom.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "ui/webui/mojo_web_ui_controller.h"

namespace ash::kiosk_vision {

namespace {

mojom::StatePtr ToState(mojom::Status status) {
  return mojom::State::New(status, std::vector<mojom::LabelPtr>{},
                           std::vector<mojom::BoxPtr>{},
                           std::vector<mojom::FacePtr>{});
}

}  // namespace

UIController::UIController(
    content::WebUI* web_ui,
    SetupWebUIDataSourceCallback setup_callback,
    GetInternalsPageProcessorCallback get_processor_callback)
    : ui::MojoWebUIController(web_ui, /*enable_chrome_send=*/true),
      get_processor_callback_(std::move(get_processor_callback)) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      web_ui->GetWebContents()->GetBrowserContext(),
      std::string(kChromeUIKioskVisionInternalsHost));

  setup_callback.Run(source,
                     base::make_span(kKioskVisionInternalsResources,
                                     kKioskVisionInternalsResourcesSize),
                     IDR_KIOSK_VISION_INTERNALS_KIOSK_VISION_INTERNALS_HTML);
}

UIController::~UIController() = default;

WEB_UI_CONTROLLER_TYPE_IMPL(UIController)

void UIController::BindInterface(
    mojo::PendingReceiver<mojom::PageConnector> receiver) {
  receiver_.reset();
  receiver_.Bind(std::move(receiver));
}

void UIController::BindPage(mojo::PendingRemote<mojom::Page> page_remote) {
  page_.reset();
  page_.Bind(std::move(page_remote));
  page_.set_disconnect_handler(
      base::BindOnce([](UIController* self) { self->observation_.Reset(); },
                     // Safe because `this` owns `page_`.
                     base::Unretained(this)));
  observation_.Reset();

  if (!IsInternalsPageEnabled()) {
    page_->Display(ToState(mojom::Status::kFeatureDisabled));
  } else if (InternalsPageProcessor* processor = get_processor_callback_.Run();
             processor == nullptr) {
    page_->Display(ToState(mojom::Status::kFeatureNotInitialized));
  } else {
    observation_.Observe(processor);
  }
}

void UIController::OnStateChange(const mojom::State& new_state) {
  if (page_.is_bound()) {
    page_->Display(new_state.Clone());
  }
}

UIConfig::UIConfig(SetupWebUIDataSourceCallback setup_callback,
                   GetInternalsPageProcessorCallback get_processor_callback)
    : WebUIConfig(content::kChromeUIScheme, kChromeUIKioskVisionInternalsHost),
      setup_callback_(std::move(setup_callback)),
      get_processor_callback_(std::move(get_processor_callback)) {}

UIConfig::~UIConfig() = default;

bool UIConfig::IsWebUIEnabled(content::BrowserContext* browser_context) {
  return IsInternalsPageEnabled();
}

std::unique_ptr<content::WebUIController> UIConfig::CreateWebUIController(
    content::WebUI* web_ui,
    const GURL& url) {
  return std::make_unique<UIController>(web_ui, setup_callback_,
                                        get_processor_callback_);
}

}  // namespace ash::kiosk_vision
