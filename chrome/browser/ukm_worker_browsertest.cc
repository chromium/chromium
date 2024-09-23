// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/privacy_budget/scoped_privacy_budget_config.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/platform_browser_test.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/browser/worker_type.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class UkmWorkerBrowserTest : public PlatformBrowserTest {
 public:
  UkmWorkerBrowserTest()
      : privacy_budget_config_(
            test::ScopedPrivacyBudgetConfig::Presets::kEnableRandomSampling) {}

  void SetUpOnMainThread() override {
    test_ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();
  }

  content::WebContents* web_contents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

  const ukm::TestAutoSetUkmRecorder& test_ukm_recorder() const {
    return *test_ukm_recorder_;
  }

 private:
  test::ScopedPrivacyBudgetConfig privacy_budget_config_;
  std::unique_ptr<ukm::TestAutoSetUkmRecorder> test_ukm_recorder_;
};

}  // namespace

IN_PROC_BROWSER_TEST_F(UkmWorkerBrowserTest,
                       SharedWorker_DocumentClientIdIsPlumbed) {
  using DocumentCreatedEntry = ukm::builders::DocumentCreated;
  using AddedEntry = ukm::builders::Worker_ClientAdded;

  embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
      [](const net::test_server::HttpRequest& request)
          -> std::unique_ptr<net::test_server::HttpResponse> {
        if (request.GetURL().path() != "/shared_worker_script")
          return nullptr;
        auto response = std::make_unique<net::test_server::BasicHttpResponse>();
        response->set_content_type("text/javascript");
        response->set_content(
            R"(self.onconnect = e => { e.ports[0].postMessage('DONE'); };)");
        return response;
      }));
  ASSERT_TRUE(embedded_test_server()->Start());
  content::DOMMessageQueue messages(web_contents());

  ASSERT_TRUE(content::NavigateToURL(
      web_contents(), embedded_test_server()->GetURL(
                          "/workers/create_shared_worker.html?worker_url=/"
                          "shared_worker_script")));

  // Wait until the worker script is loaded and executed, to ensure the UKM is
  // logged.
  EXPECT_EQ("DONE", content::EvalJs(
                        browser()->tab_strip_model()->GetActiveWebContents(),
                        "waitForMessage();"));

  std::vector<raw_ptr<const ukm::mojom::UkmEntry, VectorExperimental>>
      doc_created_entries = test_ukm_recorder().GetEntriesByName(
          DocumentCreatedEntry::kEntryName);
  EXPECT_EQ(1u, doc_created_entries.size());
  const ukm::SourceId document_source_id = doc_created_entries[0]->source_id;

  // Check that we got the WorkerClientConnected event.
  std::vector<raw_ptr<const ukm::mojom::UkmEntry, VectorExperimental>>
      connected_entries =
          test_ukm_recorder().GetEntriesByName(AddedEntry::kEntryName);
  EXPECT_EQ(1u, connected_entries.size());
  const ukm::SourceId client_source_id = *test_ukm_recorder().GetEntryMetric(
      connected_entries[0], AddedEntry::kClientSourceIdName);
  const ukm::SourceId worker_source_id = connected_entries[0]->source_id;
  const int64_t worker_type = *test_ukm_recorder().GetEntryMetric(
      connected_entries[0], AddedEntry::kWorkerTypeName);

  // Check that we have two source IDs in play (namely that of the
  // client/document, and the SharedWorker) and that they are different.
  EXPECT_EQ(document_source_id, client_source_id);
  EXPECT_NE(worker_source_id, client_source_id);

  EXPECT_EQ(static_cast<int64_t>(WorkerType::kSharedWorker), worker_type);
}

IN_PROC_BROWSER_TEST_F(UkmWorkerBrowserTest,
                       ServiceWorker_DocumentClientIdIsPlumbed) {
  using DocumentCreatedEntry = ukm::builders::DocumentCreated;
  using AddedEntry = ukm::builders::Worker_ClientAdded;

  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(content::NavigateToURL(
      web_contents(), embedded_test_server()->GetURL(
                          "/service_worker/create_service_worker.html")));

  // Wait until the worker script is loaded and executed, to ensure the UKM is
  // logged.
  EXPECT_EQ("DONE", EvalJs(web_contents(),
                           "register('fetch_event_respond_with_fetch.js');"));

  std::vector<raw_ptr<const ukm::mojom::UkmEntry, VectorExperimental>>
      doc_created_entries = test_ukm_recorder().GetEntriesByName(
          DocumentCreatedEntry::kEntryName);
  ASSERT_EQ(1u, doc_created_entries.size());
  const ukm::SourceId document_source_id = doc_created_entries[0]->source_id;

  // Check that we got the Worker.ClientAdded event.
  std::vector<raw_ptr<const ukm::mojom::UkmEntry, VectorExperimental>>
      connected_entries =
          test_ukm_recorder().GetEntriesByName(AddedEntry::kEntryName);
  ASSERT_EQ(1u, connected_entries.size());
  const ukm::SourceId client_source_id = *test_ukm_recorder().GetEntryMetric(
      connected_entries[0], AddedEntry::kClientSourceIdName);
  const ukm::SourceId worker_source_id = connected_entries[0]->source_id;
  const int64_t worker_type = *test_ukm_recorder().GetEntryMetric(
      connected_entries[0], AddedEntry::kWorkerTypeName);

  // Check that we have two source IDs in play (namely that of the
  // client/document, and the ServiceWorker) and that they are different.
  EXPECT_EQ(document_source_id, client_source_id);
  EXPECT_NE(worker_source_id, client_source_id);

  EXPECT_EQ(static_cast<int64_t>(WorkerType::kServiceWorker), worker_type);
}

