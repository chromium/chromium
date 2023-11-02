// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/browser/url_blocklist_manager.h"

#include <stdint.h>
#include <memory>
#include <ostream>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/url_formatter/url_fixer.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/base/load_flags.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace policy {

namespace {

class TestingURLBlocklistManager : public URLBlocklistManager {
 public:
  explicit TestingURLBlocklistManager(PrefService* pref_service)
      : URLBlocklistManager(pref_service,
                            policy_prefs::kUrlBlocklist,
                            policy_prefs::kUrlAllowlist),
        update_called_(0),
        set_blocklist_called_(false) {}
  TestingURLBlocklistManager(const TestingURLBlocklistManager&) = delete;
  TestingURLBlocklistManager& operator=(const TestingURLBlocklistManager&) =
      delete;

  ~TestingURLBlocklistManager() override = default;

  // Make this method public for testing.
  using URLBlocklistManager::ScheduleUpdate;

  // URLBlocklistManager overrides:
  void SetBlocklist(std::unique_ptr<URLBlocklist> blocklist) override {
    set_blocklist_called_ = true;
    URLBlocklistManager::SetBlocklist(std::move(blocklist));
  }

  void Update() override {
    update_called_++;
    URLBlocklistManager::Update();
  }

  int update_called() const { return update_called_; }
  bool set_blocklist_called() const { return set_blocklist_called_; }

 private:
  int update_called_;
  bool set_blocklist_called_;
};

class URLBlocklistManagerTest : public testing::Test {
 protected:
  URLBlocklistManagerTest() = default;

  void SetUp() override {
    pref_service_.registry()->RegisterListPref(policy_prefs::kUrlBlocklist);
    pref_service_.registry()->RegisterListPref(policy_prefs::kUrlAllowlist);
    blocklist_manager_ =
        std::make_unique<TestingURLBlocklistManager>(&pref_service_);
    task_environment_.RunUntilIdle();
  }

  void TearDown() override {
    if (blocklist_manager_)
      task_environment_.RunUntilIdle();
    blocklist_manager_.reset();
  }

  TestingPrefServiceSimple pref_service_;
  std::unique_ptr<TestingURLBlocklistManager> blocklist_manager_;

  base::test::TaskEnvironment task_environment_;
};

}  // namespace

// Returns whether |url| matches the |pattern|.
bool IsMatch(const std::string& pattern, const std::string& url) {
  URLBlocklist blocklist;

  // Add the pattern to blocklist.
  base::Value::List blocked;
  blocked.Append(pattern);
  blocklist.Block(blocked);

  return blocklist.IsURLBlocked(GURL(url));
}

// Returns the state from blocklist after adding |pattern| to be blocked or
// allowed depending on |use_allowlist| and checking |url|.
policy::URLBlocklist::URLBlocklistState GetMatch(const std::string& pattern,
                                                 const std::string& url,
                                                 const bool use_allowlist) {
  URLBlocklist blocklist;

  // Add the pattern to list.
  base::Value::List blocked;
  blocked.Append(pattern);

  if (use_allowlist) {
    blocklist.Allow(blocked);
  } else {
    blocklist.Block(blocked);
  }

  return blocklist.GetURLBlocklistState(GURL(url));
}

TEST_F(URLBlocklistManagerTest, LoadBlocklistOnCreate) {
  base::Value::List list;
  list.Append("example.com");
  pref_service_.SetManagedPref(policy_prefs::kUrlBlocklist,
                               std::make_unique<base::Value>(std::move(list)));
  auto manager = std::make_unique<URLBlocklistManager>(
      &pref_service_, policy_prefs::kUrlBlocklist, policy_prefs::kUrlAllowlist);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(URLBlocklist::URL_IN_BLOCKLIST,
            manager->GetURLBlocklistState(GURL("http://example.com")));
}

TEST_F(URLBlocklistManagerTest, LoadAllowlistOnCreate) {
  base::Value list(base::Value::Type::LIST);
  list.Append("example.com");
  pref_service_.SetManagedPref(policy_prefs::kUrlAllowlist,
                               base::Value::ToUniquePtrValue(std::move(list)));
  auto manager = std::make_unique<URLBlocklistManager>(
      &pref_service_, policy_prefs::kUrlBlocklist, policy_prefs::kUrlAllowlist);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(URLBlocklist::URL_IN_ALLOWLIST,
            manager->GetURLBlocklistState(GURL("http://example.com")));
}

