// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/omnibox/chrome_omnibox_navigation_observer.h"

#include <unordered_map>
#include <vector>

#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/search_engines/template_url_service_factory_test_util.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_data.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/web_contents.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"

using network::TestURLLoaderFactory;

// A trival ChromeOmniboxNavigationObserver that keeps track of whether
// CreateAlternateNavInfoBar() has been called.
class MockChromeOmniboxNavigationObserver
    : public ChromeOmniboxNavigationObserver {
 public:
  MockChromeOmniboxNavigationObserver(
      Profile* profile,
      const base::string16& text,
      const AutocompleteMatch& match,
      const AutocompleteMatch& alternate_nav_match,
      bool* displayed_infobar)
      : ChromeOmniboxNavigationObserver(profile,
                                        text,
                                        match,
                                        alternate_nav_match),
        displayed_infobar_(displayed_infobar) {
    *displayed_infobar_ = false;
  }

 protected:
  void CreateAlternateNavInfoBar() override { *displayed_infobar_ = true; }

 private:
  // True if CreateAlternateNavInfoBar was called.  This cannot be kept in
  // memory within this class because this class is automatically deleted when
  // all fetchers finish (before the test can query this value), hence the
  // pointer.
  bool* displayed_infobar_;

  DISALLOW_COPY_AND_ASSIGN(MockChromeOmniboxNavigationObserver);
};

class ChromeOmniboxNavigationObserverTest
    : public ChromeRenderViewHostTestHarness {
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
  static base::string16 auto_generated_search_keyword() {
    return base::ASCIIToUTF16("auto_generated_search_keyword");
  }
  static base::string16 non_auto_generated_search_keyword() {
    return base::ASCIIToUTF16("non_auto_generated_search_keyword");
  }
  static base::string16 default_search_keyword() {
    return base::ASCIIToUTF16("default_search_keyword");
  }
  static base::string16 prepopulated_search_keyword() {
    return base::ASCIIToUTF16("prepopulated_search_keyword");
  }
  static base::string16 policy_search_keyword() {
    return base::ASCIIToUTF16("policy_search_keyword");
  }

 private:
  // ChromeRenderViewHostTestHarness:
  void SetUp() override;

  DISALLOW_COPY_AND_ASSIGN(ChromeOmniboxNavigationObserverTest);
};

void ChromeOmniboxNavigationObserverTest::SetUp() {
  ChromeRenderViewHostTestHarness::SetUp();
  InfoBarService::CreateForWebContents(web_contents());

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
  policy_turl.created_by_policy = true;
  factory_util.model()->Add(std::make_unique<TemplateURL>(policy_turl));
}

TEST_F(ChromeOmniboxNavigationObserverTest, LoadStateAfterPendingNavigation) {
  std::unique_ptr<ChromeOmniboxNavigationObserver> observer =
      std::make_unique<ChromeOmniboxNavigationObserver>(
          profile(), base::ASCIIToUTF16("test text"), AutocompleteMatch(),
          AutocompleteMatch());
  EXPECT_EQ(ChromeOmniboxNavigationObserver::LOAD_NOT_SEEN,
            observer->load_state());

  std::unique_ptr<content::NavigationEntry> entry =
      content::NavigationController::CreateNavigationEntry(
          GURL(), content::Referrer(), base::nullopt,
          ui::PAGE_TRANSITION_FROM_ADDRESS_BAR, false, std::string(), profile(),
          nullptr /* blob_url_loader_factory */);

  content::NotificationService::current()->Notify(
      content::NOTIFICATION_NAV_ENTRY_PENDING,
      content::Source<content::NavigationController>(navigation_controller()),
      content::Details<content::NavigationEntry>(entry.get()));

  // A pending navigation notification should synchronously update the load
  // state to pending.
  EXPECT_EQ(ChromeOmniboxNavigationObserver::LOAD_PENDING,
            observer->load_state());
}

