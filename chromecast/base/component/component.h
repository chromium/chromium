// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A component is a large, long-lived set of functionality that may be enabled
// or disabled at runtime. Some examples include: the multizone features,
// MetricsRecorder, or OpencastController. Components may depend on each other
// (ie, a component may call the public methods of other components); the
// Component infrastructure ensures that when a component is disabled, nothing
// that depends on it will call any of its methods until it is enabled again.
//
// Components may be used without a dependency relationship via a weak
// reference. A weak reference does not allow direct access to the component;
// instead, it must be either used to create a strict dependency (see below), or
// be converted to a scoped reference via Try(). Scoped references must be
// checked for validity before use (they are convertible to bool); an invalid
// scoped reference must not be used. Scoped references should be short-lived;
// to encourage this, they are only move-constructible and cannot be copied or
// assigned.
//
// If component Y depends on Component X, then Y has a Dependency reference
// to X. This causes Y to be disabled as X is being disabled (before X's
// OnDisable() method is called). Similarly, this dependency will cause X to be
// enabled when Y is being enabled (X will be enabled before Y's OnEnable()
// method is called). A component may freely access any of its dependencies
// as long as it is enabled. When a component is disabled, it must ensure that
// none of its dependencies will be used again until it is enabled. It is
// recommended to set up dependencies in your component's constructor; it is an
// error to add a dependency to a component that is not disabled.
//
// When a component is disabled, it will first recursively disable any other
// components that depend on it. It will also disable the creation of
// new scoped references. It then waits for all scoped references to be
// destroyed before calling OnDisable() to actually disable the component.
//
// Components MUST be disabled before they are deleted. For ease of use, a
// Destroy() method is provided. When Destroy() is called, it prevents the
// component from being enabled ever again, and then disables it, deleting it
// once it is disabled.
//
// Example usage:
//
// class MetricsRecorder : public Component<MetricsRecorder> {
//  public:
//   virtual ~MetricsRecorder() {}
//   virtual void RecordEvent(const std::string& event) = 0;
// };
//
// class SetupManager : public Component<SetupManager> {
//  public:
//   virtual ~SetupManager() {}
//   virtual int GetMultizoneDelay() = 0;
// };
//
// class Multizone : public Component<Multizone> {
//  public:
//   virtual ~Multizone() {}
//   virtual void DoMultizoneStuff() = 0;
// };
//
// class MetricsRecorderImpl : public MetricsRecorder {
//  public:
//   void OnEnable() override {
//     // ... Enable metrics reporting ...
//     OnEnableComplete(true);
//   }
//
//   // Release all resources; public methods will not be called after this.
//   void OnDisable() override {
//     OnDisableComplete();
//   }
//
//   void RecordEvent(const std::string& event) override {
//     // ... Record an event ...
//   }
// };
//
// class SetupManagerImpl : public SetupManager {
//  public:
//   void OnEnable() override {
//     // ... Enable setup manager ...
//     // OnEnableComplete() may be called asynchronously.
//     base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
//         FROM_HERE, base::BindOnce(&SetupManagerImpl::CompleteEnable,
//                                   base::Unretained(this)));
//   }
//
//   void CompleteEnable() {
//     OnEnableComplete(true);
//   }
//
//   void OnDisable() override {
//     OnDisableComplete();
//   }
//
//   int GetMultizoneDelay() override { return 0; }
// };
//
// class MultizoneImpl : public Multizone {
//  public:
//   MultizoneImpl(const MetricsRecorder::WeakRef& metrics_recorder,
//                 const SetupManager::WeakRef& setup_manager)
//       : metrics_recorder_(metrics_recorder, this),
//         setup_manager_(setup_manager) {
//     // We can try to use weak deps even before this component is enabled.
//     // However, we MUST NOT attempt to use any strong dependencies.
//     if (auto setup = setup_manager_.Try()) {
//       int delay = setup->GetMultizoneDelay();
//       // ... Do something with delay ...
//     }
//   }
//
//   void OnEnable() override {
//     // ... Enable multizone ...
//     // Can use strong dependencies directly
//     metrics_recorder_->RecordEvent("enable multizone");
//     OnEnableComplete();
//   }
//
//   void OnDisable() override {
//     // Can still use strong dependencies here. However, this method MUST
//     // ensure that strong dependencies will NOT be used after it returns.
//     metrics_recorder_->RecordEvent("disable multizone");
//     OnDisableComplete();
//   }
//
//   void DoMultizoneStuff() {
//     metrics_recorder_->RecordEvent("multizone stuff");
//     // You have to Try() every time you use a weak dependency.
//     if (auto setup = setup_manager_.Try()) {
//       int delay = setup->GetMultizoneDelay();
//       // ... Do something with delay ...
//     }
//   }
//
//  private:
//   MetricsRecorder::Dependency metrics_recorder_;
//   SetupManager::WeakRef setup_manager_;
// };

#ifndef CHROMECAST_BASE_COMPONENT_COMPONENT_H_
#define CHROMECAST_BASE_COMPONENT_COMPONENT_H_

#include <vector>

#include "base/check.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list_threadsafe.h"
#include "base/threading/thread_checker.h"
#include "chromecast/base/component/component_internal.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace chromecast {

class ComponentBase {
 public:
  class Observer {
   public:
    // Called when a component finishes being enabled. If the component was
    // enabled successfully, |success| will be |true|. Note that access to
    // |component| is not guaranteed to be safe; since the observers are
    // notified asynchronously, |component| may have been already deleted.
    virtual void OnComponentEnabled(ComponentBase* component, bool success) {}
    // Called when a component has been disabled. Access to |component| is not
    // guaranteed to be safe.
    virtual void OnComponentDisabled(ComponentBase* component) {}