TEST_F(URLBlocklistManagerTest, SingleUpdateForTwoPrefChanges) {
  base::Value blocklist(base::Value::Type::LIST);
  blocklist.Append("*.google.com");
  base::Value allowlist(base::Value::Type::LIST);
  allowlist.Append("mail.google.com");
  pref_service_.SetManagedPref(
      policy_prefs::kUrlBlocklist,
      base::Value::ToUniquePtrValue(std::move(blocklist)));
  pref_service_.SetManagedPref(
      policy_prefs::kUrlBlocklist,
      base::Value::ToUniquePtrValue(std::move(allowlist)));
  task_environment_.RunUntilIdle();

  EXPECT_EQ(1, blocklist_manager_->update_called());
}

TEST_F(URLBlocklistManagerTest, Filtering) {
  URLBlocklist blocklist;

  // Block domain and all subdomains, for any filtered scheme.
  EXPECT_TRUE(IsMatch("google.com", "http://google.com"));
  EXPECT_TRUE(IsMatch("google.com", "http://google.com/"));
  EXPECT_TRUE(IsMatch("google.com", "http://google.com/whatever"));
  EXPECT_TRUE(IsMatch("google.com", "https://google.com/"));
  EXPECT_FALSE(IsMatch("google.com", "bogus://google.com/"));
  EXPECT_FALSE(IsMatch("google.com", "http://notgoogle.com/"));
  EXPECT_TRUE(IsMatch("google.com", "http://mail.google.com"));
  EXPECT_TRUE(IsMatch("google.com", "http://x.mail.google.com"));
  EXPECT_TRUE(IsMatch("google.com", "https://x.mail.google.com/"));
  EXPECT_TRUE(IsMatch("google.com", "http://x.y.google.com/a/b"));
  EXPECT_FALSE(IsMatch("google.com", "http://youtube.com/"));

  // Filter only http, ftp and ws schemes.
  EXPECT_TRUE(IsMatch("http://secure.com", "http://secure.com"));
  EXPECT_TRUE(IsMatch("http://secure.com", "http://secure.com/whatever"));
  EXPECT_TRUE(IsMatch("ftp://secure.com", "ftp://secure.com/"));
  EXPECT_TRUE(IsMatch("ws://secure.com", "ws://secure.com"));
  EXPECT_FALSE(IsMatch("http://secure.com", "https://secure.com/"));
  EXPECT_FALSE(IsMatch("ws://secure.com", "wss://secure.com"));
  EXPECT_TRUE(IsMatch("http://secure.com", "http://www.secure.com"));
  EXPECT_FALSE(IsMatch("http://secure.com", "https://www.secure.com"));
  EXPECT_FALSE(IsMatch("ws://secure.com", "wss://www.secure.com"));

  // Filter only a certain path prefix.
  EXPECT_TRUE(IsMatch("path.to/ruin", "http://path.to/ruin"));
  EXPECT_TRUE(IsMatch("path.to/ruin", "https://path.to/ruin"));
  EXPECT_TRUE(IsMatch("path.to/ruin", "http://path.to/ruins"));
  EXPECT_TRUE(IsMatch("path.to/ruin", "http://path.to/ruin/signup"));
  EXPECT_TRUE(IsMatch("path.to/ruin", "http://www.path.to/ruin"));
  EXPECT_FALSE(IsMatch("path.to/ruin", "http://path.to/fortune"));

  // Filter only a certain path prefix and scheme.
  EXPECT_TRUE(IsMatch("https://s.aaa.com/path", "https://s.aaa.com/path"));
  EXPECT_TRUE(IsMatch("https://s.aaa.com/path", "https://s.aaa.com/path/bbb"));
  EXPECT_FALSE(IsMatch("https://s.aaa.com/path", "http://s.aaa.com/path"));
  EXPECT_FALSE(IsMatch("https://s.aaa.com/path", "https://aaa.com/path"));
  EXPECT_FALSE(IsMatch("https://s.aaa.com/path", "https://x.aaa.com/path"));
  EXPECT_FALSE(IsMatch("https://s.aaa.com/path", "https://s.aaa.com/bbb"));
  EXPECT_FALSE(IsMatch("https://s.aaa.com/path", "https://s.aaa.com/"));

  // Filter only ws and wss schemes.
  EXPECT_TRUE(IsMatch("ws://ws.aaa.com", "ws://ws.aaa.com"));
  EXPECT_TRUE(IsMatch("wss://ws.aaa.com", "wss://ws.aaa.com"));
  EXPECT_FALSE(IsMatch("ws://ws.aaa.com", "http://ws.aaa.com"));
  EXPECT_FALSE(IsMatch("ws://ws.aaa.com", "https://ws.aaa.com"));
  EXPECT_FALSE(IsMatch("ws://ws.aaa.com", "ftp://ws.aaa.com"));

  // Block an ip address.
  EXPECT_TRUE(IsMatch("123.123.123.123", "http://123.123.123.123/"));
  EXPECT_FALSE(IsMatch("123.123.123.123", "http://123.123.123.124/"));

  // Test exceptions to path prefixes, and most specific matches.
  base::Value::List blocked;
  base::Value::List allowed;
  blocked.Append("s.xxx.com/a");
  allowed.Append("s.xxx.com/a/b");
  blocked.Append("https://s.xxx.com/a/b/c");
  allowed.Append("https://s.xxx.com/a/b/c/d");
  blocklist.Block(blocked);
  blocklist.Allow(allowed);
  EXPECT_TRUE(blocklist.IsURLBlocked(GURL("http://s.xxx.com/a")));
  EXPECT_TRUE(blocklist.IsURLBlocked(GURL("http://s.xxx.com/a/x")));
  EXPECT_TRUE(blocklist.IsURLBlocked(GURL("https://s.xxx.com/a/x")));
  EXPECT_FALSE(blocklist.IsURLBlocked(GURL("http://s.xxx.com/a/b")));
  EXPECT_FALSE(blocklist.IsURLBlocked(GURL("https://s.xxx.com/a/b")));
  EXPECT_FALSE(blocklist.IsURLBlocked(GURL("http://s.xxx.com/a/b/x")));
  EXPECT_FALSE(blocklist.IsURLBlocked(GURL("http://s.xxx.com/a/b/c")));
  EXPECT_TRUE(blocklist.IsURLBlocked(GURL("https://s.xxx.com/a/b/c")));
  EXPECT_TRUE(blocklist.IsURLBlocked(GURL("https://s.xxx.com/a/b/c/x")));
  EXPECT_FALSE(blocklist.IsURLBlocked(GURL("https://s.xxx.com/a/b/c/d")));
  EXPECT_FALSE(blocklist.IsURLBlocked(GURL("http://s.xxx.com/a/b/c/d")));
  EXPECT_FALSE(blocklist.IsURLBlocked(GURL("https://s.xxx.com/a/b/c/d/x")));
  EXPECT_FALSE(blocklist.IsURLBlocked(GURL("http://s.xxx.com/a/b/c/d/x")));
  EXPECT_FALSE(blocklist.IsURLBlocked(GURL("http://xxx.com/a")));
  EXPECT_FALSE(blocklist.IsURLBlocked(GURL("http://xxx.com/a/b")));

  // Open an exception.
  blocked.clear();
  blocked.Append("google.com");
  allowed.clear();
  allowed.Append("plus.google.com");
  blocklist.Block(blocked);
  blocklist.Allow(allowed);
  EXPECT_TRUE(blocklist.IsURLBlocked(GURL("http://google.com/")));
  EXPECT_TRUE(blocklist.IsURLBlocked(GURL("http://www.google.com/")));
  EXPECT_FALSE(blocklist.IsURLBlocked(GURL("http://plus.google.com/")));

  // Open an exception only when using https for mail.
  allowed.clear();
  allowed.Append("https://mail.google.com");
  blocklist.Allow(allowed);
  EXPECT_TRUE(blocklist.IsURLBlocked(GURL("http://google.com/")));
  EXPECT_TRUE(blocklist.IsURLBlocked(GURL("http://mail.google.com/")));
  EXPECT_TRUE(blocklist.IsURLBlocked(GURL("http://www.google.com/")));
  EXPECT_TRUE(blocklist.IsURLBlocked(GURL("https://www.google.com/")));
  EXPECT_FALSE(blocklist.IsURLBlocked(GURL("https://mail.google.com/")));

  // Match exactly "google.com", only for http. Subdomains without exceptions
  // are still blocked.
  allowed.clear();
  allowed.Append("http://.google.com");
  blocklist.Allow(allowed);
  EXPECT_FALSE(blocklist.IsURLBlocked(GURL("http://google.com/")));
  EXPECT_TRUE(blocklist.IsURLBlocked(GURL("https://google.com/")));
  EXPECT_TRUE(blocklist.IsURLBlocked(GURL("http://www.google.com/")));

  // A smaller path match in an exact host overrides a longer path for hosts
  // that also match subdomains.
  blocked.clear();
  blocked.Append("yyy.com/aaa");
  blocklist.Block(blocked);
  allowed.clear();
  allowed.Append(".yyy.com/a");
  blocklist.Allow(allowed);
  EXPECT_FALSE(blocklist.IsURLBlocked(GURL("http://yyy.com")));
  EXPECT_FALSE(blocklist.IsURLBlocked(GURL("http://yyy.com/aaa")));
  EXPECT_FALSE(blocklist.IsURLBlocked(GURL("http://yyy.com/aaa2")));
  EXPECT_FALSE(blocklist.IsURLBlocked(GURL("http://www.yyy.com")));
  EXPECT_TRUE(blocklist.IsURLBlocked(GURL("http://www.yyy.com/aaa")));
  EXPECT_TRUE(blocklist.IsURLBlocked(GURL("http://www.yyy.com/aaa2")));

  // If the exact entry is both allowed and blocked, allowing takes precedence.
  blocked.clear();
  blocked.Append("example.com");
  blocklist.Block(blocked);
  allowed.clear();
  allowed.Append("example.com");
  blocklist.Allow(allowed);
  EXPECT_FALSE(blocklist.IsURLBlocked(GURL("http://example.com")));

  // Devtools should not be blocked.
  blocked.clear();
  blocked.Append("*");
  blocklist.Block(blocked);
  allowed.clear();
  allowed.Append("devtools://*");
  blocklist.Allow(allowed);
  EXPECT_FALSE(blocklist.IsURLBlocked(GURL("devtools://something.com")));
  EXPECT_TRUE(blocklist.IsURLBlocked(GURL("https://something.com")));
}

