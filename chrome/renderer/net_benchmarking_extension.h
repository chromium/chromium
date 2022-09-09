// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_NET_BENCHMARKING_EXTENSION_H_
#define CHROME_RENDERER_NET_BENCHMARKING_EXTENSION_H_

#include <memory>

namespace v8 {
class Extension;
}

namespace extensions_v8 {

class NetBenchmarkingExtension {
 public:
  static std::unique_ptr<v8::Extension> Get();
};

}  // namespace extensions_v8

#endif  // CHROME_RENDERER_NET_BENCHMARKING_EXTENSION_H_
