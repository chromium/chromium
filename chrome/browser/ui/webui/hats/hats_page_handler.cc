// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/hats/hats_page_handler.h"

#include <string>

#include "base/json/json_writer.h"
#include "google_apis/google_api_keys.h"

HatsPageHandler::HatsPageHandler(
    mojo::PendingReceiver<hats::mojom::PageHandler> receiver,
    mojo::PendingRemote<hats::mojom::Page> page,
    HatsPageHandlerDelegate* delegate)
    : receiver_(this, std::move(receiver)),
      page_(std::move(page)),
      delegate_(delegate) {
  CHECK(delegate_ != nullptr);
  std::string product_specific_data_json;
  base::JSONWriter::Write(delegate_->GetProductSpecificDataJson(),
                          &product_specific_data_json);
  page_->RequestSurvey(google_apis::GetHatsAPIKey(), delegate_->GetTriggerId(),
                       delegate_->GetEnableTesting(),
                       delegate_->GetLanguageList(),
                       product_specific_data_json);
}

// Triggered by onSurveyLoaded() call in TS.
void HatsPageHandler::OnSurveyLoaded() {
  delegate_->OnSurveyLoaded();
}
// Triggered by onSurveyCompleted() call in TS.
void HatsPageHandler::OnSurveyCompleted() {
  delegate_->OnSurveyCompleted();
}

// Triggered by onSurveyClosed() call in TS.
void HatsPageHandler::OnSurveyClosed() {
  delegate_->OnSurveyClosed();
}

HatsPageHandler::~HatsPageHandler() = default;