   protected:
    virtual ~Observer() {}
  };

  ComponentBase(const ComponentBase&) = delete;
  ComponentBase& operator=(const ComponentBase&) = delete;

  virtual ~ComponentBase();

  // Enables this component if possible. Attempts to enable all strong
  // dependencies first. It is OK to call Disable() while the component is in
  // the process of being enabled. All components MUST be created/enabled/
  // disabled/destroyed on the same thread.
  // Note that enabling a component may occur asynchronously; components must
  // always be accessed through a Dependency or WeakReference to ensure safety.
  // TODO(kmackay) Consider allowing components to be used on any thread.
  void Enable();

  // Disables this component; disabling may complete asynchronously. It is OK to
  // call Enable() again while the component is being disabled. Note that a
  // component MUST be disabled (or never enabled) before it is deleted.
  void Disable();

  // Deletes this component, disabling it first if necessary.
  void Destroy();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 protected:
  ComponentBase();

  // Enables the component implementation. This method must set things up so
  // that any public method calls are valid, and then call OnEnableComplete(),
  // passing in |true| if the enable was successful, |false| otherwise.
  // OnEnableComplete() may be called from any thread. OnEnable() will not be
  // called again until after the component has been disabled, and will not be
  // called during an ongoing OnDisable() call (so if OnDisable() is called,
  // then OnEnable() will not be called until OnDisableComplete() has been
  // called). This method is called only on the thread that the component was
  // created on.
  virtual void OnEnable() = 0;

  // Disables the component implementation. This is not called until there are
  // no more live dependencies, so there will be no more public method calls
  // to the component until after OnEnable() is called again. This method must
  // do whatever is necessary to ensure that no more calls to dependencies of
  // this component will be made, and then call the |disabled_cb|. The
  // |disabled_cb| may be called from any thread. This method is called only on
  // the thread that the component was created on.
  virtual void OnDisable() = 0;

  // Handles the success/failure of a call to OnEnable(). When OnEnable() is
  // called, it must eventually call OnEnableComplete() (after the component is
  // ready to be used by dependents), passing in |true| if the component was
  // enabled successfully. If |success| is false, then OnDisable() will be
  // called immediately to return the component to a consistent disabled state.
  // May be called on any thread.
  void OnEnableComplete(bool success);

  // Handles the completion of a call to OnDisable(). When OnDisable() is
  // called, it must eventually call OnDisableComplete() (after ensuring that
  // none of the component's strong dependencies will be used anymore). May be
  // called on any thread.
  void OnDisableComplete();

 private:
  friend class subtle::DependencyCount;
  friend class subtle::DependencyBase;
  friend class subtle::WeakReferenceBase;

  enum State {
    kStateDisabled,
    kStateDisabling,
    kStateEnabled,
    kStateEnabling,
    kStateDestroying
  };

  void DependencyReady();
  void TryOnEnable();
  void OnEnableCompleteInternal(bool success);
  void DependencyCountDisableComplete();
  void TryOnDisable();
  void OnDisableCompleteInternal();
  void AddDependency(subtle::DependencyBase* dependency);
  void StopUsingDependencies();
  // Returns |true| if |component| is a transitive dependency of this component.
  bool DependsOn(ComponentBase* component);

  const scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  scoped_refptr<subtle::DependencyCount> counter_;
  std::vector<subtle::DependencyBase*> strong_dependencies_;
  State state_;
  // |true| when a call to OnEnable()/OnDisable() is in progress.
  bool async_call_in_progress_;
  int pending_dependency_count_;
  const scoped_refptr<base::ObserverListThreadSafe<Observer>> observers_;
};

template <typename C>
class StrongDependency : public subtle::DependencyBase {
 public:
  StrongDependency(const WeakReference<C>& dependency, ComponentBase* dependent)
      : subtle::DependencyBase(dependency, dependent) {}

  StrongDependency(const StrongDependency&) = delete;
  StrongDependency& operator=(const StrongDependency&) = delete;

  C* operator->() const {
    DCHECK(dependency_);
    return static_cast<C*>(dependency_);
  }
};

template <typename C>
class WeakReference : public subtle::WeakReferenceBase {
 public:
  explicit WeakReference(const C& dependency) : WeakReferenceBase(dependency) {}
  explicit WeakReference(const StrongDependency<C>& dependency)
      : subtle::WeakReferenceBase(dependency) {}

  // Explicitly allow copy.
  WeakReference(const WeakReference& other) = default;
  WeakReference(WeakReference&& other) = default;

  // Disallow assignment.
  void operator=(const WeakReference&) = delete;

  // Try to get a scoped reference. Expected usage:
  // if (auto ref = weak.Try()) {
  //    // ... use ref ...
  // }
  subtle::Ref_DO_NOT_DECLARE<C> Try() const {
    return subtle::Ref_DO_NOT_DECLARE<C>(counter_);
  }
};

template <typename C>
class Component : public ComponentBase {
 public:
  using WeakRef = WeakReference<C>;
  using Dependency = StrongDependency<C>;

  Component() = default;

  Component(const Component&) = delete;
  Component& operator=(const Component&) = delete;

  WeakRef GetRef() { return WeakRef(*static_cast<C*>(this)); }
};

}  // namespace chromecast

#endif  // CHROMECAST_BASE_COMPONENT_COMPONENT_H_