TEST_F(URLBlocklistManagerTest, QueryParameters) {
  URLBlocklist blocklist;
  base::Value::List blocked;
  base::Value::List allowed;

  // Block domain and all subdomains, for any filtered scheme.
  blocked.Append("youtube.com");
  allowed.Append("youtube.com/watch?v=XYZ");
  blocklist.Block(blocked);
  blocklist.Allow(allowed);

  EXPECT_TRUE(blocklist.IsURLBlocked(GURL("http://youtube.com")));
  EXPECT_TRUE(blocklist.IsURLBlocked(GURL("http://youtube.com/watch?v=123")));
  EXPECT_TRUE(
      blocklist.IsURLBlocked(GURL("http://youtube.com/watch?v=123&v=XYZ")));
  EXPECT_TRUE(
      blocklist.IsURLBlocked(GURL("http://youtube.com/watch?v=XYZ&v=123")));
  EXPECT_FALSE(blocklist.IsURLBlocked(GURL("http://youtube.com/watch?v=XYZ")));
  EXPECT_FALSE(
      blocklist.IsURLBlocked(GURL("http://youtube.com/watch?v=XYZ&foo=bar")));
  EXPECT_FALSE(
      blocklist.IsURLBlocked(GURL("http://youtube.com/watch?foo=bar&v=XYZ")));

  allowed.clear();
  allowed.Append("youtube.com/watch?av=XYZ&ag=123");
  blocklist.Allow(allowed);
  EXPECT_TRUE(blocklist.IsURLBlocked(GURL("http://youtube.com")));
  EXPECT_TRUE(blocklist.IsURLBlocked(GURL("http://youtube.com/watch?av=123")));
  EXPECT_TRUE(blocklist.IsURLBlocked(GURL("http://youtube.com/watch?av=XYZ")));
  EXPECT_TRUE(
      blocklist.IsURLBlocked(GURL("http://youtube.com/watch?av=123&ag=XYZ")));
  EXPECT_TRUE(
      blocklist.IsURLBlocked(GURL("http://youtube.com/watch?ag=XYZ&av=123")));
  EXPECT_FALSE(
      blocklist.IsURLBlocked(GURL("http://youtube.com/watch?av=XYZ&ag=123")));
  EXPECT_FALSE(
      blocklist.IsURLBlocked(GURL("http://youtube.com/watch?ag=123&av=XYZ")));
  EXPECT_TRUE(blocklist.IsURLBlocked(
      GURL("http://youtube.com/watch?av=XYZ&ag=123&av=123")));
  EXPECT_TRUE(blocklist.IsURLBlocked(
      GURL("http://youtube.com/watch?av=XYZ&ag=123&ag=1234")));

  allowed.clear();
  allowed.Append("youtube.com/watch?foo=bar*&vid=2*");
  blocklist.Allow(allowed);
  EXPECT_TRUE(blocklist.IsURLBlocked(GURL("http://youtube.com")));
  EXPECT_TRUE(blocklist.IsURLBlocked(GURL("http://youtube.com/watch?vid=2")));
  EXPECT_TRUE(blocklist.IsURLBlocked(GURL("http://youtube.com/watch?foo=bar")));
  EXPECT_FALSE(
      blocklist.IsURLBlocked(GURL("http://youtube.com/watch?vid=2&foo=bar")));
  EXPECT_FALSE(
      blocklist.IsURLBlocked(GURL("http://youtube.com/watch?vid=2&foo=bar1")));
  EXPECT_FALSE(
      blocklist.IsURLBlocked(GURL("http://youtube.com/watch?vid=234&foo=bar")));
  EXPECT_FALSE(blocklist.IsURLBlocked(
      GURL("http://youtube.com/watch?vid=234&foo=bar23")));

  blocked.clear();
  blocked.Append("youtube1.com/disallow?v=44678");
  blocklist.Block(blocked);
  EXPECT_FALSE(blocklist.IsURLBlocked(GURL("http://youtube1.com")));
  EXPECT_FALSE(blocklist.IsURLBlocked(GURL("http://youtube1.com?v=123")));
  // Path does not match
  EXPECT_FALSE(blocklist.IsURLBlocked(GURL("http://youtube1.com?v=44678")));
  EXPECT_TRUE(
      blocklist.IsURLBlocked(GURL("http://youtube1.com/disallow?v=44678")));
  EXPECT_FALSE(
      blocklist.IsURLBlocked(GURL("http://youtube1.com/disallow?v=4467")));
  EXPECT_FALSE(blocklist.IsURLBlocked(
      GURL("http://youtube1.com/disallow?v=4467&v=123")));
  EXPECT_TRUE(blocklist.IsURLBlocked(
      GURL("http://youtube1.com/disallow?v=4467&v=123&v=44678")));

  blocked.clear();
  blocked.Append("youtube1.com/disallow?g=*");
  blocklist.Block(blocked);
  EXPECT_FALSE(blocklist.IsURLBlocked(GURL("http://youtube1.com")));
  EXPECT_FALSE(blocklist.IsURLBlocked(GURL("http://youtube1.com?ag=123")));
  EXPECT_TRUE(
      blocklist.IsURLBlocked(GURL("http://youtube1.com/disallow?g=123")));
  EXPECT_TRUE(
      blocklist.IsURLBlocked(GURL("http://youtube1.com/disallow?ag=13&g=123")));

  blocked.clear();
  blocked.Append("youtube2.com/disallow?a*");
  blocklist.Block(blocked);
  EXPECT_FALSE(blocklist.IsURLBlocked(GURL("http://youtube2.com")));
  EXPECT_TRUE(blocklist.IsURLBlocked(
      GURL("http://youtube2.com/disallow?b=123&a21=467")));
  EXPECT_TRUE(
      blocklist.IsURLBlocked(GURL("http://youtube2.com/disallow?abba=true")));
  EXPECT_FALSE(
      blocklist.IsURLBlocked(GURL("http://youtube2.com/disallow?baba=true")));

  allowed.clear();
  blocked.clear();
  blocked.Append("youtube3.com");
  allowed.Append("youtube3.com/watch?fo*");
  blocklist.Block(blocked);
  blocklist.Allow(allowed);
  EXPECT_TRUE(blocklist.IsURLBlocked(GURL("http://youtube3.com")));
  EXPECT_TRUE(
      blocklist.IsURLBlocked(GURL("http://youtube3.com/watch?b=123&a21=467")));
  EXPECT_FALSE(blocklist.IsURLBlocked(
      GURL("http://youtube3.com/watch?b=123&a21=467&foo1")));
  EXPECT_FALSE(blocklist.IsURLBlocked(
      GURL("http://youtube3.com/watch?b=123&a21=467&foo=bar")));
  EXPECT_FALSE(blocklist.IsURLBlocked(
      GURL("http://youtube3.com/watch?b=123&a21=467&fo=ba")));
  EXPECT_FALSE(
      blocklist.IsURLBlocked(GURL("http://youtube3.com/watch?foreign=true")));
  EXPECT_FALSE(blocklist.IsURLBlocked(GURL("http://youtube3.com/watch?fold")));

  allowed.clear();
  blocked.clear();
  blocked.Append("youtube4.com");
  allowed.Append("youtube4.com?*");
  blocklist.Block(blocked);
  blocklist.Allow(allowed);
  EXPECT_TRUE(blocklist.IsURLBlocked(GURL("http://youtube4.com")));
  EXPECT_FALSE(blocklist.IsURLBlocked(GURL("http://youtube4.com/?hello")));
  EXPECT_FALSE(blocklist.IsURLBlocked(GURL("http://youtube4.com/?foo")));

  allowed.clear();
  blocked.clear();
  blocked.Append("youtube5.com?foo=bar");
  allowed.Append("youtube5.com?foo1=bar1&foo2=bar2&");
  blocklist.Block(blocked);
  blocklist.Allow(allowed);
  EXPECT_FALSE(blocklist.IsURLBlocked(GURL("http://youtube5.com")));
  EXPECT_TRUE(blocklist.IsURLBlocked(GURL("http://youtube5.com/?foo=bar&a=b")));
  // More specific filter is given precedence.
  EXPECT_FALSE(blocklist.IsURLBlocked(
      GURL("http://youtube5.com/?a=b&foo=bar&foo1=bar1&foo2=bar2")));
}

