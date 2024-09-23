// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ui/omnibox/chrome_omnibox_navigation_observer.h"

#include <unordered_map>
#include <vector>

#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/search_engines/template_url_service_factory_test_util.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_data.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/navigation_simulator.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"

using network::TestURLLoaderFactory;

class ChromeOmniboxNavigationObserverTest
    : public ChromeRenderViewHostTestHarness {
 public:
  ChromeOmniboxNavigationObserverTest(
      const ChromeOmniboxNavigationObserverTest&) = delete;
  ChromeOmniboxNavigationObserverTest& operator=(
      const ChromeOmniboxNavigationObserverTest&) = delete;

 protected:
  ChromeOmniboxNavigationObserverTest() {}
  ~ChromeOmniboxNavigationObserverTest() override {}

  content::NavigationController* navigation_controller() {
    return &(web_contents()->GetController());
  }

  TemplateURLService* model() {
    return TemplateURLServiceFactory::GetForProfile(profile());
  }

  // Functions that return the name of certain search keywords that are part
  // of the TemplateURLService attached to this profile.
  static std::u16string auto_generated_search_keyword() {
    return u"auto_generated_search_keyword";
  }
  static std::u16string non_auto_generated_search_keyword() {
    return u"non_auto_generated_search_keyword";
  }
  static std::u16string default_search_keyword() {
    return u"default_search_keyword";
  }
  static std::u16string prepopulated_search_keyword() {
    return u"prepopulated_search_keyword";
  }
  static std::u16string policy_search_keyword() {
    return u"policy_search_keyword";
  }
  static std::u16string starter_pack_keyword() {
    return u"starter_pack_keyword";
  }

 private:
  // ChromeRenderViewHostTestHarness:
  void SetUp() override;
};

void ChromeOmniboxNavigationObserverTest::SetUp() {
  ChromeRenderViewHostTestHarness::SetUp();
  infobars::ContentInfoBarManager::CreateForWebContents(web_contents());

  // Set up a series of search engines for later testing.
  TemplateURLServiceFactoryTestUtil factory_util(profile());
  factory_util.VerifyLoad();

  TemplateURLData auto_gen_turl;
  auto_gen_turl.SetKeyword(auto_generated_search_keyword());
  auto_gen_turl.safe_for_autoreplace = true;
  factory_util.model()->Add(std::make_unique<TemplateURL>(auto_gen_turl));

  TemplateURLData non_auto_gen_turl;
  non_auto_gen_turl.SetKeyword(non_auto_generated_search_keyword());
  factory_util.model()->Add(std::make_unique<TemplateURL>(non_auto_gen_turl));

  TemplateURLData default_turl;
  default_turl.SetKeyword(default_search_keyword());
  factory_util.model()->SetUserSelectedDefaultSearchProvider(
      factory_util.model()->Add(std::make_unique<TemplateURL>(default_turl)));

  TemplateURLData prepopulated_turl;
  prepopulated_turl.SetKeyword(prepopulated_search_keyword());
  prepopulated_turl.prepopulate_id = 1;
  factory_util.model()->Add(std::make_unique<TemplateURL>(prepopulated_turl));

  TemplateURLData policy_turl;
  policy_turl.SetKeyword(policy_search_keyword());
  policy_turl.created_by_policy =
      TemplateURLData::CreatedByPolicy::kDefaultSearchProvider;
  factory_util.model()->Add(std::make_unique<TemplateURL>(policy_turl));

  TemplateURLData starter_pack_turl;
  starter_pack_turl.SetKeyword(starter_pack_keyword());
  starter_pack_turl.starter_pack_id = 1;
  factory_util.model()->Add(std::make_unique<TemplateURL>(starter_pack_turl));
}

