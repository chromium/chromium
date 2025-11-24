// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
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
  UkmWorkerBrowserTest() = default;

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
        if (request.GetURL().GetPath() != "/shared_worker_script") {
          return nullptr;
        }
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