TEST_F(URLBlocklistManagerTest, BlockAllWithExceptions) {
  URLBlocklist blocklist;

  base::Value::List blocked;
  base::Value::List allowed;
  blocked.Append("*");
  allowed.Append(".www.google.com");
  allowed.Append("plus.google.com");
  allowed.Append("https://mail.google.com");
  allowed.Append("https://very.safe/path");
  blocklist.Block(blocked);
  blocklist.Allow(allowed);
  EXPECT_TRUE(blocklist.IsURLBlocked(GURL("http://random.com")));
  EXPECT_TRUE(blocklist.IsURLBlocked(GURL("http://google.com")));
  EXPECT_TRUE(blocklist.IsURLBlocked(GURL("http://s.www.google.com")));
  EXPECT_FALSE(blocklist.IsURLBlocked(GURL("http://www.google.com")));
  EXPECT_FALSE(blocklist.IsURLBlocked(GURL("http://plus.google.com")));
  EXPECT_FALSE(blocklist.IsURLBlocked(GURL("http://s.plus.google.com")));
  EXPECT_TRUE(blocklist.IsURLBlocked(GURL("http://mail.google.com")));
  EXPECT_FALSE(blocklist.IsURLBlocked(GURL("https://mail.google.com")));
  EXPECT_FALSE(blocklist.IsURLBlocked(GURL("https://s.mail.google.com")));
  EXPECT_TRUE(blocklist.IsURLBlocked(GURL("https://very.safe/")));
  EXPECT_TRUE(blocklist.IsURLBlocked(GURL("http://very.safe/path")));
  EXPECT_FALSE(blocklist.IsURLBlocked(GURL("https://very.safe/path")));
}