namespace {

scoped_refptr<net::HttpResponseHeaders> GetHeadersForResponseCode(int code) {
  if (code == 200) {
    return base::MakeRefCounted<net::HttpResponseHeaders>(
        "HTTP/1.1 200 OK\r\n");
  } else if (code == 404) {
    return base::MakeRefCounted<net::HttpResponseHeaders>(
        "HTTP/1.1 404 Not Found\r\n");
  }
  NOTREACHED_IN_MIGRATION();
  return nullptr;
}

void WriteMojoMessage(const mojo::ScopedDataPipeProducerHandle& handle,
                      std::string message) {
  size_t actually_written_bytes = 0;
  ASSERT_EQ(MOJO_RESULT_OK, handle->WriteData(base::as_byte_span(message),
                                              MOJO_WRITE_DATA_FLAG_NONE,
                                              actually_written_bytes));
  ASSERT_EQ(message.size(), actually_written_bytes);
}

}  // namespace

TEST_F(ChromeOmniboxNavigationObserverTest, DeleteBrokenCustomSearchEngines) {
  // The actual URL doesn't matter for this test as long as it's valid.
  struct TestData {
    std::u16string keyword;
    int status_code;
    bool expect_exists;
  };
  std::vector<TestData> cases = {
      {auto_generated_search_keyword(), 200, true},
      {auto_generated_search_keyword(), 404, false},
      {non_auto_generated_search_keyword(), 404, true},
      {default_search_keyword(), 404, true},
      {prepopulated_search_keyword(), 404, true},
      {policy_search_keyword(), 404, true},
      {starter_pack_keyword(), 404, true}};

  std::u16string query = u" text";
  for (size_t i = 0; i < cases.size(); ++i) {
    SCOPED_TRACE("case #" + base::NumberToString(i));
    // The keyword should always exist at the beginning.
    EXPECT_TRUE(model()->GetTemplateURLForKeyword(cases[i].keyword));

    AutocompleteMatch match;
    match.keyword = cases[i].keyword;
    // Append the case number to the URL to ensure that we are loading a new
    // page.
    auto navigation = content::NavigationSimulator::CreateBrowserInitiated(
        GURL(base::StringPrintf("https://foo.com/%zu", i)), web_contents());
    navigation->SetResponseHeaders(
        GetHeadersForResponseCode(cases[i].status_code));

    // HTTPErrorNavigationThrottle checks whether body is null or not to decide
    // whether to commit an error page or a regular one.
    mojo::ScopedDataPipeProducerHandle producer_handle;
    mojo::ScopedDataPipeConsumerHandle consumer_handle;
    ASSERT_EQ(mojo::CreateDataPipe(nullptr, producer_handle, consumer_handle),
              MOJO_RESULT_OK);
    navigation->SetResponseBody(std::move(consumer_handle));
    WriteMojoMessage(producer_handle, "data");

    navigation->Start();
    ChromeOmniboxNavigationObserver::CreateForTesting(
        navigation->GetNavigationHandle(), profile(), cases[i].keyword + query,
        match, AutocompleteMatch(), nullptr, base::DoNothing());

    navigation->Commit();

    EXPECT_EQ(cases[i].expect_exists,
              model()->GetTemplateURLForKeyword(cases[i].keyword) != nullptr);
  }
}

