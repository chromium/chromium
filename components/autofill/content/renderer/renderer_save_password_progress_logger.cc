// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/renderer/renderer_save_password_progress_logger.h"

#include <string>

#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "third_party/blink/public/web/web_form_control_element.h"

namespace autofill {

RendererSavePasswordProgressLogger::RendererSavePasswordProgressLogger(
    mojom::PasswordManagerDriver* password_manager_driver)
    : password_manager_driver_(password_manager_driver) {
  DCHECK(password_manager_driver);
}

RendererSavePasswordProgressLogger::~RendererSavePasswordProgressLogger() {}

void RendererSavePasswordProgressLogger::SendLog(const std::string& log) {
  password_manager_driver_->RecordSavePasswordProgress(log);
}

void RendererSavePasswordProgressLogger::LogElementName(
    StringID label,
    const blink::WebFormControlElement& element) {
  std::string text =
      "name = " + ScrubElementID(element.NameForAutofill().Utf8()) +
      ", renderer_id = " +
      base::NumberToString(element.UniqueRendererFormControlId());
  LogValue(label, base::Value(text));
}

}  // namespace autofill
