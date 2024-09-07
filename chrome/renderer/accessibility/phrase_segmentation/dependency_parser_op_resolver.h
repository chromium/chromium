// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_ACCESSIBILITY_PHRASE_SEGMENTATION_DEPENDENCY_PARSER_OP_RESOLVER_H_
#define CHROME_RENDERER_ACCESSIBILITY_PHRASE_SEGMENTATION_DEPENDENCY_PARSER_OP_RESOLVER_H_

#include "third_party/tflite/src/tensorflow/lite/mutable_op_resolver.h"

class DependencyParserOpResolver : public tflite::MutableOpResolver {
 public:
  DependencyParserOpResolver();
  ~DependencyParserOpResolver() override;

  DependencyParserOpResolver(const DependencyParserOpResolver&) = delete;
  DependencyParserOpResolver& operator=(const DependencyParserOpResolver&) =
      delete;
};

#endif  // CHROME_RENDERER_ACCESSIBILITY_PHRASE_SEGMENTATION_DEPENDENCY_PARSER_OP_RESOLVER_H_
