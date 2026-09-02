// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_SAFE_INVOKE_SAFE_INVOKE_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_SAFE_INVOKE_SAFE_INVOKE_H_

#include <functional>
#include <memory>
#include <optional>
#include <type_traits>
#include <utility>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/types/to_address.h"

template <typename T>
class SafeChain;

namespace internal {

// TODO(crbug.com/555734133): Add trait support for intermediate `.Then()` calls
// that return smart pointer types (`base::WeakPtr`, `scoped_refptr`) to
// automatically unwrap them and continue chaining in `SafeChain<T>`.
// Additionally, determine the policy for `std::unique_ptr` return types (e.g.
// managing ownership transfer vs. explicitly failing at compile time).

// TODO(crbug.com/555736563): Support pointer-like return types (such as
// `gfx::NativeView` / `gfx::NativeWindow`, which are class wrappers on macOS
// but raw pointers on other platforms) to ensure consistent chaining across
// platforms.

// TODO(crbug.com/555741149): Support automatic string-like parameter
// conversions (e.g. between `const char*`, `std::string`, and
// `std::string_view`) when forwarding return values as arguments along the
// chain.

// -----------------------------------------------------------------------------
// Type Traits
// -----------------------------------------------------------------------------

// Trait to detect Chromium's base::OnceCallback and base::RepeatingCallback to
// ensure that the right syntax (.Run) is used to execute them.
template <typename T>
struct IsBaseCallback : std::false_type {};

template <typename Signature>
struct IsBaseCallback<base::OnceCallback<Signature>> : std::true_type {};

template <typename Signature>
struct IsBaseCallback<base::RepeatingCallback<Signature>> : std::true_type {};

template <typename T>
inline constexpr bool is_base_callback_v =
    IsBaseCallback<std::remove_cvref_t<T>>::value;

// Unified invocation engine that abstracts callable dispatch:
// - Chromium Callbacks: Invoked via `.Run(args...)`.
// - Standard C++ Callables (member functions, free functions, lambdas):
//   Invoked via `std::invoke(...)`.
template <typename Fn, typename... Args>
decltype(auto) InvokeCallable(Fn&& fn, Args&&... args) {
  if constexpr (is_base_callback_v<Fn>) {
    return std::forward<Fn>(fn).Run(std::forward<Args>(args)...);
  } else {
    return std::invoke(std::forward<Fn>(fn), std::forward<Args>(args)...);
  }
}

// Deduces return type of invoking `Fn` with `Args...`.
template <typename Fn, typename... Args>
using InvokeResult =
    decltype(InvokeCallable(std::declval<Fn>(), std::declval<Args>()...));

}  // namespace internal

// A lightweight wrapper providing safe null-navigation and method chaining.
//
// If the underlying pointer is null, all subsequent `.Then(...)` calls in the
// chain are safely skipped.
//
// For detailed documentation, lifetime rules, and usage examples, see
// chrome/browser/ui/views/frame/safe_invoke/README.md.
template <typename T>
class SafeChain {
 public:
  explicit SafeChain(T* ptr) : ptr_(ptr) {}
  ~SafeChain() = default;

  SafeChain(const SafeChain&) = delete;
  SafeChain& operator=(const SafeChain&) = delete;

  SafeChain(SafeChain&&) = delete;
  SafeChain& operator=(SafeChain&&) = delete;

