// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_EDUCATION_COMMON_USER_EDUCATION_CONTEXT_H_
#define COMPONENTS_USER_EDUCATION_COMMON_USER_EDUCATION_CONTEXT_H_

#include "base/memory/ref_counted.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/framework_specific_implementation.h"

namespace user_education {

// Represents the "place" in which a promo could or should happen.
//
// This includes the `ElementContext` and a way to get an `AcceleratorProvider`,
// which is typically tied to a window and is platform-specific.
//
// Because e.g. preconditions need to reference a context whose surface or
// window may have already gone away, these objects are refcounted, and
// primarily passed via `UserEducationContextPtr` (see below).
class UserEducationContext : public ui::FrameworkSpecificImplementation,
                             public base::RefCounted<UserEducationContext> {
 public:
  // Returns whether the information in this object can still be used.
  //
  // Over time, some surface, window, or other object or resource this object
  // refers to may disappear. If that happens, `IsValid()` becomes false.
  virtual bool IsValid() const = 0;

  // Note: depending on the implementation and the situation, the result of
  // calling any of the below methods may CHECK() if `IsValid()` is false.

  // Gets the element context, which corresponds to the surface or window in
  // question, if one is specified.
  virtual ui::ElementContext GetElementContext() const = 0;

  // Gets the accelerator provider associated with the window, surface, or
  // application.
  virtual const ui::AcceleratorProvider* GetAcceleratorProvider() const = 0;

 protected:
  ~UserEducationContext() override = default;
  friend class base::RefCounted<UserEducationContext>;
};

// Use these pointer objects to pass around contexts.
using UserEducationContextPtr = scoped_refptr<UserEducationContext>;

}  // namespace user_education

#endif  // COMPONENTS_USER_EDUCATION_COMMON_USER_EDUCATION_CONTEXT_H_
