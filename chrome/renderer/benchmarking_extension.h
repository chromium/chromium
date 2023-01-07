// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_BENCHMARKING_EXTENSION_H_
#define CHROME_RENDERER_BENCHMARKING_EXTENSION_H_

#include <memory>

namespace v8 {
class Extension;
}

namespace extensions_v8 {

// Profiler is an extension to allow javascript access to the API for
// an external profiler program (such as Quantify). The "External" part of the
// name is to distinguish it from the built-in V8 Profiler.
class BenchmarkingExtension {
 public:
  static std::unique_ptr<v8::Extension> Get();
};

}  // namespace extensions_v8

#endif  // CHROME_RENDERER_BENCHMARKING_EXTENSION_H_

