// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browsing_data/content/cookie_helper.h"

#include <optional>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/time/time.h"
#include "content/public/browser/cookie_access_details.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_options.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace browsing_data {
namespace {

net::CookieAccessResultList ConvertCookieListToCookieAccessResultList(
    const net::CookieList& cookie_list) {
  net::CookieAccessResultList result;
  base::ranges::transform(cookie_list, std::back_inserter(result),
                          [](const net::CanonicalCookie& cookie) {
                            return net::CookieWithAccessResult{cookie, {}};
                          });
  return result;
}

// Test expectations for a given cookie.
class CookieExpectation {
 public:
  CookieExpectation() = default;

  bool MatchesCookie(const net::CanonicalCookie& cookie) const {
    if (!domain_.empty() && domain_ != cookie.Domain())
      return false;
    if (!path_.empty() && path_ != cookie.Path())
      return false;
    if (!name_.empty() && name_ != cookie.Name())
      return false;
    if (!value_.empty() && value_ != cookie.Value())
      return false;
    return true;
  }

  GURL source_;
  std::string domain_;
  std::string path_;
  std::string name_;
  std::string value_;
  bool matched_ = false;
};

// Matches a CookieExpectation against a Cookie.
class CookieMatcher {
 public:
  explicit CookieMatcher(const net::CanonicalCookie& cookie)
      : cookie_(cookie) {}
  bool operator()(const CookieExpectation& expectation) {
    return expectation.MatchesCookie(cookie_);
  }
  net::CanonicalCookie cookie_;
};

// Unary predicate to determine whether an expectation has been matched.
bool ExpectationIsMatched(const CookieExpectation& expectation) {
  return expectation.matched_;
}

class CookieHelperTest : public testing::Test {
 public:
  CookieHelperTest()
      : testing_browser_context_(
            std::make_unique<content::TestBrowserContext>()) {}

  void SetUp() override { cookie_expectations_.clear(); }

  // Adds an expectation for a cookie that satisfies the given parameters.
  void AddCookieExpectation(const char* source,
                            const char* domain,
                            const char* path,
                            const char* name,
                            const char* value) {
    CookieExpectation matcher;
    if (source)
      matcher.source_ = GURL(source);
    if (domain)
      matcher.domain_ = domain;
    if (path)
      matcher.path_ = path;
    if (name)
      matcher.name_ = name;
    if (value)
      matcher.value_ = value;
    cookie_expectations_.push_back(matcher);
  }

  // Checks the existing expectations, and then clears all existing
  // expectations.
  void CheckCookieExpectations() {
    ASSERT_EQ(cookie_expectations_.size(), cookie_list_.size());

    // For each cookie, look for a matching expectation.
    for (const auto& cookie : cookie_list_) {
      auto match =
          base::ranges::find_if(cookie_expectations_, CookieMatcher(cookie));
      if (match != cookie_expectations_.end())
        match->matched_ = true;
    }

    // Check that each expectation has been matched.
    unsigned long match_count =
        base::ranges::count_if(cookie_expectations_, ExpectationIsMatched);
    EXPECT_EQ(cookie_expectations_.size(), match_count);

    cookie_expectations_.clear();
  }

  void CreateCookiesForTest() {
    std::optional<base::Time> server_time = std::nullopt;
    GURL cookie1_source("https://www.google.com");
    auto cookie1 = net::CanonicalCookie::CreateForTesting(
        cookie1_source, "A=1", base::Time::Now(), server_time);
    GURL cookie2_source("https://www.gmail.google.com");
    auto cookie2 = net::CanonicalCookie::CreateForTesting(
        cookie2_source, "B=1", base::Time::Now(), server_time);

    network::mojom::CookieManager* cookie_manager =
        storage_partition()->GetCookieManagerForBrowserProcess();
    cookie_manager->SetCanonicalCookie(*cookie1, cookie1_source,
                                       net::CookieOptions::MakeAllInclusive(),
                                       base::DoNothing());
    cookie_manager->SetCanonicalCookie(*cookie2, cookie2_source,
                                       net::CookieOptions::MakeAllInclusive(),
                                       base::DoNothing());
  }

