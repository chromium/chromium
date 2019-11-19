// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_BROWSER_SAVE_PASSWORD_PROGRESS_LOGGER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_BROWSER_SAVE_PASSWORD_PROGRESS_LOGGER_H_

#include <string>

#include "base/macros.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom.h"
#include "components/autofill/core/common/password_form.h"
#include "components/autofill/core/common/save_password_progress_logger.h"
#include "url/gurl.h"

namespace autofill {
struct FormData;
class FormStructure;
class LogManager;
}

namespace password_manager {

// This is the SavePasswordProgressLogger specialization for the browser code,
// where the LogManager can be directly called.
class BrowserSavePasswordProgressLogger
    : public autofill::SavePasswordProgressLogger {
 public:
  explicit BrowserSavePasswordProgressLogger(
      const autofill::LogManager* log_manager);
  ~BrowserSavePasswordProgressLogger() override;

  // Browser-specific addition to the base class' Log* methods. The input is
  // sanitized and passed to SendLog for display.
  void LogFormStructure(StringID label, const autofill::FormStructure& form);

  // Browser-specific addition to the base class' Log* methods. The input is
  // sanitized and passed to SendLog for display.
  void LogSuccessiveOrigins(StringID label,
                            const GURL& old_origin,
                            const GURL& new_origin);

  // Browser-specific addition to the base class' Log* methods. The input is
  // passed to SendLog for display.
  void LogString(StringID label, const std::string& s);

  // Log a password successful submission event.
  void LogSuccessfulSubmissionIndicatorEvent(
      autofill::mojom::SubmissionIndicatorEvent event);

  // Browser-specific addition to the base class' Log* methods. The input is
  // sanitized and passed to SendLog for display.
  void LogFormData(StringID label, const autofill::FormData& form);

 protected:
  // autofill::SavePasswordProgressLogger:
  void SendLog(const std::string& log) override;

 private:
  // The LogManager to which logs can be sent for display. The log_manager must
  // outlive this logger.
  const autofill::LogManager* const log_manager_;

  // Returns string representation for |FormStructure|.
  std::string FormStructureToFieldsLogString(
      const autofill::FormStructure& form);

  // Returns string representation of password attributes for |FormStructure|.
  std::string FormStructurePasswordAttributesLogString(
      const autofill::FormStructure& form);

  // Returns the string representation of a password attribute.
  std::string PasswordAttributeLogString(StringID string_id,
                                         const std::string& attribute_value);

  // Returns the string representation of a binary password attribute.
  std::string BinaryPasswordAttributeLogString(StringID string_id,
                                               bool attribute_value);

  DISALLOW_COPY_AND_ASSIGN(BrowserSavePasswordProgressLogger);
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_BROWSER_SAVE_PASSWORD_PROGRESS_LOGGER_H_