IN_PROC_BROWSER_TEST_F(UkmWorkerBrowserTest,
                       ServiceWorker_DedicatedWorkerClientIdIsIgnored) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(content::NavigateToURL(
      web_contents(), embedded_test_server()->GetURL(
                          "/service_worker/create_service_worker.html")));
  EXPECT_EQ("DONE", EvalJs(web_contents(),
                           "register('fetch_event_respond_with_fetch.js');"));

  // Wait until the worker script is loaded and executed, to ensure the UKM is
  // logged.
  EXPECT_EQ("loaded", EvalJs(web_contents(), R"SCRIPT(
      const worker = new Worker('../workers/dedicated_worker.js');
      const onmessage_promise = new Promise(r => worker.onmessage = r);
      async function waitForMessage() {
        const message = await onmessage_promise;
        return message.data;
      }
      waitForMessage();
  )SCRIPT"));

  // Check that we only have the single Worker.ClientAdded event (for the
  // document).
  std::vector<raw_ptr<const ukm::mojom::UkmEntry, VectorExperimental>>
      connected_entries = test_ukm_recorder().GetEntriesByName(
          ukm::builders::Worker_ClientAdded::kEntryName);
  EXPECT_EQ(1u, connected_entries.size());
}

IN_PROC_BROWSER_TEST_F(UkmWorkerBrowserTest,
                       ServiceWorker_SharedWorkerClientIdIsPlumbed) {
  using AddedEntry = ukm::builders::Worker_ClientAdded;

  embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
      [](const net::test_server::HttpRequest& request)
          -> std::unique_ptr<net::test_server::HttpResponse> {
        if (request.GetURL().path() != "/shared_worker_script")
          return nullptr;
        auto response = std::make_unique<net::test_server::BasicHttpResponse>();
        response->set_content_type("text/javascript");
        response->set_content(
            R"(self.onconnect = e => { e.ports[0].postMessage('DONE'); };)");
        return response;
      }));

  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(content::NavigateToURL(
      web_contents(), embedded_test_server()->GetURL(
                          "/service_worker/create_service_worker.html")));

  // Wait for the service worker to load.
  EXPECT_EQ("DONE", EvalJs(web_contents(),
                           "register('fetch_event_respond_with_fetch.js');"));

  // Wait for the shared worker to load.
  EXPECT_EQ("DONE", EvalJs(web_contents(), R"SCRIPT(
      const worker = new SharedWorker('/shared_worker_script');
      const onmessage_promise = new Promise(r => worker.port.onmessage = r);
      async function waitForMessage() {
        const message = await onmessage_promise;
        return message.data;
      }
      waitForMessage();
  )SCRIPT"));

  // Check that we have a Worker.ClientAdded event for all three pairs:
  // document-shared worker, document-service worker, and shared worker-service
  // worker.
  std::vector<raw_ptr<const ukm::mojom::UkmEntry, VectorExperimental>>
      connected_entries =
          test_ukm_recorder().GetEntriesByName(AddedEntry::kEntryName);
  ASSERT_EQ(3u, connected_entries.size());

  // Get the document and shared worker ids from the shared worker event.
  ukm::SourceId document_source_id = ukm::kInvalidSourceId;
  ukm::SourceId shared_worker_source_id = ukm::kInvalidSourceId;
  int shared_worker_event_index;
  for (int i = 0; i < 3; i++) {
    const int64_t worker_type = *test_ukm_recorder().GetEntryMetric(
        connected_entries[i], AddedEntry::kWorkerTypeName);
    if (worker_type == static_cast<int64_t>(WorkerType::kSharedWorker)) {
      EXPECT_EQ(document_source_id, ukm::kInvalidSourceId);
      EXPECT_EQ(shared_worker_source_id, ukm::kInvalidSourceId);
      document_source_id = *test_ukm_recorder().GetEntryMetric(
          connected_entries[i], AddedEntry::kClientSourceIdName);
      shared_worker_source_id = connected_entries[i]->source_id;
      shared_worker_event_index = i;
    }
  }
  ASSERT_NE(document_source_id, ukm::kInvalidSourceId);
  ASSERT_NE(shared_worker_source_id, ukm::kInvalidSourceId);
  EXPECT_NE(document_source_id, shared_worker_source_id);

  // Remove the shared worker event to leave just the service worker events.
  connected_entries.erase(connected_entries.begin() +
                          shared_worker_event_index);

  // Check the events contain the expected information without enforcing any
  // ordering.
  ukm::SourceId service_worker_source_id = connected_entries[0]->source_id;
  EXPECT_EQ(service_worker_source_id, connected_entries[1]->source_id);

  EXPECT_EQ(*test_ukm_recorder().GetEntryMetric(connected_entries[0],
                                                AddedEntry::kWorkerTypeName),
            static_cast<int64_t>(WorkerType::kServiceWorker));
  EXPECT_EQ(*test_ukm_recorder().GetEntryMetric(connected_entries[1],
                                                AddedEntry::kWorkerTypeName),
            static_cast<int64_t>(WorkerType::kServiceWorker));

  ukm::SourceId client_source_id_1 = *test_ukm_recorder().GetEntryMetric(
      connected_entries[0], AddedEntry::kClientSourceIdName);
  ukm::SourceId client_source_id_2 = *test_ukm_recorder().GetEntryMetric(
      connected_entries[1], AddedEntry::kClientSourceIdName);

  EXPECT_EQ(
      std::set<ukm::SourceId>({document_source_id, shared_worker_source_id}),
      std::set<ukm::SourceId>({client_source_id_1, client_source_id_2}));
}
