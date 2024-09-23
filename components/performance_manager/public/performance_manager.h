// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_PERFORMANCE_MANAGER_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_PERFORMANCE_MANAGER_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "components/performance_manager/public/browser_child_process_host_id.h"
#include "components/performance_manager/public/render_process_host_id.h"
#include "third_party/blink/public/common/tokens/tokens.h"

namespace content {
class BrowserChildProcessHost;
class RenderFrameHost;
class RenderProcessHost;
class WebContents;
}

namespace performance_manager {

class FrameNode;
class Graph;
class GraphOwned;
class PageNode;
class ProcessNode;
class PerformanceManagerMainThreadMechanism;
class PerformanceManagerMainThreadObserver;
class PerformanceManagerOwned;
class PerformanceManagerRegistered;
class WorkerNode;

template <typename DerivedType>
class PerformanceManagerRegisteredImpl;

// The performance manager is a rendezvous point for communicating with the
// performance manager graph on its dedicated sequence.
class PerformanceManager {
 public:
  virtual ~PerformanceManager();

  PerformanceManager(const PerformanceManager&) = delete;
  PerformanceManager& operator=(const PerformanceManager&) = delete;

  // Returns true if the performance manager is initialized. Valid to call from
  // the main thread only.
  static bool IsAvailable();

  // Posts a callback that will run on the PM sequence. Valid to call from any
  // sequence.
  //
  // Note: If called from the main thread, the |callback| is guaranteed to run
  //       if and only if "IsAvailable()" returns true.
  //
  //       If called from any other sequence, there is no guarantee that the
  //       callback will run. It will depend on if the PerformanceManager was
  //       destroyed before the the task is scheduled.
  static void CallOnGraph(const base::Location& from_here,
                          base::OnceClosure callback);

  // Same as the above, but the callback is provided a pointer to the graph.
  using GraphCallback = base::OnceCallback<void(Graph*)>;
  static void CallOnGraph(const base::Location& from_here,
                          GraphCallback callback);

  // Passes a GraphOwned object into the Graph on the PM sequence. Must only be
  // called if "IsAvailable()" returns true. Valid to call from the main thread
  // only.
  static void PassToGraph(const base::Location& from_here,
                          std::unique_ptr<GraphOwned> graph_owned);

  // Returns a WeakPtr to the *primary* PageNode associated with a given
  // WebContents, or a null WeakPtr if there's no PageNode for this WebContents.
  // Valid to call from the main thread only, the returned WeakPtr should only
  // be dereferenced on the PM sequence (e.g. it can be used in a
  // CallOnGraph callback).
  // NOTE: Consider using GetPageNodeForRenderFrameHost if you are in the
  // context of a specific RenderFrameHost.
  static base::WeakPtr<PageNode> GetPrimaryPageNodeForWebContents(
      content::WebContents* wc);

  // Returns a WeakPtr to the FrameNode associated with a given
  // RenderFrameHost, or a null WeakPtr if there's no FrameNode for this RFH.
  // (There is a brief window after the RFH is created before the FrameNode is
  // added.) Valid to call from the main thread only, the returned WeakPtr
  // should only be dereferenced on the PM sequence (e.g. it can be used in a
  // CallOnGraph callback).
  static base::WeakPtr<FrameNode> GetFrameNodeForRenderFrameHost(
      content::RenderFrameHost* rfh);

  // Returns a WeakPtr to the ProcessNode associated with the browser process,
  // or a null WeakPtr if there is none. Valid to call from the main thread
  // only, the returned WeakPtr should only be dereferenced on the PM sequence
  // (e.g. it can be used in a CallOnGraph callback).
  static base::WeakPtr<ProcessNode> GetProcessNodeForBrowserProcess();

  // Returns a WeakPtr to the ProcessNode associated with a given
  // RenderProcessHost, or a null WeakPtr if there's no ProcessNode for this
  // RPH. (There is a brief window after the RPH is created before the
  // ProcessNode is added.) Valid to call from the main thread only, the
  // returned WeakPtr should only be dereferenced on the PM sequence (e.g. it
  // can be used in a CallOnGraph callback).
  static base::WeakPtr<ProcessNode> GetProcessNodeForRenderProcessHost(
      content::RenderProcessHost* rph);

  // Returns a WeakPtr to the ProcessNode associated with a given
  // RenderProcessHostId (which must be valid), or a null WeakPtr if there's no
  // ProcessNode for this ID. (There may be no RenderProcessHost for this ID,
  // or it may be during a brief window after the RPH is created but before the
  // ProcessNode is added.) Valid to call from the main thread only, the
  // returned WeakPtr should only be dereferenced on the PM sequence (e.g. it
  // can be used in a CallOnGraph callback).
  static base::WeakPtr<ProcessNode> GetProcessNodeForRenderProcessHostId(
      RenderProcessHostId id);