TEST_F(URLBlocklistManagerTest, DefaultBlocklistExceptions) {
  URLBlocklist blocklist;
  base::Value::List blocked;

  // Blocklist everything:
  blocked.Append("*");
  blocklist.Block(blocked);

  // Internal NTP and extension URLs are not blocked by the "*":
  EXPECT_TRUE(blocklist.IsURLBlocked(GURL("http://www.google.com")));
  EXPECT_FALSE(blocklist.IsURLBlocked(GURL("chrome-extension://xyz")));
  EXPECT_FALSE(
      blocklist.IsURLBlocked(GURL("chrome-search://most-visited/title.html")));
  EXPECT_FALSE(blocklist.IsURLBlocked(GURL("chrome-native://ntp")));
#if BUILDFLAG(IS_IOS)
  // Ensure that the NTP is not blocked on iOS by "*".
  // TODO(crbug.com/1073291): On iOS, the NTP can not be blocked even by
  // explicitly listing it as a blocked URL. This is due to the usage of
  // "about:newtab" as its URL which is not recognized and filtered by the
  // URLBlocklist code.
  EXPECT_FALSE(blocklist.IsURLBlocked(GURL("about:newtab")));
  EXPECT_FALSE(blocklist.IsURLBlocked(GURL("about://newtab/")));
  EXPECT_FALSE(blocklist.IsURLBlocked(GURL("chrome://newtab")));
#endif

  // Unless they are explicitly on the blocklist:
  blocked.Append("chrome-extension://*");
  base::Value::List allowed;
  allowed.Append("chrome-extension://abc");
  blocklist.Block(blocked);
  blocklist.Allow(allowed);

  EXPECT_TRUE(blocklist.IsURLBlocked(GURL("http://www.google.com")));
  EXPECT_TRUE(blocklist.IsURLBlocked(GURL("chrome-extension://xyz")));
  EXPECT_FALSE(blocklist.IsURLBlocked(GURL("chrome-extension://abc")));
  EXPECT_FALSE(
      blocklist.IsURLBlocked(GURL("chrome-search://most-visited/title.html")));
  EXPECT_FALSE(blocklist.IsURLBlocked(GURL("chrome-native://ntp")));
}