TEST_F(ChromeOmniboxNavigationObserverTest, AlternateNavInfoBar) {
  TestURLLoaderFactory test_url_loader_factory;
  scoped_refptr<network::SharedURLLoaderFactory> shared_factory =
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          &test_url_loader_factory);

  const int kNetError = net::ERR_FAILED;
  const int kNoResponse = -1;
  struct Response {
    const std::vector<std::string> urls;  // If more than one, 301 between them.
    // The final status code to return after all the redirects, or one of
    // kNetError or kNoResponse.
    const int http_response_code;
    std::string content;
  };

  // All of these test cases assume the alternate nav URL is http://example/.
  struct Case {
    const Response response;
    const bool expected_alternate_nav_bar_shown;
  } cases[] = {
      // The only response provided is a net error.
      {{{"http://example/"}, kNetError}, false},
      // The response connected to a valid page.
      {{{"http://example/"}, 200}, true},
      // A non-empty page, despite the HEAD.
      {{{"http://example/"}, 200, "Content"}, true},
      // The response connected to an error page.
      {{{"http://example/"}, 404}, false},

      // The response redirected to same host, just http->https, with a path
      // change as well.  In this case the second URL should not fetched; Chrome
      // will optimistically assume the destination will return a valid page and
      // display the infobar.
      {{{"http://example/", "https://example/path"}, kNoResponse}, true},
      // Ditto, making sure it still holds when the final destination URL
      // returns a valid status code.
      {{{"http://example/", "https://example/path"}, 200}, true},

      // The response redirected to an entirely different host.  In these cases,
      // no URL should be fetched against this second host; again Chrome will
      // optimistically assume the destination will return a valid page and
      // display the infobar.
      {{{"http://example/", "http://new-destination/"}, kNoResponse}, true},
      // Ditto, making sure it still holds when the final destination URL
      // returns a valid status code.
      {{{"http://example/", "http://new-destination/"}, 200}, true},

      // The response redirected to same host, just http->https, with no other
      // changes.  In these cases, Chrome will fetch the second URL.
      // The second URL response returned a valid page.
      {{{"http://example/", "https://example/"}, 200}, true},
      // The second URL response returned an error page.
      {{{"http://example/", "https://example/"}, 404}, false},
      // The second URL response returned a net error.
      {{{"http://example/", "https://example/"}, kNetError}, false},
      // The second URL response redirected again.
      {{{"http://example/", "https://example/", "https://example/root"},
        kNoResponse},
       true},
  };
  for (size_t i = 0; i < std::size(cases); ++i) {
    SCOPED_TRACE("case #" + base::NumberToString(i));
    const Case& test_case = cases[i];
    const Response& response = test_case.response;

    // Set the URL request responses.
    test_url_loader_factory.ClearResponses();

    // Compute URL redirect chain.
    TestURLLoaderFactory::Redirects redirects;
    for (size_t dest = 1; dest < response.urls.size(); ++dest) {
      net::RedirectInfo redir_info;
      redir_info.new_url = GURL(response.urls[dest]);
      redir_info.status_code = net::HTTP_MOVED_PERMANENTLY;
      auto redir_head =
          network::CreateURLResponseHead(net::HTTP_MOVED_PERMANENTLY);
      redirects.push_back({redir_info, std::move(redir_head)});
    }

    // Fill in final response.
    network::mojom::URLResponseHeadPtr http_head =
        network::mojom::URLResponseHead::New();
    network::URLLoaderCompletionStatus net_status;
    if (response.http_response_code == kNetError) {
      net_status = network::URLLoaderCompletionStatus(net::ERR_FAILED);
    } else if (response.http_response_code != kNoResponse) {
      net_status = network::URLLoaderCompletionStatus(net::OK);
      http_head = network::CreateURLResponseHead(
          static_cast<net::HttpStatusCode>(response.http_response_code));
    }

    test_url_loader_factory.AddResponse(GURL(response.urls[0]),
                                        std::move(http_head), response.content,
                                        net_status, std::move(redirects));

    // Create the alternate nav match and the observer.
    // |observer| gets deleted automatically after all fetchers complete.
    AutocompleteMatch alternate_nav_match;
    alternate_nav_match.destination_url = GURL("http://example/");
    auto navigation = content::NavigationSimulator::CreateBrowserInitiated(
        GURL(base::StringPrintf("https://foo.com/%zu", i)), web_contents());
    bool displayed_infobar = false;

    navigation->Start();
    ChromeOmniboxNavigationObserver::CreateForTesting(
        navigation->GetNavigationHandle(), profile(), u"example",
        AutocompleteMatch(), alternate_nav_match, shared_factory.get(),
        base::BindLambdaForTesting(
            [&](ChromeOmniboxNavigationObserver* observer) {
              displayed_infobar = true;
            }));

    // Make sure the fetcher(s) have finished.
    base::RunLoop().RunUntilIdle();

    if (test_case.response.http_response_code != kNetError) {
      navigation->Commit();
    } else {
      navigation->Fail(kNetError);
      navigation->CommitErrorPage();
    }

    // See if AlternateNavInfoBarDelegate::Create() was called.
    EXPECT_EQ(test_case.expected_alternate_nav_bar_shown, displayed_infobar);
  }
}
