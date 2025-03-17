// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_SPECIALIZED_FEATURES_FEEDBACK_H_
#define CHROMEOS_ASH_COMPONENTS_SPECIALIZED_FEATURES_FEEDBACK_H_

#include <optional>
#include <string>

#include "base/component_export.h"

namespace feedback {
class FeedbackUploader;
}

namespace specialized_features {

// Uploads feedback about a specialized feature after redacting the given
// description.
// WARNING: The start and end of `description` may not be redacted correctly due
// to limitations in `feedback::RedactionTool`. To work around this, prepend
// exactly two spaces (for credit cards WITHOUT triggering the three space hash
// exception) and append a new line (for credit cards).
// TODO: b/367882164 - Fix this in `feedback::RedactionTool`, or work around it.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_SPECIALIZED_FEATURES)
void SendFeedback(feedback::FeedbackUploader& uploader,
                  int product_id,
                  std::string description,
                  std::optional<std::string> image = std::nullopt,
                  std::optional<std::string> image_mime_type = std::nullopt);

}  // namespace specialized_features

#endif  // CHROMEOS_ASH_COMPONENTS_SPECIALIZED_FEATURES_FEEDBACK_H_