TEST_F(URLBlocklistManagerTest, BlocklistBasicCoverage) {
  // Tests to cover the documentation from
  // http://www.chromium.org/administrators/url-blocklist-filter-format

  // [scheme://][.]host[:port][/path][@query]
  // Scheme can be http, https, ftp, chrome, etc. This field is optional, and
  // must be followed by '://'.
  EXPECT_TRUE(IsMatch("file://*", "file:///abc.txt"));
  EXPECT_TRUE(IsMatch("file:*", "file:///usr/local/boot.txt"));
  EXPECT_TRUE(IsMatch("https://*", "https:///abc.txt"));
  EXPECT_TRUE(IsMatch("ftp://*", "ftp://ftp.txt"));
  EXPECT_TRUE(IsMatch("chrome://*", "chrome:policy"));
  EXPECT_TRUE(IsMatch("noscheme", "http://noscheme"));
  // Filter custom schemes.
  EXPECT_TRUE(IsMatch("custom://*", "custom://example_app"));
  EXPECT_TRUE(IsMatch("custom:*", "custom:example2_app"));
  EXPECT_FALSE(IsMatch("custom://*", "customs://example_apps"));
  EXPECT_FALSE(IsMatch("custom://*", "cust*://example_ap"));
  EXPECT_FALSE(IsMatch("custom://*", "ecustom:example_app"));
  EXPECT_TRUE(IsMatch("custom://*", "custom:///abc.txt"));
  // Tests for custom scheme patterns that are not supported.
  EXPECT_FALSE(IsMatch("wrong://app", "wrong://app"));
  EXPECT_FALSE(IsMatch("wrong ://*", "wrong ://app"));
  EXPECT_FALSE(IsMatch(" wrong:*", " wrong://app"));

  // Ommitting the scheme matches most standard schemes.
  EXPECT_TRUE(IsMatch("example.com", "chrome:example.com"));
  EXPECT_TRUE(IsMatch("example.com", "chrome://example.com"));
  EXPECT_TRUE(IsMatch("example.com", "file://example.com/"));
  EXPECT_TRUE(IsMatch("example.com", "ftp://example.com"));
  EXPECT_TRUE(IsMatch("example.com", "http://example.com"));
  EXPECT_TRUE(IsMatch("example.com", "https://example.com"));
  EXPECT_TRUE(IsMatch("example.com", "ws://example.com"));
  EXPECT_TRUE(IsMatch("example.com", "wss://example.com"));

  // Some schemes are not matched when the scheme is ommitted.
  EXPECT_FALSE(IsMatch("example.com", "about://example.com"));
  EXPECT_FALSE(IsMatch("example.com", "about:example.com"));
  EXPECT_FALSE(IsMatch("example.com/*", "filesystem:///something"));
  EXPECT_FALSE(IsMatch("example.com", "custom://example.com"));
  EXPECT_FALSE(IsMatch("example", "custom://example"));
  EXPECT_FALSE(IsMatch("example.com", "gopher://example.com"));

  // An optional '.' (dot) can prefix the host field to disable subdomain
  // matching, see below for details.
  EXPECT_TRUE(IsMatch(".example.com", "http://example.com/path"));
  EXPECT_FALSE(IsMatch(".example.com", "http://mail.example.com/path"));
  EXPECT_TRUE(IsMatch("example.com", "http://mail.example.com/path"));
  EXPECT_TRUE(IsMatch("ftp://.ftp.file", "ftp://ftp.file"));
  EXPECT_FALSE(IsMatch("ftp://.ftp.file", "ftp://sub.ftp.file"));

  // The host field is required, and is a valid hostname or an IP address. It
  // can also take the special '*' value, see below for details.
  EXPECT_TRUE(IsMatch("*", "http://anything"));
  EXPECT_TRUE(IsMatch("*", "ftp://anything"));
  EXPECT_TRUE(IsMatch("*", "custom://anything"));
  EXPECT_TRUE(IsMatch("host", "http://host:8080"));
  EXPECT_FALSE(IsMatch("host", "file:///host"));
  EXPECT_TRUE(IsMatch("10.1.2.3", "http://10.1.2.3:8080/path"));
  // No host, will match nothing.
  EXPECT_FALSE(IsMatch(":8080", "http://host:8080"));
  EXPECT_FALSE(IsMatch(":8080", "http://:8080"));

  // An optional port can come after the host. It must be a valid port value
  // from 1 to 65535.
  EXPECT_TRUE(IsMatch("host:8080", "http://host:8080/path"));
  EXPECT_TRUE(IsMatch("host:1", "http://host:1/path"));
  // Out of range port.
  EXPECT_FALSE(IsMatch("host:65536", "http://host:65536/path"));
  // Star is not allowed in port numbers.
  EXPECT_FALSE(IsMatch("example.com:*", "http://example.com"));
  EXPECT_FALSE(IsMatch("example.com:*", "http://example.com:8888"));

  // An optional path can come after port.
  EXPECT_TRUE(IsMatch("host/path", "http://host:8080/path"));
  EXPECT_TRUE(IsMatch("host/path/path2", "http://host/path/path2"));
  EXPECT_TRUE(IsMatch("host/path", "http://host/path/path2"));

  // An optional query can come in the end, which is a set of key-value and
  // key-only tokens delimited by '&'. The key-value tokens are separated
  // by '='. A query token can optionally end with a '*' to indicate prefix
  // match. Token order is ignored during matching.
  EXPECT_TRUE(IsMatch("host?q1=1&q2=2", "http://host?q2=2&q1=1"));
  EXPECT_FALSE(IsMatch("host?q1=1&q2=2", "http://host?q2=1&q1=2"));
  EXPECT_FALSE(IsMatch("host?q1=1&q2=2", "http://host?Q2=2&Q1=1"));
  EXPECT_TRUE(IsMatch("host?q1=1&q2=2", "http://host?q2=2&q1=1&q3=3"));
  EXPECT_TRUE(IsMatch("host?q1=1&q2=2*", "http://host?q2=21&q1=1&q3=3"));

  // user:pass fields can be included but will be ignored
  // (e.g. http://user:pass@ftp.example.com/pub/bigfile.iso).
  EXPECT_TRUE(IsMatch("host.com/path", "http://user:pass@host.com:8080/path"));
  EXPECT_TRUE(
      IsMatch("ftp://host.com/path", "ftp://user:pass@host.com:8080/path"));

  // Case sensitivity.
  // Scheme is case insensitive.
  EXPECT_TRUE(IsMatch("suPPort://*", "support:example"));
  EXPECT_TRUE(IsMatch("FILE://*", "file:example"));
  EXPECT_TRUE(IsMatch("FILE://*", "FILE://example"));
  EXPECT_TRUE(IsMatch("FtP:*", "ftp://example"));
  EXPECT_TRUE(IsMatch("http://example.com", "HTTP://example.com"));
  EXPECT_TRUE(IsMatch("HTTP://example.com", "http://example.com"));
  // Host is case insensitive.
  EXPECT_TRUE(IsMatch("http://EXAMPLE.COM", "http://example.com"));
  EXPECT_TRUE(IsMatch("Example.com", "http://examplE.com/Path?Query=1"));
  // Path is case sensitive.
  EXPECT_FALSE(IsMatch("example.com/Path", "http://example.com/path"));
  EXPECT_TRUE(IsMatch("http://example.com/aB", "http://example.com/aB"));
  EXPECT_FALSE(IsMatch("http://example.com/aB", "http://example.com/Ab"));
  EXPECT_FALSE(IsMatch("http://example.com/aB", "http://example.com/ab"));
  EXPECT_FALSE(IsMatch("http://example.com/aB", "http://example.com/AB"));
  // Query is case sensitive.
  EXPECT_FALSE(IsMatch("host/path?Query=1", "http://host/path?query=1"));
}

