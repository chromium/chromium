// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BASE_COMPONENT_COMPONENT_INTERNAL_H_
#define CHROMECAST_BASE_COMPONENT_COMPONENT_INTERNAL_H_

#include "base/check.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"

namespace chromecast {

template <typename C>
class WeakReference;

class ComponentBase;

namespace subtle {

class WeakReferenceBase;

// Manages thread-safe dependency counting. Instances of this class are
// RefCountedThreadSafe since they must live as long as any dependent.
class DependencyCount;

// Base class for strong dependencies. A strong dependency is tied to a
// specific dependent component instance. This allows dependents to be
// disabled before the component they depend on. May be used on any thread.
class DependencyBase {
 public:
  DependencyBase(const WeakReferenceBase& dependency, ComponentBase* dependent);

  DependencyBase(const DependencyBase&) = delete;
  DependencyBase& operator=(const DependencyBase&) = delete;

  ~DependencyBase();

  void StartUsing();
  void StopUsing();
  bool DependsOn(ComponentBase* component);

 protected:
  ComponentBase* const dependent_;
  ComponentBase* dependency_;

 private:
  friend class DependencyCount;
  friend class WeakReferenceBase;

  void Ready(ComponentBase* dependency);
  void Disable();

  const scoped_refptr<DependencyCount> counter_;
  base::ThreadChecker thread_checker_;
};

// Base class for weak dependencies. Weak dependencies cannot be used
// directly; they must be converted to a strong dependency or a temp
// dependency first. May be converted on any thread.
class WeakReferenceBase {
 protected:
  friend class DependencyBase;

  explicit WeakReferenceBase(const ComponentBase& dependency);
  explicit WeakReferenceBase(const DependencyBase& dependency);
  WeakReferenceBase(const WeakReferenceBase& other);
  WeakReferenceBase(WeakReferenceBase&& other);
  ~WeakReferenceBase();

  const scoped_refptr<DependencyCount> counter_;
};

// Base class for temp dependencies. Temp dependencies are meant for
// short-term use only, but can be used from any thread.
class ScopedReferenceBase {
 protected:
  explicit ScopedReferenceBase(const scoped_refptr<DependencyCount>& counter);
  ScopedReferenceBase(ScopedReferenceBase&& other);
  ~ScopedReferenceBase();

  const scoped_refptr<DependencyCount> counter_;
  ComponentBase* dependency_;
};

// This class is not intended to be long-lived, and should not be declared as
// a variable type (eg, don't use it as a member variable). Instead, use auto
// (see WeakReference::Try() for an example).
template <typename C>
class Ref_DO_NOT_DECLARE : public ScopedReferenceBase {
 public:
  Ref_DO_NOT_DECLARE(Ref_DO_NOT_DECLARE&& other) = default;

  C* operator->() const {
    DCHECK(dependency_);
    return static_cast<C*>(dependency_);
  }

  explicit operator bool() const { return (dependency_ != nullptr); }

 private:
  friend class WeakReference<C>;

  explicit Ref_DO_NOT_DECLARE(const scoped_refptr<DependencyCount>& counter)
      : ScopedReferenceBase(counter) {}

  Ref_DO_NOT_DECLARE(const Ref_DO_NOT_DECLARE& other) = delete;
  Ref_DO_NOT_DECLARE& operator=(const Ref_DO_NOT_DECLARE& rhs) = delete;
  Ref_DO_NOT_DECLARE& operator=(Ref_DO_NOT_DECLARE&& rhs) = delete;
};

}  // namespace subtle
}  // namespace chromecast

#endif  // CHROMECAST_BASE_COMPONENT_COMPONENT_INTERNAL_H_
