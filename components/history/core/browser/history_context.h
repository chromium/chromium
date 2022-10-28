// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CORE_BROWSER_HISTORY_CONTEXT_H_
#define COMPONENTS_HISTORY_CORE_BROWSER_HISTORY_CONTEXT_H_

#include <cstdint>

namespace history {

// Identifier for a context to scope the lifetime of navigation entry
// references. ContextIDs are derived from Context*, used in comparison only,
// and are never dereferenced. We use an std::uintptr_t here to match the size
// of a pointer, and to prevent dereferencing. Also, our automated tooling
// complains about dangling pointers if we pass around a Context*.
using ContextID = std::uintptr_t;

// Context is an empty struct that is used to scope the lifetime of
// navigation entry references. They don't have any data and their
// lifetime is controlled by the embedder, thus they don't need a
// virtual destructor.
struct Context {
 public:
  Context(const Context&) = delete;
  Context& operator=(const Context&) = delete;

  ContextID GetContextID() const {
    // Safe because `ContextID` is big enough to hold the pointer value.
    return reinterpret_cast<ContextID>(this);
  }

 protected:
  Context() = default;
  ~Context() = default;
};

}  // namespace history

#endif  // COMPONENTS_HISTORY_CORE_BROWSER_HISTORY_CONTEXT_H_
