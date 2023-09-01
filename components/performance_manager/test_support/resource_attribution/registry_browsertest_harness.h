// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_TEST_SUPPORT_RESOURCE_ATTRIBUTION_REGISTRY_BROWSERTEST_HARNESS_H_
#define COMPONENTS_PERFORMANCE_MANAGER_TEST_SUPPORT_RESOURCE_ATTRIBUTION_REGISTRY_BROWSERTEST_HARNESS_H_

#include "components/performance_manager/test_support/performance_manager_browsertest_harness.h"

#include "base/functional/function_ref.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "components/performance_manager/public/performance_manager.h"
#include "components/performance_manager/public/resource_attribution/resource_contexts.h"
#include "content/public/browser/global_routing_id.h"
#include "content/shell/browser/shell.h"

namespace content {
class WebContents;
}

namespace performance_manager {
class Graph;
}

namespace performance_manager::resource_attribution {

// A test harness that creates PM nodes to test with ResourceContext registry
// classes. By default this also enables the registries in GraphFeatures.
class RegistryBrowserTestHarness : public PerformanceManagerBrowserTestHarness {
 public:
  using Super = PerformanceManagerBrowserTestHarness;

  explicit RegistryBrowserTestHarness(bool enable_registries = true);
  ~RegistryBrowserTestHarness() override;

  RegistryBrowserTestHarness(const RegistryBrowserTestHarness&) = delete;
  RegistryBrowserTestHarness& operator=(const RegistryBrowserTestHarness&) =
      delete;

  // Gets a pointer to the given Registry class and passes it to `function` on
  // the PM sequence, blocking the main thread until `function` is executed. If
  // the registry is not enabled, `function` will be called with nullptr.
  template <typename Registry>
  static void RunInGraphWithRegistry(
      base::FunctionRef<void(const Registry*)> function);

  // Convenience function to return the default WebContents.
  content::WebContents* web_contents() const { return shell()->web_contents(); }

  // Returns a PageContext for the default WebContents without using the
  // PageContextRegistry.
  ResourceContext GetWebContentsPageContext() const;

 protected:
  // Creates a set of PM nodes for the test. By default this creates one
  // PageNode with two FrameNodes (a main frame and a subframe), each with their
  // own ProcessNode. Subclasses can override CreateNodes() and DeleteNodes() to
  // create additional nodes; call the inherited CreateNodes() last to wait
  // until all nodes are in the PM graph.
  virtual void CreateNodes();

  // Deletes all PM nodes created by CreateNodes(). This is called from
  // PostRunTestOnMainThread(), and can be called earlier to delete nodes during
  // the test. When overriding this, call the inherited DeleteNodes() last to
  // wait until all nodes are removed from the PM graph.
  virtual void DeleteNodes();

  // BrowserTestBase overrides:
  void SetUp() override;
  void PreRunTestOnMainThread() override;
  void PostRunTestOnMainThread() override;

 protected:
  // Details of the frames created by CreateFrameNodes().
  content::GlobalRenderFrameHostId main_frame_id_;
  content::GlobalRenderFrameHostId sub_frame_id_;

  // True if web_contents() has a page that must be unloaded to delete frames.
  bool web_contents_loaded_page_ = false;

 private:
  // True if the ResourceContext registries should be enabled for the test.
  bool enable_registries_ = true;
};

// A test harness that creates PM nodes to test but does NOT enable the
// ResourceContext registries.
class RegistryDisabledBrowserTestHarness : public RegistryBrowserTestHarness {
 public:
  RegistryDisabledBrowserTestHarness() : RegistryBrowserTestHarness(false) {}
};

// static
template <typename Registry>
void RegistryBrowserTestHarness::RunInGraphWithRegistry(
    base::FunctionRef<void(const Registry*)> function) {
  base::RunLoop run_loop;
  PerformanceManager::CallOnGraph(
      FROM_HERE, base::BindLambdaForTesting([function](Graph* graph) {
                   function(Registry::GetFromGraph(graph));
                 }).Then(run_loop.QuitClosure()));
  run_loop.Run();
}

}  // namespace performance_manager::resource_attribution

#endif  // COMPONENTS_PERFORMANCE_MANAGER_TEST_SUPPORT_RESOURCE_ATTRIBUTION_REGISTRY_BROWSERTEST_HARNESS_H_
