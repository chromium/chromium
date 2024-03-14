// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_content_annotations/core/page_content_annotations_switches.h"

#include "base/command_line.h"
#include "base/strings/string_number_conversions.h"

namespace page_content_annotations::switches {

const char kPageContentAnnotationsLoggingEnabled[] =
    "enable-page-content-annotations-logging";

const char kPageContentAnnotationsValidationStartupDelaySeconds[] =
    "page-content-annotations-validation-startup-delay-seconds";

const char kPageContentAnnotationsValidationBatchSizeOverride[] =
    "page-content-annotations-validation-batch-size";

// Enables the specific annotation type to run validation at startup after a
// delay. A comma separated list of inputs can be given as a value which will be
// used as input for the validation job.
const char kPageContentAnnotationsValidationContentVisibility[] =
    "page-content-annotations-validation-content-visibility";

// Writes the output of page content annotation validations to the given file.
const char kPageContentAnnotationsValidationWriteToFile[] =
    "page-content-annotations-validation-write-to-file";

bool ShouldLogPageContentAnnotationsInput() {
  static bool enabled = base::CommandLine::ForCurrentProcess()->HasSwitch(
      kPageContentAnnotationsLoggingEnabled);
  return enabled;
}

std::optional<base::TimeDelta> PageContentAnnotationsValidationStartupDelay() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(
          kPageContentAnnotationsValidationStartupDelaySeconds)) {
    return std::nullopt;
  }

  std::string value = command_line->GetSwitchValueASCII(
      kPageContentAnnotationsValidationStartupDelaySeconds);

  size_t seconds = 0;
  if (base::StringToSizeT(value, &seconds)) {
    return base::Seconds(seconds);
  }
  return std::nullopt;
}

std::optional<size_t> PageContentAnnotationsValidationBatchSize() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(
          kPageContentAnnotationsValidationBatchSizeOverride)) {
    return std::nullopt;
  }

  std::string value = command_line->GetSwitchValueASCII(
      kPageContentAnnotationsValidationBatchSizeOverride);

  size_t size = 0;
  if (base::StringToSizeT(value, &size)) {
    return size;
  }
  return std::nullopt;
}

bool LogPageContentAnnotationsValidationToConsole() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  return command_line->HasSwitch(
      kPageContentAnnotationsValidationContentVisibility);
  ;
}

std::optional<base::FilePath> PageContentAnnotationsValidationWriteToFile() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(kPageContentAnnotationsValidationWriteToFile)) {
    return std::nullopt;
  }
  return command_line->GetSwitchValuePath(
      kPageContentAnnotationsValidationWriteToFile);
}

}  // namespace page_content_annotations::switches
