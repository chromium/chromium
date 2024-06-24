// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INPUT_RENDER_INPUT_ROUTER_ITERATOR_H_
#define COMPONENTS_INPUT_RENDER_INPUT_ROUTER_ITERATOR_H_

namespace input {

class RenderInputRouter;

class COMPONENT_EXPORT(INPUT) RenderInputRouterIterator {
 public:
  virtual ~RenderInputRouterIterator() = default;

  // Returns the next RenderInputRouter in the list. Returns nullptr if none is
  // available.
  virtual RenderInputRouter* GetNextRouter() = 0;
};

}  // namespace input

#endif  // COMPONENTS_INPUT_RENDER_INPUT_ROUTER_ITERATOR_H_
