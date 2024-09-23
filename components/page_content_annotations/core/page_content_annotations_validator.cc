// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/page_content_annotations/core/page_content_annotations_validator.h"

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/rand_util.h"
#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/task/thread_pool.h"
#include "base/time/default_tick_clock.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_switches.h"
#include "components/page_content_annotations/core/page_content_annotations_common.h"
#include "components/page_content_annotations/core/page_content_annotations_features.h"
#include "components/page_content_annotations/core/page_content_annotations_switches.h"
#include "components/page_content_annotations/core/page_content_annotator.h"

namespace page_content_annotations {

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
  std::optional<base::FilePath> path =
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


// Whether the page content annotation validation feature or command line flag
// is enabled for the given annotation type.
bool PageContentAnnotationValidationEnabledForType(AnnotationType type) {
  if (base::FeatureList::IsEnabled(features::kPageContentAnnotationsValidation)) {
    if (GetFieldTrialParamByFeatureAsBool(features::kPageContentAnnotationsValidation,
                                          AnnotationTypeToString(type),
                                          false)) {
      return true;
    }
  }

  base::CommandLine* cmd = base::CommandLine::ForCurrentProcess();
  switch (type) {
    case AnnotationType::kContentVisibility:
      return cmd->HasSwitch(
          switches::kPageContentAnnotationsValidationContentVisibility);
    default:
      NOTREACHED_IN_MIGRATION();
      break;
  }

  return false;
}

// Returns a set on inputs to run the validation on for the given |type|,
// using comma separated input from the command line.
std::optional<std::vector<std::string>>
PageContentAnnotationsValidationInputForType(AnnotationType type) {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();

  std::string value;
  switch (type) {
    case AnnotationType::kContentVisibility:
      value = command_line->GetSwitchValueASCII(
          switches::kPageContentAnnotationsValidationContentVisibility);
      break;
    default:
      break;
  }
  if (value.empty()) {
    return std::nullopt;
  }

  return base::SplitString(value, ",", base::KEEP_WHITESPACE,
                           base::SPLIT_WANT_ALL);
}

}  // namespace

PageContentAnnotationsValidator::~PageContentAnnotationsValidator() = default;
PageContentAnnotationsValidator::PageContentAnnotationsValidator(
    PageContentAnnotator* annotator)
    : annotator_(annotator) {
  DCHECK(annotator);
  for (AnnotationType type : {
           AnnotationType::kContentVisibility,
       }) {
    if (PageContentAnnotationValidationEnabledForType(type)) {
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
           AnnotationType::kContentVisibility,
       }) {
    enabled_for_any_type |=
        PageContentAnnotationValidationEnabledForType(type);
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
  std::optional<std::vector<std::string>> cmd_line_input =
      PageContentAnnotationsValidationInputForType(type);
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

}  // namespace page_content_annotations
