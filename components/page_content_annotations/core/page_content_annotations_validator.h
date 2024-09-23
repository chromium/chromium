// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CORE_PAGE_CONTENT_ANNOTATIONS_VALIDATOR_H_
#define COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CORE_PAGE_CONTENT_ANNOTATIONS_VALIDATOR_H_

#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "components/page_content_annotations/core/page_content_annotation_type.h"

namespace page_content_annotations {

class PageContentAnnotator;

// This class manages validation runs of the PageContentAnnotationsService,
// running the ML model for a given AnnotationType on dummy data after some
// delay from browser startup. This feature can be controlled by experimental
// feature flags and command line.
class PageContentAnnotationsValidator {
 public:
  ~PageContentAnnotationsValidator();

  // If the appropriate feature flag or command line switch is given, an
  // instance of |this| is created, else nullptr.
  static std::unique_ptr<PageContentAnnotationsValidator>
  MaybeCreateAndStartTimer(PageContentAnnotator* annotator);

 private:
  explicit PageContentAnnotationsValidator(PageContentAnnotator* annotator);

  // Runs the validation for all enabled AnnotationTypes.
  void Run();

  // Creates a set of dummy input data to run for the given |type|, either
  // randomly generated off of experiment parameters or given on the command
  // line.
  static std::vector<std::string> BuildInputsForType(AnnotationType type);

  std::vector<AnnotationType> enabled_annotation_types_;

  // Out lives |this|, not owned.
  raw_ptr<PageContentAnnotator> annotator_;

  // Starts in the ctor, roughly on browser start, and calls |Run|.
  base::OneShotTimer timer_;

  base::WeakPtrFactory<PageContentAnnotationsValidator> weak_ptr_factory_{this};
};

}  // namespace page_content_annotations

#endif  // COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CORE_PAGE_CONTENT_ANNOTATIONS_VALIDATOR_H_
