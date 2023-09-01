// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/resource_attribution/page_context_registry.h"

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "components/performance_manager/public/graph/frame_node.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "components/performance_manager/public/graph/process_node.h"
#include "components/performance_manager/public/performance_manager.h"
#include "components/performance_manager/public/resource_attribution/resource_contexts.h"
#include "components/performance_manager/test_support/resource_attribution/registry_browsertest_harness.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/isolated_world_ids.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/prerender_test_util.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace performance_manager::resource_attribution {

namespace {

using ::testing::IsEmpty;
using ::testing::UnorderedElementsAre;

class PageContextRegistryTest : public RegistryBrowserTestHarness {
 public:
  using Super = RegistryBrowserTestHarness;

  explicit PageContextRegistryTest(bool enable_registries = true)
      : Super(enable_registries) {
    // This must be done before the server is started in
    // PerformanceManagerBrowserTestHarness::PreRunTestOnMainThread().
    prerender_helper_.RegisterServerRequestMonitor(embedded_test_server());
  }

  void CreateNodes() override {
    Super::CreateNodes();

    // Save the web_contents() pointer to detect if DeleteNodes() clears it.
    weak_web_contents_ = web_contents()->GetWeakPtr();

    // Prerender another page. This will also be deleted by DeleteNodes().
    prerender_url_ = embedded_test_server()->GetURL("a.com", "/a.html");
    int prerender_host_id = prerender_helper_.AddPrerender(
        prerender_url_, content::ISOLATED_WORLD_ID_GLOBAL);
    ASSERT_NE(prerender_host_id, content::RenderFrameHost::kNoFrameTreeNodeId);
    content::RenderFrameHost* prerender_rfh =
        prerender_helper_.GetPrerenderedMainFrameHost(prerender_host_id);
    ASSERT_TRUE(prerender_rfh);
    prerender_frame_id_ = prerender_rfh->GetGlobalId();
  }

 protected:
  // Details of the frames created by CreateNodes().
  content::GlobalRenderFrameHostId prerender_frame_id_;
  base::WeakPtr<content::WebContents> weak_web_contents_;

  // The url of a prendered page. Navigating to this will make the prerendered
  // page current.
  GURL prerender_url_;