  void CreateCookiesForDomainCookieTest() {
    std::optional<base::Time> server_time = std::nullopt;
    GURL cookie_source("https://www.google.com");
    auto cookie1 = net::CanonicalCookie::CreateForTesting(
        cookie_source, "A=1", base::Time::Now(), server_time);
    auto cookie2 = net::CanonicalCookie::CreateForTesting(
        cookie_source, "A=2; Domain=.www.google.com ", base::Time::Now(),
        server_time);

    network::mojom::CookieManager* cookie_manager =
        storage_partition()->GetCookieManagerForBrowserProcess();
    cookie_manager->SetCanonicalCookie(*cookie1, cookie_source,
                                       net::CookieOptions::MakeAllInclusive(),
                                       base::DoNothing());
    cookie_manager->SetCanonicalCookie(*cookie2, cookie_source,
                                       net::CookieOptions::MakeAllInclusive(),
                                       base::DoNothing());
  }

  void FetchCallback(base::OnceClosure quit_closure,
                     const net::CookieList& cookies) {
    cookie_list_ = cookies;

    AddCookieExpectation(nullptr, "www.google.com", nullptr, "A", nullptr);
    AddCookieExpectation(nullptr, "www.gmail.google.com", nullptr, "B",
                         nullptr);
    CheckCookieExpectations();
    std::move(quit_closure).Run();
  }

  void DomainCookieCallback(base::OnceClosure quit_closure,
                            const net::CookieList& cookies) {
    cookie_list_ = cookies;

    AddCookieExpectation(nullptr, "www.google.com", nullptr, "A", "1");
    AddCookieExpectation(nullptr, ".www.google.com", nullptr, "A", "2");
    CheckCookieExpectations();
    std::move(quit_closure).Run();
  }

  void DeleteCallback(base::OnceClosure quit_closure,
                      const net::CookieList& cookies) {
    cookie_list_ = cookies;
    AddCookieExpectation(nullptr, "www.gmail.google.com", nullptr, "B",
                         nullptr);
    CheckCookieExpectations();
    std::move(quit_closure).Run();
  }

  void CannedUniqueCallback(base::OnceClosure quit_closure,
                            const net::CookieList& cookies) {
    cookie_list_ = cookies;
    AddCookieExpectation("http://www.google.com/", "www.google.com", "/", "A",
                         nullptr);
    CheckCookieExpectations();
    std::move(quit_closure).Run();
  }

  void CannedReplaceCookieCallback(base::OnceClosure quit_closure,
                                   const net::CookieList& cookies) {
    cookie_list_ = cookies;
    AddCookieExpectation("http://www.google.com/", "www.google.com", "/", "A",
                         "2");
    AddCookieExpectation("http://www.google.com/", "www.google.com",
                         "/example/0", "A", "4");
    AddCookieExpectation("http://www.google.com/", ".google.com", "/", "A",
                         "6");
    AddCookieExpectation("http://www.google.com/", ".google.com", "/example/1",
                         "A", "8");
    AddCookieExpectation("http://www.google.com/", ".www.google.com", "/", "A",
                         "10");
    CheckCookieExpectations();
    std::move(quit_closure).Run();
  }

  void CannedDomainCookieCallback(base::OnceClosure quit_closure,
                                  const net::CookieList& cookies) {
    cookie_list_ = cookies;
    AddCookieExpectation("http://www.google.com/", "www.google.com", nullptr,
                         "A", nullptr);
    AddCookieExpectation("http://www.google.com/", ".www.google.com", nullptr,
                         "A", nullptr);
    CheckCookieExpectations();
    std::move(quit_closure).Run();
  }

  void CannedDifferentFramesCallback(base::OnceClosure quit_closure,
                                     const net::CookieList& cookie_list) {
    ASSERT_EQ(3U, cookie_list.size());
    std::move(quit_closure).Run();
  }

  void DeleteCookie(CookieHelper* helper, const std::string& domain) {
    for (const auto& cookie : cookie_list_) {
      if (cookie.Domain() == domain)
        helper->DeleteCookie(cookie);
    }
  }