// Test for GetURLBlocklistState method.
TEST_F(URLBlocklistManagerTest, UseBlocklistState) {
  const policy::URLBlocklist::URLBlocklistState in_blocklist =
      policy::URLBlocklist::URLBlocklistState::URL_IN_BLOCKLIST;
  const policy::URLBlocklist::URLBlocklistState in_allowlist =
      policy::URLBlocklist::URLBlocklistState::URL_IN_ALLOWLIST;
  const policy::URLBlocklist::URLBlocklistState neutral_state =
      policy::URLBlocklist::URLBlocklistState::URL_NEUTRAL_STATE;

  // Test allowlist states.
  EXPECT_EQ(in_allowlist, GetMatch("example.com", "http://example.com", true));
  EXPECT_EQ(in_allowlist, GetMatch("http://*", "http://example.com", true));
  EXPECT_EQ(in_allowlist, GetMatch("custom://*", "custom://app", true));
  EXPECT_EQ(in_allowlist, GetMatch("custom:*", "custom://app/play", true));
  EXPECT_EQ(in_allowlist, GetMatch("custom:*", "custom://app:8080", true));
  // Test blocklist states.
  EXPECT_EQ(in_blocklist, GetMatch("ftp:*", "ftp://server", false));
  // Test neutral states.
  EXPECT_EQ(neutral_state, GetMatch("file:*", "http://example.com", true));
  EXPECT_EQ(neutral_state, GetMatch("https://*", "http://example.com", false));
}
}  // namespace policy
