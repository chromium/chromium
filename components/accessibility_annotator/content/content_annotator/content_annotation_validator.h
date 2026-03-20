// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACCESSIBILITY_ANNOTATOR_CONTENT_CONTENT_ANNOTATOR_CONTENT_ANNOTATION_VALIDATOR_H_
#define COMPONENTS_ACCESSIBILITY_ANNOTATOR_CONTENT_CONTENT_ANNOTATOR_CONTENT_ANNOTATION_VALIDATOR_H_

#include <memory>
#include <string>

#include "base/values.h"

namespace accessibility_annotator {

// Validates and sanitizes data extracted by the model.
class ContentAnnotationValidator {
 public:
  // Returns a nullptr if the schema is malformed JSON.
  static std::unique_ptr<ContentAnnotationValidator> Create();

  explicit ContentAnnotationValidator(base::DictValue schema);
  virtual ~ContentAnnotationValidator();

  ContentAnnotationValidator(const ContentAnnotationValidator&) = delete;
  ContentAnnotationValidator& operator=(const ContentAnnotationValidator&) =
      delete;

  // Returns true if the validator was created with a non-empty schema.
  virtual bool IsValidatorEnabled() const;

  // Validates the extracted data against the expected schema and sanitizes it.
  // Returns the validated data as a JSON string, if acceptable.
  // Virtual for testing.
  virtual std::optional<std::string> Validate(std::string extracted_data) const;

 private:
  base::DictValue schema_;
};

}  // namespace accessibility_annotator

#endif  // COMPONENTS_ACCESSIBILITY_ANNOTATOR_CONTENT_CONTENT_ANNOTATOR_CONTENT_ANNOTATION_VALIDATOR_H_