  content::StoragePartition* storage_partition() {
    return testing_browser_context_->GetDefaultStoragePartition();
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<content::TestBrowserContext> testing_browser_context_;

  std::vector<CookieExpectation> cookie_expectations_;
  net::CookieList cookie_list_;
};

TEST_F(CookieHelperTest, FetchData) {
  CreateCookiesForTest();
  auto cookie_helper = base::MakeRefCounted<CookieHelper>(storage_partition(),
                                                          base::NullCallback());

  base::RunLoop run_loop;
  cookie_helper->StartFetching(base::BindOnce(&CookieHelperTest::FetchCallback,
                                              base::Unretained(this),
                                              run_loop.QuitClosure()));
  run_loop.Run();
}

TEST_F(CookieHelperTest, DomainCookie) {
  CreateCookiesForDomainCookieTest();
  auto cookie_helper = base::MakeRefCounted<CookieHelper>(storage_partition(),
                                                          base::NullCallback());
  base::RunLoop run_loop;
  cookie_helper->StartFetching(
      base::BindOnce(&CookieHelperTest::DomainCookieCallback,
                     base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();
}

TEST_F(CookieHelperTest, DeleteCookie) {
  CreateCookiesForTest();
  auto cookie_helper = base::MakeRefCounted<CookieHelper>(storage_partition(),
                                                          base::NullCallback());

  {
    base::RunLoop run_loop;
    cookie_helper->StartFetching(
        base::BindOnce(&CookieHelperTest::FetchCallback, base::Unretained(this),
                       run_loop.QuitClosure()));
    run_loop.Run();
  }

  net::CanonicalCookie cookie = cookie_list_[0];
  cookie_helper->DeleteCookie(cookie);

  {
    base::RunLoop run_loop;
    cookie_helper->StartFetching(
        base::BindOnce(&CookieHelperTest::DeleteCallback,
                       base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
  }
}

TEST_F(CookieHelperTest, DeleteCookieWithCallback) {
  CreateCookiesForTest();
  bool disable_delete = true;
  auto cookie_helper = base::MakeRefCounted<CookieHelper>(
      storage_partition(), base::BindLambdaForTesting([&](const GURL& url) {
        return disable_delete;
      }));

  {
    base::RunLoop run_loop;
    cookie_helper->StartFetching(
        base::BindOnce(&CookieHelperTest::FetchCallback, base::Unretained(this),
                       run_loop.QuitClosure()));
    run_loop.Run();
  }

  net::CanonicalCookie cookie = cookie_list_[0];
  cookie_helper->DeleteCookie(cookie);
  {
    base::RunLoop run_loop;
    cookie_helper->StartFetching(
        base::BindOnce(&CookieHelperTest::FetchCallback, base::Unretained(this),
                       run_loop.QuitClosure()));

    run_loop.Run();
  }

  disable_delete = false;
  cookie_helper->DeleteCookie(cookie);
  {
    base::RunLoop run_loop;
    cookie_helper->StartFetching(
        base::BindOnce(&CookieHelperTest::DeleteCallback,
                       base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
  }
}

TEST_F(CookieHelperTest, CannedDeleteCookie) {
  CreateCookiesForTest();
  auto helper = base::MakeRefCounted<CannedCookieHelper>(storage_partition(),
                                                         base::NullCallback());

  ASSERT_TRUE(helper->empty());

  const GURL origin1("http://www.google.com");
  std::unique_ptr<net::CanonicalCookie> cookie1(
      net::CanonicalCookie::CreateForTesting(origin1, "A=1",
                                             base::Time::Now()));
  ASSERT_TRUE(cookie1);
  helper->AddCookies({content::CookieAccessDetails::Type::kChange,
                      origin1,
                      origin1,
                      {{*cookie1}}});
  const GURL origin2("http://www.gmail.google.com");
  std::unique_ptr<net::CanonicalCookie> cookie2(
      net::CanonicalCookie::CreateForTesting(origin2, "B=1",
                                             base::Time::Now()));
  ASSERT_TRUE(cookie2);
  helper->AddCookies({content::CookieAccessDetails::Type::kChange,
                      origin2,
                      origin2,
                      {{*cookie2}}});
  {
    base::RunLoop run_loop;
    helper->StartFetching(base::BindOnce(&CookieHelperTest::FetchCallback,
                                         base::Unretained(this),
                                         run_loop.QuitClosure()));
    run_loop.Run();
  }

  EXPECT_EQ(2u, helper->GetCookieCount());

  DeleteCookie(helper.get(), origin1.host());

  EXPECT_EQ(1u, helper->GetCookieCount());
  {
    base::RunLoop run_loop;
    helper->StartFetching(base::BindOnce(&CookieHelperTest::DeleteCallback,
                                         base::Unretained(this),
                                         run_loop.QuitClosure()));
    run_loop.Run();
  }
}

TEST_F(CookieHelperTest, CannedDomainCookie) {
  const GURL origin("http://www.google.com");
  net::CookieList cookies;

  auto helper = base::MakeRefCounted<CannedCookieHelper>(storage_partition(),
                                                         base::NullCallback());

  ASSERT_TRUE(helper->empty());
  std::unique_ptr<net::CanonicalCookie> cookie1(
      net::CanonicalCookie::CreateForTesting(origin, "A=1", base::Time::Now()));
  ASSERT_TRUE(cookie1);
  helper->AddCookies({content::CookieAccessDetails::Type::kChange,
                      origin,
                      origin,
                      {{*cookie1}}});
  std::unique_ptr<net::CanonicalCookie> cookie2(
      net::CanonicalCookie::CreateForTesting(
          origin, "A=1; Domain=.www.google.com", base::Time::Now()));
  ASSERT_TRUE(cookie2);
  helper->AddCookies({content::CookieAccessDetails::Type::kChange,
                      origin,
                      origin,
                      {{*cookie2}}});
  {
    base::RunLoop run_loop;
    helper->StartFetching(
        base::BindOnce(&CookieHelperTest::CannedDomainCookieCallback,
                       base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
  }

  cookies = cookie_list_;
  helper->Reset();
  ASSERT_TRUE(helper->empty());

  helper->AddCookies({content::CookieAccessDetails::Type::kRead, origin, origin,
                      ConvertCookieListToCookieAccessResultList(cookies), 1u});
  {
    base::RunLoop run_loop;
    helper->StartFetching(
        base::BindOnce(&CookieHelperTest::CannedDomainCookieCallback,
                       base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
  }
}

TEST_F(CookieHelperTest, CannedUnique) {
  const GURL origin("http://www.google.com");

  auto helper = base::MakeRefCounted<CannedCookieHelper>(storage_partition(),
                                                         base::NullCallback());

  ASSERT_TRUE(helper->empty());
  std::unique_ptr<net::CanonicalCookie> cookie(
      net::CanonicalCookie::CreateForTesting(origin, "A=1", base::Time::Now()));
  ASSERT_TRUE(cookie);
  helper->AddCookies({content::CookieAccessDetails::Type::kChange,
                      origin,
                      origin,
                      {{*cookie}}});
  helper->AddCookies({content::CookieAccessDetails::Type::kChange,
                      origin,
                      origin,
                      {{*cookie}}});
  {
    base::RunLoop run_loop;
    helper->StartFetching(
        base::BindOnce(&CookieHelperTest::CannedUniqueCallback,
                       base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
  }

  net::CookieList cookie_list = cookie_list_;
  helper->Reset();
  ASSERT_TRUE(helper->empty());

  helper->AddCookies({content::CookieAccessDetails::Type::kRead, origin, origin,
                      ConvertCookieListToCookieAccessResultList(cookie_list)});
  helper->AddCookies({content::CookieAccessDetails::Type::kRead, origin, origin,
                      ConvertCookieListToCookieAccessResultList(cookie_list)});
  {
    base::RunLoop run_loop;
    helper->StartFetching(
        base::BindOnce(&CookieHelperTest::CannedUniqueCallback,
                       base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
  }
}

TEST_F(CookieHelperTest, CannedReplaceCookie) {
  const GURL origin("http://www.google.com");

  auto helper = base::MakeRefCounted<CannedCookieHelper>(storage_partition(),
                                                         base::NullCallback());

  ASSERT_TRUE(helper->empty());
  std::unique_ptr<net::CanonicalCookie> cookie1(
      net::CanonicalCookie::CreateForTesting(origin, "A=1", base::Time::Now()));
  ASSERT_TRUE(cookie1);
  helper->AddCookies({content::CookieAccessDetails::Type::kChange,
                      origin,
                      origin,
                      {{*cookie1}}});
  std::unique_ptr<net::CanonicalCookie> cookie2(
      net::CanonicalCookie::CreateForTesting(origin, "A=2", base::Time::Now()));
  ASSERT_TRUE(cookie2);
  helper->AddCookies({content::CookieAccessDetails::Type::kChange,
                      origin,
                      origin,
                      {{*cookie2}}});
  std::unique_ptr<net::CanonicalCookie> cookie3(
      net::CanonicalCookie::CreateForTesting(origin, "A=3; Path=/example/0",
                                             base::Time::Now()));
  ASSERT_TRUE(cookie3);
  helper->AddCookies({content::CookieAccessDetails::Type::kChange,
                      origin,
                      origin,
                      {{*cookie3}}});
  std::unique_ptr<net::CanonicalCookie> cookie4(
      net::CanonicalCookie::CreateForTesting(origin, "A=4; Path=/example/0",
                                             base::Time::Now()));
  ASSERT_TRUE(cookie4);
  helper->AddCookies({content::CookieAccessDetails::Type::kChange,
                      origin,
                      origin,
                      {{*cookie4}}});
  std::unique_ptr<net::CanonicalCookie> cookie5(
      net::CanonicalCookie::CreateForTesting(origin, "A=5; Domain=google.com",
                                             base::Time::Now()));
  ASSERT_TRUE(cookie5);
  helper->AddCookies({content::CookieAccessDetails::Type::kChange,
                      origin,
                      origin,
                      {{*cookie5}}});
  std::unique_ptr<net::CanonicalCookie> cookie6(
      net::CanonicalCookie::CreateForTesting(origin, "A=6; Domain=google.com",
                                             base::Time::Now()));
  ASSERT_TRUE(cookie6);
  helper->AddCookies({content::CookieAccessDetails::Type::kChange,
                      origin,
                      origin,
                      {{*cookie6}}});
  std::unique_ptr<net::CanonicalCookie> cookie7(
      net::CanonicalCookie::CreateForTesting(
          origin, "A=7; Domain=google.com; Path=/example/1",
          base::Time::Now()));
  ASSERT_TRUE(cookie7);
  helper->AddCookies({content::CookieAccessDetails::Type::kChange,
                      origin,
                      origin,
                      {{*cookie7}}});
  std::unique_ptr<net::CanonicalCookie> cookie8(
      net::CanonicalCookie::CreateForTesting(
          origin, "A=8; Domain=google.com; Path=/example/1",
          base::Time::Now()));
  ASSERT_TRUE(cookie8);
  helper->AddCookies({content::CookieAccessDetails::Type::kChange,
                      origin,
                      origin,
                      {{*cookie8}}});

  std::unique_ptr<net::CanonicalCookie> cookie9(
      net::CanonicalCookie::CreateForTesting(
          origin, "A=9; Domain=www.google.com", base::Time::Now()));
  ASSERT_TRUE(cookie9);
  helper->AddCookies({content::CookieAccessDetails::Type::kChange,
                      origin,
                      origin,
                      {{*cookie9}}});
  std::unique_ptr<net::CanonicalCookie> cookie10(
      net::CanonicalCookie::CreateForTesting(
          origin, "A=10; Domain=www.google.com", base::Time::Now()));
  ASSERT_TRUE(cookie10);
  helper->AddCookies({content::CookieAccessDetails::Type::kChange,
                      origin,
                      origin,
                      {{*cookie10}}});
  {
    base::RunLoop run_loop;
    helper->StartFetching(
        base::BindOnce(&CookieHelperTest::CannedReplaceCookieCallback,
                       base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
  }

  net::CookieList cookie_list = cookie_list_;
  helper->Reset();
  ASSERT_TRUE(helper->empty());

  helper->AddCookies({content::CookieAccessDetails::Type::kRead, origin, origin,
                      ConvertCookieListToCookieAccessResultList(cookie_list)});
  helper->AddCookies({content::CookieAccessDetails::Type::kRead, origin, origin,
                      ConvertCookieListToCookieAccessResultList(cookie_list)});
  {
    base::RunLoop run_loop;
    helper->StartFetching(
        base::BindOnce(&CookieHelperTest::CannedReplaceCookieCallback,
                       base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
  }
}

TEST_F(CookieHelperTest, CannedEmpty) {
  const GURL url_google("http://www.google.com");

  auto helper = base::MakeRefCounted<CannedCookieHelper>(storage_partition(),
                                                         base::NullCallback());

  ASSERT_TRUE(helper->empty());
  std::unique_ptr<net::CanonicalCookie> changed_cookie(
      net::CanonicalCookie::CreateForTesting(url_google, "a=1",
                                             base::Time::Now()));
  ASSERT_TRUE(changed_cookie);
  helper->AddCookies({content::CookieAccessDetails::Type::kChange,
                      url_google,
                      url_google,
                      {{*changed_cookie}}});
  ASSERT_FALSE(helper->empty());
  helper->Reset();
  ASSERT_TRUE(helper->empty());

  net::CookieList cookies;
  std::unique_ptr<net::CanonicalCookie> cookie(
      net::CanonicalCookie::CreateForTesting(url_google, "a=1",
                                             base::Time::Now()));
  ASSERT_TRUE(cookie);
  cookies.push_back(*cookie);

  helper->AddCookies({content::CookieAccessDetails::Type::kRead, url_google,
                      url_google,
                      ConvertCookieListToCookieAccessResultList(cookies)});
  ASSERT_FALSE(helper->empty());
  helper->Reset();
  ASSERT_TRUE(helper->empty());
}

TEST_F(CookieHelperTest, CannedDifferentFrames) {
  GURL frame1_url("http://www.google.com");
  GURL frame2_url("http://www.google.de");
  GURL request_url("http://www.google.com");

  auto helper = base::MakeRefCounted<CannedCookieHelper>(storage_partition(),
                                                         base::NullCallback());

  ASSERT_TRUE(helper->empty());
  std::unique_ptr<net::CanonicalCookie> cookie1(
      net::CanonicalCookie::CreateForTesting(request_url, "a=1",
                                             base::Time::Now()));
  ASSERT_TRUE(cookie1);
  helper->AddCookies({content::CookieAccessDetails::Type::kChange,
                      frame1_url,
                      request_url,
                      {{*cookie1}}});
  std::unique_ptr<net::CanonicalCookie> cookie2(
      net::CanonicalCookie::CreateForTesting(request_url, "b=1",
                                             base::Time::Now()));
  ASSERT_TRUE(cookie2);
  helper->AddCookies({content::CookieAccessDetails::Type::kChange,
                      frame1_url,
                      request_url,
                      {{*cookie2}}});
  std::unique_ptr<net::CanonicalCookie> cookie3(
      net::CanonicalCookie::CreateForTesting(request_url, "c=1",
                                             base::Time::Now()));
  ASSERT_TRUE(cookie3);
  helper->AddCookies({content::CookieAccessDetails::Type::kChange,
                      frame1_url,
                      request_url,
                      {{*cookie3}}});

  base::RunLoop run_loop;
  helper->StartFetching(
      base::BindOnce(&CookieHelperTest::CannedDifferentFramesCallback,
                     base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();
}

TEST_F(CookieHelperTest, CannedGetCookieCount) {
  // The URL in the omnibox is a frame URL. This is not necessarily the request
  // URL, since websites usually include other resources.
  GURL frame1_url("http://www.google.com");
  GURL frame2_url("http://www.google.de");
  // The request URL used for all cookies that are added to the |helper|.
  GURL request1_url("http://static.google.com/foo/res1.html");
  GURL request2_url("http://static.google.com/bar/res2.html");
  std::string cookie_domain(".www.google.com");

  auto helper = base::MakeRefCounted<CannedCookieHelper>(storage_partition(),
                                                         base::NullCallback());

  // Add two different cookies (distinguished by the tuple [cookie-name,
  // domain-value, path-value]) for a HTTP request to |frame1_url| and verify
  // that the cookie count is increased to two. The set-cookie-string consists
  // only of the cookie-pair. This means that the host and the default-path of
  // the |request_url| are used as domain-value and path-value for the added
  // cookies.
  EXPECT_EQ(0U, helper->GetCookieCount());
  std::unique_ptr<net::CanonicalCookie> cookie1(
      net::CanonicalCookie::CreateForTesting(frame1_url, "A=1",
                                             base::Time::Now()));
  ASSERT_TRUE(cookie1);
  helper->AddCookies({content::CookieAccessDetails::Type::kChange,
                      frame1_url,
                      frame1_url,
                      {{*cookie1}}});
  EXPECT_EQ(1U, helper->GetCookieCount());
  std::unique_ptr<net::CanonicalCookie> cookie2(
      net::CanonicalCookie::CreateForTesting(frame1_url, "B=1",
                                             base::Time::Now()));
  ASSERT_TRUE(cookie2);
  helper->AddCookies({content::CookieAccessDetails::Type::kChange,
                      frame1_url,
                      frame1_url,
                      {{*cookie2}}});
  EXPECT_EQ(2U, helper->GetCookieCount());

  // Use a different frame URL for adding another cookie that will replace one
  // of the previously added cookies. This could happen during an automatic
  // redirect e.g. |frame1_url| redirects to |frame2_url| and a cookie set by a
  // request to |frame1_url| is updated.
  // The cookie-name of |cookie3| must match the cookie-name of |cookie1|.
  std::unique_ptr<net::CanonicalCookie> cookie3(
      net::CanonicalCookie::CreateForTesting(frame1_url, "A=2",
                                             base::Time::Now()));
  ASSERT_TRUE(cookie3);
  helper->AddCookies({content::CookieAccessDetails::Type::kChange,
                      frame2_url,
                      frame1_url,
                      {{*cookie3}}});
  EXPECT_EQ(2U, helper->GetCookieCount());

  // Add two more cookies that are set while loading resources. The two cookies
  // below have a differnt path-value since the request URLs have different
  // paths.
  std::unique_ptr<net::CanonicalCookie> cookie4(
      net::CanonicalCookie::CreateForTesting(request1_url, "A=2",
                                             base::Time::Now()));
  ASSERT_TRUE(cookie4);
  helper->AddCookies({content::CookieAccessDetails::Type::kChange,
                      frame2_url,
                      request1_url,
                      {{*cookie4}}});
  EXPECT_EQ(3U, helper->GetCookieCount());
  std::unique_ptr<net::CanonicalCookie> cookie5(
      net::CanonicalCookie::CreateForTesting(request2_url, "A=2",
                                             base::Time::Now()));
  ASSERT_TRUE(cookie5);
  helper->AddCookies({content::CookieAccessDetails::Type::kChange,
                      frame2_url,
                      request2_url,
                      {{*cookie5}}});
  EXPECT_EQ(4U, helper->GetCookieCount());

  // Host-only and domain cookies are treated as seperate items. This means that
  // the following two cookie-strings are stored as two separate cookies, even
  // though they have the same name and are send with the same request:
  //   "A=1;
  //   "A=3; Domain=www.google.com"
  // Add a domain cookie and check if it increases the cookie count.
  std::unique_ptr<net::CanonicalCookie> cookie6(
      net::CanonicalCookie::CreateForTesting(
          frame1_url, "A=3; Domain=.www.google.com", base::Time::Now()));
  ASSERT_TRUE(cookie6);
  helper->AddCookies({content::CookieAccessDetails::Type::kChange,
                      frame2_url,
                      frame1_url,
                      {{*cookie6}}});
  EXPECT_EQ(5U, helper->GetCookieCount());
}

}  // namespace
}  // namespace browsing_data
