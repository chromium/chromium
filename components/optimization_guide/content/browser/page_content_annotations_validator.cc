// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/content/browser/page_content_annotations_validator.h"

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/rand_util.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/task/thread_pool.h"
#include "base/time/default_tick_clock.h"
#include "components/optimization_guide/content/browser/page_content_annotator.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_switches.h"
#include "components/optimization_guide/core/page_content_annotations_common.h"

namespace optimization_guide {

namespace {

const char* kRandomNouns[] = {
    "Airplane", "Boat",       "Book",          "Dinosaur",   "Earth",
    "Football", "Fork",       "Hummingbird",   "Magic Wand", "Mailbox",
    "Molecule", "Pizza",      "Record Player", "Skeleton",   "Soda",
    "Sphere",   "Strawberry", "Tiger",         "Turkey",     "Wolf",
};
const size_t kCountRandomNouns = 20;

void LogLinesToFileOnBackgroundSequence(const base::FilePath& path,
                                        const std::vector<std::string>& lines) {
  std::string data = base::JoinString(lines, "\n") + "\n";
  if (base::PathExists(path)) {
    base::AppendToFile(path, data);
    return;
  }
  base::WriteFile(path, data);
}

void MaybeLogAnnotationResultToFile(
    const std::vector<BatchAnnotationResult>& results) {
  absl::optional<base::FilePath> path =
      switches::PageContentAnnotationsValidationWriteToFile();
  if (!path) {
    return;
  }

  std::vector<std::string> lines;
  for (const BatchAnnotationResult& result : results) {
    lines.push_back(result.ToJSON());
  }

  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&LogLinesToFileOnBackgroundSequence, *path, lines));
}

}  // namespace

PageContentAnnotationsValidator::~PageContentAnnotationsValidator() = default;
PageContentAnnotationsValidator::PageContentAnnotationsValidator(
    PageContentAnnotator* annotator)
    : annotator_(annotator) {
  DCHECK(annotator);
  for (AnnotationType type : {
           AnnotationType::kPageEntities,
           AnnotationType::kContentVisibility,
       }) {
    if (features::PageContentAnnotationValidationEnabledForType(type)) {
      enabled_annotation_types_.push_back(type);
      annotator_->RequestAndNotifyWhenModelAvailable(type, base::DoNothing());
    }
  }

  timer_.Start(FROM_HERE,
               features::PageContentAnnotationValidationStartupDelay(),
               base::BindOnce(&PageContentAnnotationsValidator::Run,
                              weak_ptr_factory_.GetWeakPtr()));
}

// static
std::unique_ptr<PageContentAnnotationsValidator>
PageContentAnnotationsValidator::MaybeCreateAndStartTimer(
    PageContentAnnotator* annotator) {
  // This can happen with certain build/feature flags.
  if (!annotator) {
    return nullptr;
  }

  bool enabled_for_any_type = false;
  for (AnnotationType type : {
           AnnotationType::kPageEntities,
           AnnotationType::kContentVisibility,
       }) {
    enabled_for_any_type |=
        features::PageContentAnnotationValidationEnabledForType(type);
  }
  if (!enabled_for_any_type) {
    return nullptr;
  }

  // This is done because |PageContentAnnotationsValidator| has a private ctor.
  return base::WrapUnique(new PageContentAnnotationsValidator(annotator));
}

void PageContentAnnotationsValidator::Run() {
  for (AnnotationType type : enabled_annotation_types_) {
    annotator_->Annotate(base::BindOnce(&MaybeLogAnnotationResultToFile),
                         BuildInputsForType(type), type);
  }
}

// static
std::vector<std::string> PageContentAnnotationsValidator::BuildInputsForType(
    AnnotationType type) {
  absl::optional<std::vector<std::string>> cmd_line_input =
      switches::PageContentAnnotationsValidationInputForType(type);
  if (cmd_line_input) {
    return *cmd_line_input;
  }

  std::vector<std::string> inputs;
  for (size_t i = 0; i < features::PageContentAnnotationsValidationBatchSize();
       i++) {
    const char* word1 = kRandomNouns[base::RandGenerator(kCountRandomNouns)];
    const char* word2 = kRandomNouns[base::RandGenerator(kCountRandomNouns)];
    inputs.emplace_back(base::StrCat({word1, " ", word2}));
  }
  return inputs;
}

}  // namespace optimization_guide