  content::test::PrerenderTestHelper prerender_helper_{
      base::BindRepeating(&Super::web_contents, base::Unretained(this))};
};

class PageContextRegistryDisabledTest : public PageContextRegistryTest {
 public:
  PageContextRegistryDisabledTest() : PageContextRegistryTest(false) {}
};

IN_PROC_BROWSER_TEST_F(PageContextRegistryTest, PageContexts) {
  CreateNodes();

  auto* main_frame = content::RenderFrameHost::FromID(main_frame_id_);
  auto* sub_frame = content::RenderFrameHost::FromID(sub_frame_id_);
  auto* prerender_frame = content::RenderFrameHost::FromID(prerender_frame_id_);

  EXPECT_TRUE(weak_web_contents_);
  absl::optional<PageContext> context_from_web_contents =
      PageContextRegistry::ContextForWebContents(web_contents());
  EXPECT_EQ(context_from_web_contents,
            PageContextRegistry::ContextForRenderFrameHost(main_frame));
  EXPECT_EQ(context_from_web_contents,
            PageContextRegistry::ContextForRenderFrameHost(sub_frame));
  EXPECT_EQ(context_from_web_contents,
            PageContextRegistry::ContextForRenderFrameHost(prerender_frame));
  EXPECT_EQ(context_from_web_contents,
            PageContextRegistry::ContextForRenderFrameHostId(main_frame_id_));
  EXPECT_EQ(context_from_web_contents,
            PageContextRegistry::ContextForRenderFrameHostId(sub_frame_id_));
  EXPECT_EQ(
      context_from_web_contents,
      PageContextRegistry::ContextForRenderFrameHostId(prerender_frame_id_));

  ASSERT_TRUE(context_from_web_contents.has_value());
  const PageContext page_context = context_from_web_contents.value();
  const ResourceContext resource_context = page_context;
  EXPECT_EQ(web_contents(),
            PageContextRegistry::WebContentsFromContext(page_context));
  EXPECT_EQ(web_contents(),
            PageContextRegistry::WebContentsFromContext(resource_context));
  EXPECT_EQ(
      main_frame,
      PageContextRegistry::CurrentMainRenderFrameHostFromContext(page_context));
  EXPECT_EQ(main_frame,
            PageContextRegistry::CurrentMainRenderFrameHostFromContext(
                resource_context));
  EXPECT_THAT(
      PageContextRegistry::AllMainRenderFrameHostsFromContext(page_context),
      UnorderedElementsAre(main_frame, prerender_frame));
  EXPECT_THAT(
      PageContextRegistry::AllMainRenderFrameHostsFromContext(resource_context),
      UnorderedElementsAre(main_frame, prerender_frame));

  base::WeakPtr<PageNode> page_node =
      PerformanceManager::GetPageNodeForRenderFrameHost(main_frame);
  base::WeakPtr<FrameNode> main_frame_node =
      PerformanceManager::GetFrameNodeForRenderFrameHost(main_frame);
  base::WeakPtr<FrameNode> prerender_frame_node =
      PerformanceManager::GetFrameNodeForRenderFrameHost(prerender_frame);
  RunInGraphWithRegistry<PageContextRegistry>(
      [&](const PageContextRegistry* registry) {
        // Validate that Performance Manager still uses the same PageNode for
        // prerendering pages. (See https://crbug.com/1211368.)
        ASSERT_TRUE(page_node);
        ASSERT_TRUE(main_frame_node);
        ASSERT_TRUE(prerender_frame_node);
        EXPECT_EQ(main_frame_node->GetPageNode(), page_node.get());
        EXPECT_EQ(prerender_frame_node->GetPageNode(), page_node.get());
        EXPECT_EQ(page_node->GetMainFrameNode(), main_frame_node.get());

        EXPECT_EQ(page_context, page_node->GetResourceContext());
        EXPECT_EQ(page_node.get(),
                  registry->GetPageNodeForContext(page_context));
        EXPECT_EQ(page_node.get(),
                  registry->GetPageNodeForContext(resource_context));
      });

  // Navigate to the prerendered URL, making the prerenderer frame current. The
  // registry won't be updated until the PM sequence updates the status.
  prerender_helper_.NavigatePrimaryPage(prerender_url_);
  RunInGraph([&](Graph*) {
    ASSERT_TRUE(page_node);
    ASSERT_TRUE(prerender_frame_node);
    EXPECT_EQ(page_node->GetMainFrameNode(), prerender_frame_node.get());
  });

  EXPECT_EQ(
      prerender_frame,
      PageContextRegistry::CurrentMainRenderFrameHostFromContext(page_context));
  EXPECT_THAT(
      PageContextRegistry::AllMainRenderFrameHostsFromContext(page_context),
      UnorderedElementsAre(main_frame, prerender_frame));

  DeleteNodes();

  // WebContents was cleared by DeleteNodes().
  EXPECT_FALSE(weak_web_contents_);
  EXPECT_EQ(absl::nullopt, PageContextRegistry::ContextForWebContents(
                               weak_web_contents_.get()));
  EXPECT_EQ(absl::nullopt,
            PageContextRegistry::ContextForRenderFrameHostId(main_frame_id_));
  EXPECT_EQ(nullptr, PageContextRegistry::WebContentsFromContext(page_context));
  EXPECT_EQ(nullptr, PageContextRegistry::CurrentMainRenderFrameHostFromContext(
                         page_context));
  EXPECT_THAT(
      PageContextRegistry::AllMainRenderFrameHostsFromContext(page_context),
      IsEmpty());
  RunInGraphWithRegistry<PageContextRegistry>(
      [&](const PageContextRegistry* registry) {
        ASSERT_FALSE(page_node);
        EXPECT_EQ(nullptr, registry->GetPageNodeForContext(page_context));
        EXPECT_EQ(nullptr, registry->GetPageNodeForContext(resource_context));
      });
}

IN_PROC_BROWSER_TEST_F(PageContextRegistryTest, InvalidPageContexts) {
  constexpr auto kInvalidId = content::GlobalRenderFrameHostId();

  EXPECT_EQ(absl::nullopt, PageContextRegistry::ContextForWebContents(nullptr));
  EXPECT_EQ(absl::nullopt,
            PageContextRegistry::ContextForRenderFrameHost(nullptr));
  EXPECT_EQ(absl::nullopt,
            PageContextRegistry::ContextForRenderFrameHostId(kInvalidId));

  // Load a single frame to get a non-PageNode ResourceContext.
  EXPECT_TRUE(content::NavigateToURL(
      web_contents(), embedded_test_server()->GetURL("a.com", "/a.html")));
  ASSERT_TRUE(web_contents()->GetPrimaryMainFrame());
  base::WeakPtr<ProcessNode> process_node =
      PerformanceManager::GetProcessNodeForRenderProcessHost(
          web_contents()->GetPrimaryMainFrame()->GetProcess());

  ResourceContext invalid_resource_context;
  RunInGraphWithRegistry<PageContextRegistry>(
      [&](const PageContextRegistry* registry) {
        ASSERT_TRUE(process_node);
        invalid_resource_context = process_node->GetResourceContext();

        EXPECT_EQ(nullptr,
                  registry->GetPageNodeForContext(invalid_resource_context));
      });

  EXPECT_EQ(nullptr, PageContextRegistry::WebContentsFromContext(
                         invalid_resource_context));
  EXPECT_EQ(nullptr, PageContextRegistry::CurrentMainRenderFrameHostFromContext(
                         invalid_resource_context));
  EXPECT_THAT(PageContextRegistry::AllMainRenderFrameHostsFromContext(
                  invalid_resource_context),
              IsEmpty());
}

IN_PROC_BROWSER_TEST_F(PageContextRegistryDisabledTest, UIThreadAccess) {
  CreateNodes();

  // Static accessors should safely return null if PageContextRegistry is not
  // enabled in Performance Manager.
  EXPECT_EQ(absl::nullopt,
            PageContextRegistry::ContextForWebContents(web_contents()));
  EXPECT_EQ(absl::nullopt,
            PageContextRegistry::ContextForRenderFrameHost(
                content::RenderFrameHost::FromID(main_frame_id_)));
  EXPECT_EQ(absl::nullopt,
            PageContextRegistry::ContextForRenderFrameHostId(main_frame_id_));

  const auto kDummyPageContext = PageContext();
  const ResourceContext kDummyResourceContext = kDummyPageContext;

  EXPECT_EQ(nullptr,
            PageContextRegistry::WebContentsFromContext(kDummyPageContext));
  EXPECT_EQ(nullptr,
            PageContextRegistry::WebContentsFromContext(kDummyResourceContext));
  EXPECT_EQ(nullptr, PageContextRegistry::CurrentMainRenderFrameHostFromContext(
                         kDummyPageContext));
  EXPECT_EQ(nullptr, PageContextRegistry::CurrentMainRenderFrameHostFromContext(
                         kDummyResourceContext));
  EXPECT_THAT(PageContextRegistry::AllMainRenderFrameHostsFromContext(
                  kDummyPageContext),
              IsEmpty());
  EXPECT_THAT(PageContextRegistry::AllMainRenderFrameHostsFromContext(
                  kDummyResourceContext),
              IsEmpty());
}

}  // namespace

}  // namespace performance_manager::resource_attribution