  // [[nodiscard]] is enforced for non-void returns so that the caller does not
  // accidentally drop the result of a method call.
  template <typename Fn, typename... Args>
    requires(!std::is_void_v<internal::InvokeResult<Fn, T*, Args...>>)
  [[nodiscard]] auto Then(Fn&& fn, Args&&... args) && {
    using Result = internal::InvokeResult<Fn, T*, Args...>;

    const bool can_execute = ptr_ != nullptr;

    if constexpr (std::is_pointer_v<std::decay_t<Result>>) {
      // For pointer-returning methods, wraps the resulting pointer in a new
      // SafeChain to allow further chaining.
      using NextType = std::remove_pointer_t<std::decay_t<Result>>;
      return SafeChain<NextType>(
          can_execute ? internal::InvokeCallable(std::forward<Fn>(fn), ptr_,
                                                 std::forward<Args>(args)...)
                      : nullptr);
    } else if constexpr (std::is_lvalue_reference_v<Result>) {
      // For reference-returning methods (e.g. `const T&`, `T&`), takes the
      // address via std::addressof without copying, enabling chaining on
      // references and supporting move-only types.
      using NextType = std::remove_reference_t<Result>;
      return SafeChain<NextType>(
          can_execute
              ? std::addressof(internal::InvokeCallable(
                    std::forward<Fn>(fn), ptr_, std::forward<Args>(args)...))
              : nullptr);
    } else {
      // For value-returning methods, returns the result wrapped in
      // std::optional (or std::nullopt if the pointer is null), avoiding
      // double-wrapping if the callable already returns an std::optional.
      using ValType = std::remove_cvref_t<Result>;
      if constexpr (requires(ValType v) {
                      v.has_value();
                      ValType(std::nullopt);
                    }) {
        return can_execute
                   ? internal::InvokeCallable(std::forward<Fn>(fn), ptr_,
                                              std::forward<Args>(args)...)
                   : ValType(std::nullopt);
      } else {
        using Opt = std::optional<ValType>;
        return can_execute
                   ? Opt(internal::InvokeCallable(std::forward<Fn>(fn), ptr_,
                                                  std::forward<Args>(args)...))
                   : Opt(std::nullopt);
      }
    }
  }

  // Void returns: No [[nodiscard]] attribute.
  template <typename Fn, typename... Args>
    requires(std::is_void_v<internal::InvokeResult<Fn, T*, Args...>>)
  void Then(Fn&& fn, Args&&... args) && {
    if (ptr_ != nullptr) {
      internal::InvokeCallable(std::forward<Fn>(fn), ptr_,
                               std::forward<Args>(args)...);
    }
  }

  // Returns the raw underlying pointer (or nullptr).
  T* get() && { return ptr_; }

  // Enables contextual boolean checks: `if (SafeInvoke(view)) { ...
  // }`
  explicit operator bool() const { return ptr_ != nullptr; }

 private:
  // Native raw pointer. SafeChain is strictly a short-lived stack temporary
  // wrapper for expression chaining; MiraclePtr (raw_ptr<T>) is not needed
  // on stack temporaries.
  RAW_PTR_EXCLUSION T* ptr_ = nullptr;
};

// Entry point for safe chaining.
template <typename T>
[[nodiscard]] auto SafeInvoke(T&& ptr) {
  auto* raw = base::to_address(ptr);
  using ElementType = std::remove_pointer_t<decltype(raw)>;
  return SafeChain<ElementType>(raw);
}

// -----------------------------------------------------------------------------
// Disambiguation Helpers
// -----------------------------------------------------------------------------

// Disambiguation helper for overloaded non-const member functions and
// free/static functions.
//
// Usage:
//   Overload<ui::ElementIdentifier>(
//       &views::View::GetViewByElementId)
//   Overload<tabs::TabInterface*>(&SidePanelRegistry::From)
template <typename... Args>
struct OverloadCast {
  // 1. Matches non-const member functions: ReturnType (Class::*)(Args...)
  template <typename ReturnType, typename ClassType>
  constexpr auto operator()(
      ReturnType (ClassType::*pmf)(Args...)) const noexcept {
    return pmf;
  }

  // 2. Matches free / static functions: ReturnType (*)(Args...)
  template <typename ReturnType>
  constexpr auto operator()(ReturnType (*func)(Args...)) const noexcept {
    return func;
  }
};

// Disambiguation helper for overloaded const member functions.
//
// Usage:
//   ConstOverload<ui::ElementIdentifier>(
//       &views::View::GetViewByElementId)
template <typename... Args>
struct ConstOverloadCast {
  // Matches const member functions: ReturnType (Class::*)(Args...) const
  template <typename ReturnType, typename ClassType>
  constexpr auto operator()(ReturnType (ClassType::*pmf)(Args...)
                                const) const noexcept {
    return pmf;
  }
};

// Variable templates for concise call-site syntax:
template <typename... Args>
inline constexpr OverloadCast<Args...> Overload{};

template <typename... Args>
inline constexpr ConstOverloadCast<Args...> ConstOverload{};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_SAFE_INVOKE_SAFE_INVOKE_H_