  // Returns a WeakPtr to the ProcessNode associated with a given
  // BrowserChildProcessHost, or a null WeakPtr if there's no ProcessNode for
  // this BCPH. (There is a brief window after the BCPH is created before the
  // ProcessNode is added.) Valid to call from the main thread only, the
  // returned WeakPtr should only be dereferenced on the PM sequence (e.g. it
  // can be used in a CallOnGraph callback).
  static base::WeakPtr<ProcessNode> GetProcessNodeForBrowserChildProcessHost(
      content::BrowserChildProcessHost* bcph);

  // Returns a WeakPtr to the ProcessNode associated with a given
  // BrowserChildProcessHostId (which must be valid), or a null WeakPtr if
  // there's no ProcessNode for this ID. (There may be no BCPH for this ID, or
  // it may be during a brief window after the BCPH is created but before the
  // ProcessNode is added.) Valid to call from the main thread only, the
  // returned WeakPtr should only be dereferenced on the PM sequence (e.g. it
  // can be used in a CallOnGraph callback).
  static base::WeakPtr<ProcessNode> GetProcessNodeForBrowserChildProcessHostId(
      BrowserChildProcessHostId id);

  // Returns a WeakPtr to the WorkerNode associated with the given WorkerToken,
  // or a null WeakPtr if there's no WorkerNode for this token.
  static base::WeakPtr<WorkerNode> GetWorkerNodeForToken(
      const blink::WorkerToken& token);

  // Adds / removes an observer that is notified of PerformanceManager events
  // that happen on the main thread. Can only be called on the main thread.
  static void AddObserver(PerformanceManagerMainThreadObserver* observer);
  static void RemoveObserver(PerformanceManagerMainThreadObserver* observer);

  // Adds / removes a mechanism that need to be called synchronously on the main
  // thread (ie, to apply NavigationThrottles).
  static void AddMechanism(PerformanceManagerMainThreadMechanism* mechanism);
  static void RemoveMechanism(PerformanceManagerMainThreadMechanism* mechanism);
  static bool HasMechanism(PerformanceManagerMainThreadMechanism* mechanism);

  // For convenience, allows you to pass ownership of an object that lives on
  // the main thread to the performance manager. Useful for attaching observers
  // or mechanisms that will live with the PM until it dies. If you can name the
  // object you can also take it back via "TakeFromPM". The objects will be
  // torn down gracefully (and their "OnTakenFromPM" functions invoked) as the
  // PM itself is torn down.
  static void PassToPM(std::unique_ptr<PerformanceManagerOwned> pm_owned);
  static std::unique_ptr<PerformanceManagerOwned> TakeFromPM(
      PerformanceManagerOwned* pm_owned);

  // A TakeFromPM helper for taking back the ownership of a
  // PerformanceManagerOwned object via its DerivedType.
  template <typename DerivedType>
  static std::unique_ptr<DerivedType> TakeFromPMAs(DerivedType* pm_owned) {
    return base::WrapUnique(
        static_cast<DerivedType*>(TakeFromPM(pm_owned).release()));
  }

  // Registers an object with the PM. It is expected that no more than one
  // object of a given type is registered at a given moment, and that all
  // registered objects are unregistered before PM tear-down. This allows the
  // PM to act as a rendez-vous point for objects that live on the main thread.
  // Combined with PerformanceManagerOwned this offers an alternative to
  // using singletons, and brings clear ownerships and lifetime semantics.
  static void RegisterObject(PerformanceManagerRegistered* pm_object);

  // Unregisters the provided |object|, which must previously have been
  // registered with "RegisterObject". It is expected that all registered
  // objects are unregistered before graph tear-down.
  static void UnregisterObject(PerformanceManagerRegistered* object);

  // Returns the registered object of the given type, nullptr if none has been
  // registered.
  template <typename DerivedType>
  static DerivedType* GetRegisteredObjectAs() {
    // Be sure to access the TypeId provided by PerformanceManagerRegisteredImpl
    // in case this class has other TypeId implementations.
    PerformanceManagerRegistered* object = GetRegisteredObject(
        PerformanceManagerRegisteredImpl<DerivedType>::TypeId());
    return static_cast<DerivedType*>(object);
  }

  // Returns the performance manager graph task runner. This is safe to call
  // from any thread at any time between the creation of the thread pool and its
  // destruction.
  //
  // NOTE: Tasks posted to this sequence from any thread but the UI thread, or
  // on the UI thread after IsAvailable() returns false, cannot safely access
  // the graph, graphowned objects or other performance manager related objects.
  // In practice it's preferable to use CallOnGraph() whenever possible.
  static scoped_refptr<base::SequencedTaskRunner> GetTaskRunner();

  // Logs metrics on Performance Manager's memory usage to UMA. Does nothing
  // when IsAvailable() returns false. Valid to call from the main thread only.
  static void RecordMemoryMetrics();

 protected:
  PerformanceManager();

  // Retrieves the object with the given |type_id|, returning nullptr if none
  // exists. Clients must use the GetRegisteredObjectAs wrapper instead.
  static PerformanceManagerRegistered* GetRegisteredObject(uintptr_t type_id);
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_PERFORMANCE_MANAGER_H_
