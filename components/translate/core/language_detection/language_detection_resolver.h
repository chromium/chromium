// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRANSLATE_CORE_LANGUAGE_DETECTION_LANGUAGE_DETECTION_RESOLVER_H_
#define COMPONENTS_TRANSLATE_CORE_LANGUAGE_DETECTION_LANGUAGE_DETECTION_RESOLVER_H_

#include <memory>

#include "components/translate/core/language_detection/embedding_lookup.h"
#include "components/translate/core/language_detection/ngram_hash.h"
#include "third_party/tflite/src/tensorflow/lite/kernels/builtin_op_kernels.h"
#include "third_party/tflite/src/tensorflow/lite/op_resolver.h"

namespace translate {

std::unique_ptr<tflite::MutableOpResolver> CreateLangIdResolver() {
  tflite::MutableOpResolver resolver;
  // The minimal set of OPs required to run the language detection model.
  resolver.AddBuiltin(tflite::BuiltinOperator_CONCATENATION,
                      tflite::ops::builtin::Register_CONCATENATION());
  resolver.AddBuiltin(tflite::BuiltinOperator_FULLY_CONNECTED,
                      tflite::ops::builtin::Register_FULLY_CONNECTED(),
                      /*min_version=*/1, /*max_version=*/9);
  resolver.AddBuiltin(tflite::BuiltinOperator_RESHAPE,
                      tflite::ops::builtin::Register_RESHAPE());
  resolver.AddBuiltin(tflite::BuiltinOperator_SOFTMAX,
                      tflite::ops::builtin::Register_SOFTMAX());
  resolver.AddBuiltin(::tflite::BuiltinOperator_UNPACK,
                      tflite::ops::builtin::Register_UNPACK());
  resolver.AddCustom("NGramHash", Register_NGRAM_HASH());
  resolver.AddCustom("EmbeddingLookup", Register_EMBEDDING_LOOKUP());
  return std::make_unique<tflite::MutableOpResolver>(resolver);
}

}  // namespace translate

#endif  // COMPONENTS_TRANSLATE_CORE_LANGUAGE_DETECTION_LANGUAGE_DETECTION_RESOLVER_H_