// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/feature_list.h"
#include "chrome/browser/screen_ai/screen_ai_install_state.h"
#include "chrome/browser/ui/webui/ash/settings/pages/a11y/pdf_ocr_handler.h"
#include "chrome/grit/generated_resources.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

constexpr char kPdfOcrStateChangedEventName[] = "pdf-ocr-state-changed";
constexpr char kPdfOcrDownloadingProgressChangedEventName[] =
    "pdf-ocr-downloading-progress-changed";

}  // namespace

namespace settings {

PdfOcrHandler::PdfOcrHandler() {
  DCHECK(base::FeatureList::IsEnabled(features::kPdfOcr));
}

PdfOcrHandler::~PdfOcrHandler() {
  screen_ai::ScreenAIInstallState::GetInstance()->RemoveObserver(this);
}

void PdfOcrHandler::RegisterMessages() {
  VLOG(2) << "Registering a UI handler for the PDF OCR toggle on Settings";
  web_ui()->RegisterMessageCallback(
      "pdfOcrSectionReady",
      base::BindRepeating(&PdfOcrHandler::HandlePdfOcrSectionReady,
                          base::Unretained(this)));
}

void PdfOcrHandler::OnJavascriptAllowed() {
  screen_ai::ScreenAIInstallState::GetInstance()->AddObserver(this);
}

void PdfOcrHandler::OnJavascriptDisallowed() {
  screen_ai::ScreenAIInstallState::GetInstance()->RemoveObserver(this);
}

void PdfOcrHandler::HandlePdfOcrSectionReady(const base::Value::List& args) {
  AllowJavascript();
}

void PdfOcrHandler::DownloadProgressChanged(double progress) {
  const int progress_num = progress * 100;
  VLOG(2) << "Downloading progress: " << progress_num << "%";
  FireWebUIListener(kPdfOcrDownloadingProgressChangedEventName,
                    base::Value(progress_num));
}

void PdfOcrHandler::StateChanged(screen_ai::ScreenAIInstallState::State state) {
  base::Value state_value = base::Value(static_cast<int>(state));
  FireWebUIListener(kPdfOcrStateChangedEventName, state_value);
}

}  // namespace settings
