// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DBUS_UTILS_BIND_WEAK_PTR_FOR_EXPORT_METHOD_H_
#define COMPONENTS_DBUS_UTILS_BIND_WEAK_PTR_FOR_EXPORT_METHOD_H_

#include <utility>

#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
#include "components/dbus/utils/export_method.h"
#include "dbus/message.h"

namespace dbus_utils {

namespace internal {

template <typename Method, typename C, typename R, typename... Args>
auto BindWeakPtrForExportMethodImpl(Method method, base::WeakPtr<C> weak_ptr) {
  return base::BindRepeating(
      [](Method method, base::WeakPtr<C> weak_ptr, Args... args) {
        if (!weak_ptr) {
          return R(base::unexpected(
              ExportMethodError{DBUS_ERROR_FAILED, "Object destroyed"}));
        }
        return ((*weak_ptr).*method)(std::forward<Args>(args)...);
      },
      method, weak_ptr);
}

}  // namespace internal

// A helper that creates a callback bound to a WeakPtr. If the object is
// destroyed, the callback sends a generic error. Intended to be used with
// ExportMethod.
template <typename R, typename C, typename... Args>
auto BindWeakPtrForExportMethod(R (C::*method)(Args...),
                                base::WeakPtr<C> weak_ptr) {
  return internal::BindWeakPtrForExportMethodImpl<R (C::*)(Args...), C, R,
                                                  Args...>(method, weak_ptr);
}
// A const variant of the above.
template <typename R, typename C, typename... Args>
auto BindWeakPtrForExportMethod(R (C::*method)(Args...) const,
                                base::WeakPtr<C> weak_ptr) {
  return internal::BindWeakPtrForExportMethodImpl<R (C::*)(Args...) const, C, R,
                                                  Args...>(method, weak_ptr);
}

}  // namespace dbus_utils

#endif  // COMPONENTS_DBUS_UTILS_BIND_WEAK_PTR_FOR_EXPORT_METHOD_H_
