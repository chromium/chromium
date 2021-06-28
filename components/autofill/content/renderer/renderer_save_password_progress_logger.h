// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CONTENT_RENDERER_RENDERER_SAVE_PASSWORD_PROGRESS_LOGGER_H_
#define COMPONENTS_AUTOFILL_CONTENT_RENDERER_RENDERER_SAVE_PASSWORD_PROGRESS_LOGGER_H_

#include <string>

#include "base/macros.h"
#include "components/autofill/content/common/mojom/autofill_driver.mojom.h"
#include "components/autofill/core/common/save_password_progress_logger.h"

namespace blink {
class WebFormControlElement;
}

namespace autofill {

// This is the SavePasswordProgressLogger specialization for the renderer code,
// which sends logs to the browser process over IPC.
class RendererSavePasswordProgressLogger : public SavePasswordProgressLogger {
 public:
  // The logger will use |password_manager_driver| to send logs to the browser.
  // The |password_manager_driver| needs to outlive the constructed logger.
  RendererSavePasswordProgressLogger(
      mojom::PasswordManagerDriver* password_manager_driver);
  ~RendererSavePasswordProgressLogger() override;

  void LogElementName(StringID label,
                      const blink::WebFormControlElement& element);

 protected:
  // SavePasswordProgressLogger:
  void SendLog(const std::string& log) override;

 private:
  // Used by SendLog to send the logs to the browser.
  // |password_manager_driver_| needs to outlive the logger.
  mojom::PasswordManagerDriver* password_manager_driver_;

  DISALLOW_COPY_AND_ASSIGN(RendererSavePasswordProgressLogger);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CONTENT_RENDERER_RENDERER_SAVE_PASSWORD_PROGRESS_LOGGER_H_