TEST_F(ChromeOmniboxNavigationObserverTest, DeleteBrokenCustomSearchEngines) {
  struct TestData {
    base::string16 keyword;
    int status_code;
    bool expect_exists;
  };
  std::vector<TestData> cases = {
      {auto_generated_search_keyword(), 200, true},
      {auto_generated_search_keyword(), 404, false},
      {non_auto_generated_search_keyword(), 404, true},
      {default_search_keyword(), 404, true},
      {prepopulated_search_keyword(), 404, true},
      {policy_search_keyword(), 404, true}};

  base::string16 query = base::ASCIIToUTF16(" text");
  for (size_t i = 0; i < cases.size(); ++i) {
    SCOPED_TRACE("case #" + base::NumberToString(i));
    // The keyword should always exist at the beginning.
    EXPECT_TRUE(model()->GetTemplateURLForKeyword(cases[i].keyword) != nullptr);

    AutocompleteMatch match;
    match.keyword = cases[i].keyword;
    // |observer| gets deleted by observer->NavigationEntryCommitted().
    ChromeOmniboxNavigationObserver* observer =
        new ChromeOmniboxNavigationObserver(profile(), cases[i].keyword + query,
                                            match, AutocompleteMatch());
    auto navigation_entry =
        content::NavigationController::CreateNavigationEntry(
            GURL(), content::Referrer(), base::nullopt,
            ui::PAGE_TRANSITION_FROM_ADDRESS_BAR, false, std::string(),
            profile(), nullptr /* blob_url_loader_factory */);
    content::LoadCommittedDetails details;
    details.http_status_code = cases[i].status_code;
    details.entry = navigation_entry.get();
    observer->NavigationEntryCommitted(details);
    EXPECT_EQ(cases[i].expect_exists,
              model()->GetTemplateURLForKeyword(cases[i].keyword) != nullptr);
  }

  // Also run a URL navigation that results in a 404 through the system to make
  // sure nothing crashes for regular URL navigations.
  // |observer| gets deleted by observer->NavigationEntryCommitted().
  ChromeOmniboxNavigationObserver* observer =
      new ChromeOmniboxNavigationObserver(
          profile(), base::ASCIIToUTF16("url navigation"), AutocompleteMatch(),
          AutocompleteMatch());
  auto navigation_entry = content::NavigationController::CreateNavigationEntry(
      GURL(), content::Referrer(), base::nullopt,
      ui::PAGE_TRANSITION_FROM_ADDRESS_BAR, false, std::string(), profile(),
      nullptr /* blob_url_loader_factory */);
  content::LoadCommittedDetails details;
  details.http_status_code = 404;
  details.entry = navigation_entry.get();
  observer->NavigationEntryCommitted(details);
}

TEST_F(ChromeOmniboxNavigationObserverTest, AlternateNavInfoBar) {
  TestURLLoaderFactory test_url_loader_factory;
  scoped_refptr<network::SharedURLLoaderFactory> shared_factory =
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          &test_url_loader_factory);

  const int kNetError = 0;
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
  for (size_t i = 0; i < base::size(cases); ++i) {
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
    network::TestURLLoaderFactory::ResponseProduceFlags response_flags =
        network::TestURLLoaderFactory::kResponseDefault;

    if (response.http_response_code == kNoResponse) {
      response_flags =
          TestURLLoaderFactory::kResponseOnlyRedirectsNoDestination;
    } else if (response.http_response_code == kNetError) {
      net_status = network::URLLoaderCompletionStatus(net::ERR_FAILED);
    } else {
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
    bool displayed_infobar;
    ChromeOmniboxNavigationObserver* observer =
        new MockChromeOmniboxNavigationObserver(
            profile(), base::ASCIIToUTF16("example"), AutocompleteMatch(),
            alternate_nav_match, &displayed_infobar);
    observer->SetURLLoaderFactoryForTesting(shared_factory);

    // Send the observer NAV_ENTRY_PENDING to get the URL fetcher to start.
    auto navigation_entry =
        content::NavigationController::CreateNavigationEntry(
            GURL(), content::Referrer(), base::nullopt,
            ui::PAGE_TRANSITION_FROM_ADDRESS_BAR, false, std::string(),
            profile(), nullptr /* blob_url_loader_factory */);
    content::NotificationService::current()->Notify(
        content::NOTIFICATION_NAV_ENTRY_PENDING,
        content::Source<content::NavigationController>(navigation_controller()),
        content::Details<content::NavigationEntry>(navigation_entry.get()));

    // Make sure the fetcher(s) have finished.
    base::RunLoop().RunUntilIdle();

    // Send the observer NavigationEntryCommitted() to get it to display the
    // infobar if needed.
    content::LoadCommittedDetails details;
    details.http_status_code = 200;
    details.entry = navigation_entry.get();
    observer->NavigationEntryCommitted(details);

    // See if AlternateNavInfoBarDelegate::Create() was called.
    EXPECT_EQ(test_case.expected_alternate_nav_bar_shown, displayed_infobar);
  }
}
