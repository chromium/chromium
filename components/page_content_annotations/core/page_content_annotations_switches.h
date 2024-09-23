// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CORE_PAGE_CONTENT_ANNOTATIONS_SWITCHES_H_
#define COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CORE_PAGE_CONTENT_ANNOTATIONS_SWITCHES_H_

#include <optional>

#include "base/component_export.h"
#include "base/files/file_path.h"
#include "base/time/time.h"

namespace page_content_annotations::switches {

COMPONENT_EXPORT(PAGE_CONTENT_ANNOTATIONS_FEATURES)
extern const char kPageContentAnnotationsLoggingEnabled[];
COMPONENT_EXPORT(PAGE_CONTENT_ANNOTATIONS_FEATURES)
extern const char kPageContentAnnotationsValidationStartupDelaySeconds[];
COMPONENT_EXPORT(PAGE_CONTENT_ANNOTATIONS_FEATURES)
extern const char kPageContentAnnotationsValidationBatchSizeOverride[];
COMPONENT_EXPORT(PAGE_CONTENT_ANNOTATIONS_FEATURES)
extern const char kPageContentAnnotationsValidationPageEntities[];
COMPONENT_EXPORT(PAGE_CONTENT_ANNOTATIONS_FEATURES)
extern const char kPageContentAnnotationsValidationContentVisibility[];
COMPONENT_EXPORT(PAGE_CONTENT_ANNOTATIONS_FEATURES)
extern const char kPageContentAnnotationsValidationTextEmbedding[];
COMPONENT_EXPORT(PAGE_CONTENT_ANNOTATIONS_FEATURES)
extern const char kPageContentAnnotationsValidationWriteToFile[];

// Returns true if page content annotations input should be logged.
COMPONENT_EXPORT(PAGE_CONTENT_ANNOTATIONS_FEATURES)
bool ShouldLogPageContentAnnotationsInput();

// Returns the delay to use for page content annotations validation, if given
// and valid on the command line.
COMPONENT_EXPORT(PAGE_CONTENT_ANNOTATIONS_FEATURES)
std::optional<base::TimeDelta> PageContentAnnotationsValidationStartupDelay();

// Returns the size of the batch to use for page content annotations validation,
// if given and valid on the command line.
COMPONENT_EXPORT(PAGE_CONTENT_ANNOTATIONS_FEATURES)
std::optional<size_t> PageContentAnnotationsValidationBatchSize();

// Whether the result of page content annotations validation should be sent to
// the console. True when any one of the corresponding command line flags is
// enabled.
COMPONENT_EXPORT(PAGE_CONTENT_ANNOTATIONS_FEATURES)
bool LogPageContentAnnotationsValidationToConsole();

// Returns the file path to write page content annotation validation results to.
COMPONENT_EXPORT(PAGE_CONTENT_ANNOTATIONS_FEATURES)
std::optional<base::FilePath> PageContentAnnotationsValidationWriteToFile();

}  // namespace page_content_annotations::switches

#endif  // COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CORE_PAGE_CONTENT_ANNOTATIONS_SWITCHES_H_
