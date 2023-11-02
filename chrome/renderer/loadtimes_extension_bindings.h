// Copyright 2006-2008 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The LoadTimesExtension is a v8 extension to access the time it took
// to load a page.

#ifndef CHROME_RENDERER_LOADTIMES_EXTENSION_BINDINGS_H_
#define CHROME_RENDERER_LOADTIMES_EXTENSION_BINDINGS_H_

#include <memory>

namespace v8 {
class Extension;
}

namespace extensions_v8 {

class LoadTimesExtension {
 public:
  static std::unique_ptr<v8::Extension> Get();
};

}  // namespace extensions_v8

#endif  // CHROME_RENDERER_LOADTIMES_EXTENSION_BINDINGS_H_
