// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_PERFORMANCE_MANAGER_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_PERFORMANCE_MANAGER_H_

#include "base/memory/weak_ptr.h"
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
class PageNode;
class ProcessNode;
class PerformanceManagerObserver;
class WorkerNode;

// The performance manager is a rendezvous point for communicating with the
// performance manager graph. All functions need to be called from the main
// thread only, and if IsAvailable() returns true.
class PerformanceManager {
 public:
  virtual ~PerformanceManager();

  PerformanceManager(const PerformanceManager&) = delete;
  PerformanceManager& operator=(const PerformanceManager&) = delete;

  // Returns true if the performance manager is initialized.
  // TODO(https://crbug.com/405159980): Now that the performance manager lives
  // on the main thread, remove this function in favor of making GetGraph()
  // return nullptr when it is not available.
  static bool IsAvailable();

  // Returns the performance manager graph. Note that this CHECKs if
  // IsAvailable() returns true instead of returning nullptr.
  static Graph* GetGraph();

  // Returns a WeakPtr to the *primary* PageNode associated with a given
  // WebContents, or a null WeakPtr if there's no PageNode for this WebContents.
  // The returned WeakPtr should only be dereferenced on the main thread.
  // NOTE: Consider using `GetFrameNodeForRenderFrameHost` and retrieving the
  // page from there if you are in the context of a specific RenderFrameHost.
  static base::WeakPtr<PageNode> GetPrimaryPageNodeForWebContents(
      content::WebContents* wc);

  // Returns a WeakPtr to the FrameNode associated with a given RenderFrameHost,
  // or a null WeakPtr if there's no FrameNode for this RFH. (There is a brief
  // window after the RFH is created before the FrameNode is added.) The
  // returned WeakPtr should only be dereferenced on the main thread.
  static base::WeakPtr<FrameNode> GetFrameNodeForRenderFrameHost(
      content::RenderFrameHost* rfh);

  // Returns a WeakPtr to the ProcessNode associated with the browser process,
  // or a null WeakPtr if there is none. The returned WeakPtr should only be
  // dereferenced on the main thread.
  static base::WeakPtr<ProcessNode> GetProcessNodeForBrowserProcess();

  // Returns a WeakPtr to the ProcessNode associated with a given
  // RenderProcessHost, or a null WeakPtr if there's no ProcessNode for this
  // RPH. (There is a brief window after the RPH is created before the
  // ProcessNode is added.) The returned WeakPtr should only be dereferenced on
  // the main thread.
  static base::WeakPtr<ProcessNode> GetProcessNodeForRenderProcessHost(
      content::RenderProcessHost* rph);

  // Returns a WeakPtr to the ProcessNode associated with a given
  // RenderProcessHostId (which must be valid), or a null WeakPtr if there's no
  // ProcessNode for this ID. (There may be no RenderProcessHost for this ID,
  // or it may be during a brief window after the RPH is created but before the
  // ProcessNode is added.) The returned WeakPtr should only be dereferenced on
  // the main thread.
  static base::WeakPtr<ProcessNode> GetProcessNodeForRenderProcessHostId(
      RenderProcessHostId id);

  // Returns a WeakPtr to the ProcessNode associated with a given
  // BrowserChildProcessHost, or a null WeakPtr if there's no ProcessNode for
  // this BCPH. (There is a brief window after the BCPH is created before the
  // ProcessNode is added.) The returned WeakPtr should only be dereferenced on
  // the main thread.
  static base::WeakPtr<ProcessNode> GetProcessNodeForBrowserChildProcessHost(
      content::BrowserChildProcessHost* bcph);

  // Returns a WeakPtr to the ProcessNode associated with a given
  // BrowserChildProcessHostId (which must be valid), or a null WeakPtr if
  // there's no ProcessNode for this ID. (There may be no BCPH for this ID, or
  // it may be during a brief window after the BCPH is created but before the
  // ProcessNode is added.) The returned WeakPtr should only be dereferenced on
  // the main thread.
  static base::WeakPtr<ProcessNode> GetProcessNodeForBrowserChildProcessHostId(
      BrowserChildProcessHostId id);

  // Returns a WeakPtr to the WorkerNode associated with the given WorkerToken,
  // or a null WeakPtr if there's no WorkerNode for this token. The returned
  // WeakPtr should only be dereferenced on the main thread.
  static base::WeakPtr<WorkerNode> GetWorkerNodeForToken(
      const blink::WorkerToken& token);

  // Adds/removes an observer that is notified of PerformanceManager events.
  static void AddObserver(PerformanceManagerObserver* observer);
  static void RemoveObserver(PerformanceManagerObserver* observer);

  // Logs metrics on Performance Manager's memory usage to UMA. Does nothing
  // when IsAvailable() returns false
  static void RecordMemoryMetrics();

 protected:
  PerformanceManager();
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_PERFORMANCE_MANAGER_H_
