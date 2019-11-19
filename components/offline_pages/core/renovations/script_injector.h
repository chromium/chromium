// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_RENOVATIONS_SCRIPT_INJECTOR_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_RENOVATIONS_SCRIPT_INJECTOR_H_

#include "base/callback_forward.h"
#include "base/strings/string16.h"

namespace base {
class Value;
}  // namespace base

namespace offline_pages {

// Interface for injecting and running scripts in some
// context. Inject() takes a string of JavaScript, injects it into the
// context, then calls the ResultCallback with the expression value.
class ScriptInjector {
 public:
  using ResultCallback = base::OnceCallback<void(base::Value)>;

  virtual ~ScriptInjector() = default;

  // Takes a string of JavaScript, injects it into the context, then
  // calls the ResultCallback with the expression value.
  virtual void Inject(base::string16 script, ResultCallback callback) = 0;
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_RENOVATIONS_SCRIPT_INJECTOR_H_
