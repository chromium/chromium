// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/net_internals/net_internals_ui_browsertest.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/macros.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/io_thread.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/net_internals/net_internals_ui.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "net/base/address_list.h"
#include "net/base/net_errors.h"
#include "net/dns/host_cache.h"
#include "net/dns/host_resolver.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_network_session.h"
#include "net/http/http_transaction_factory.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/request_handler_util.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using content::BrowserThread;
using content::WebUIMessageHandler;

namespace {

std::unique_ptr<net::test_server::HttpResponse> HandleExpectCTReportPreflight(
    const net::test_server::HttpRequest& request) {
  std::unique_ptr<net::test_server::BasicHttpResponse> http_response(
      new net::test_server::BasicHttpResponse());
  http_response->set_code(net::HTTP_OK);
  http_response->AddCustomHeader("Access-Control-Allow-Origin", "*");
  http_response->AddCustomHeader("Access-Control-Allow-Methods", "POST");
  http_response->AddCustomHeader("Access-Control-Allow-Headers",
                                 "Content-Type");
  return http_response;
}

// Called on IO thread.  Adds an entry to the cache for the specified hostname.
// Either |net_error| must be net::OK, or |address| must be NULL.
void AddCacheEntryOnIOThread(net::URLRequestContextGetter* context_getter,
                             const std::string& hostname,
                             const std::string& ip_literal,
                             int net_error,
                             int expire_days_from_now) {
  ASSERT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::IO));
  net::URLRequestContext* context = context_getter->GetURLRequestContext();
  net::HostCache* cache = context->host_resolver()->GetHostCache();
  ASSERT_TRUE(cache);

  net::HostCache::Key key(hostname, net::ADDRESS_FAMILY_UNSPECIFIED, 0);
  base::TimeDelta ttl = base::TimeDelta::FromDays(expire_days_from_now);

  net::AddressList address_list;
  if (net_error == net::OK) {
    // If |net_error| does not indicate an error, convert |ip_literal| to a
    // net::AddressList, so it can be used with the cache.
    int rv = net::ParseAddressList(ip_literal, hostname, &address_list);
    ASSERT_EQ(net::OK, rv);
  } else {
    ASSERT_TRUE(ip_literal.empty());
  }

  // Add entry to the cache.
  cache->Set(net::HostCache::Key(hostname, net::ADDRESS_FAMILY_UNSPECIFIED, 0),
             net::HostCache::Entry(net_error, address_list,
                                   net::HostCache::Entry::SOURCE_UNKNOWN),
             base::TimeTicks::Now(), ttl);
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// NetInternalsTest::MessageHandler
////////////////////////////////////////////////////////////////////////////////

// Class to handle messages from the renderer needed by certain tests.
class NetInternalsTest::MessageHandler : public content::WebUIMessageHandler {
 public:
  explicit MessageHandler(NetInternalsTest* net_internals_test);

 private:
  void RegisterMessages() override;

  void RegisterMessage(const std::string& message,
                       const content::WebUI::MessageCallback& handler);

  void HandleMessage(const content::WebUI::MessageCallback& handler,
                     const base::ListValue* data);

  // Runs NetInternalsTest.callback with the given value.
  void RunJavascriptCallback(base::Value* value);

  // Takes a string and provides the corresponding URL from the test server,
  // which must already have been started.
  void GetTestServerURL(const base::ListValue* list_value);

  // Called on UI thread.  Adds an entry to the cache for the specified
  // hostname by posting a task to the IO thread.  Takes the host name,
  // ip address, net error code, and expiration time in days from now
  // as parameters.  If the error code indicates failure, the ip address
  // must be an empty string.
  void AddCacheEntry(const base::ListValue* list_value);

  // Sets up the test server to receive test Expect-CT reports. Calls the
  // Javascript callback to return the test server URI.
  void SetUpTestReportURI(const base::ListValue* list_value);

  Browser* browser() { return net_internals_test_->browser(); }

  NetInternalsTest* net_internals_test_;

  DISALLOW_COPY_AND_ASSIGN(MessageHandler);
};

NetInternalsTest::MessageHandler::MessageHandler(
    NetInternalsTest* net_internals_test)
    : net_internals_test_(net_internals_test) {}

void NetInternalsTest::MessageHandler::RegisterMessages() {
  RegisterMessage(
      "getTestServerURL",
      base::BindRepeating(&NetInternalsTest::MessageHandler::GetTestServerURL,
                          base::Unretained(this)));
  RegisterMessage(
      "addCacheEntry",
      base::BindRepeating(&NetInternalsTest::MessageHandler::AddCacheEntry,
                          base::Unretained(this)));
  RegisterMessage(
      "setUpTestReportURI",
      base::BindRepeating(&NetInternalsTest::MessageHandler::SetUpTestReportURI,
                          base::Unretained(this)));
}

void NetInternalsTest::MessageHandler::RegisterMessage(
    const std::string& message,
    const content::WebUI::MessageCallback& handler) {
  web_ui()->RegisterMessageCallback(
      message,
      base::BindRepeating(&NetInternalsTest::MessageHandler::HandleMessage,
                          base::Unretained(this), handler));
}

void NetInternalsTest::MessageHandler::HandleMessage(
    const content::WebUI::MessageCallback& handler,
    const base::ListValue* data) {
  // The handler might run a nested loop to wait for something.
  base::MessageLoopCurrent::ScopedNestableTaskAllower nestable_task_allower;
  handler.Run(data);
}

void NetInternalsTest::MessageHandler::RunJavascriptCallback(
    base::Value* value) {
  web_ui()->CallJavascriptFunctionUnsafe("NetInternalsTest.callback", *value);
}

void NetInternalsTest::MessageHandler::GetTestServerURL(
    const base::ListValue* list_value) {
  ASSERT_TRUE(net_internals_test_->StartTestServer());
  std::string path;
  ASSERT_TRUE(list_value->GetString(0, &path));
  GURL url = net_internals_test_->embedded_test_server()->GetURL(path);
  std::unique_ptr<base::Value> url_value(new base::Value(url.spec()));
  RunJavascriptCallback(url_value.get());
}

void NetInternalsTest::MessageHandler::AddCacheEntry(
    const base::ListValue* list_value) {
  std::string hostname;
  std::string ip_literal;
  double net_error;
  double expire_days_from_now;
  ASSERT_TRUE(list_value->GetString(0, &hostname));
  ASSERT_TRUE(list_value->GetString(1, &ip_literal));
  ASSERT_TRUE(list_value->GetDouble(2, &net_error));
  ASSERT_TRUE(list_value->GetDouble(3, &expire_days_from_now));
  ASSERT_TRUE(browser());

  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(
          &AddCacheEntryOnIOThread,
          base::RetainedRef(browser()->profile()->GetRequestContext()),
          hostname, ip_literal, static_cast<int>(net_error),
          static_cast<int>(expire_days_from_now)));
}

void NetInternalsTest::MessageHandler::SetUpTestReportURI(
    const base::ListValue* list_value) {
  net_internals_test_->embedded_test_server()->RegisterRequestHandler(
      base::Bind(&HandleExpectCTReportPreflight));
  ASSERT_TRUE(net_internals_test_->embedded_test_server()->Start());
  base::Value report_uri_value(
      net_internals_test_->embedded_test_server()->GetURL("/").spec());
  RunJavascriptCallback(&report_uri_value);
}

////////////////////////////////////////////////////////////////////////////////
// NetInternalsTest
////////////////////////////////////////////////////////////////////////////////

NetInternalsTest::NetInternalsTest()
    : test_server_started_(false) {
  message_handler_.reset(new MessageHandler(this));
}

NetInternalsTest::~NetInternalsTest() {
}

content::WebUIMessageHandler* NetInternalsTest::GetMockMessageHandler() {
  return message_handler_.get();
}

bool NetInternalsTest::StartTestServer() {
  if (test_server_started_)
    return true;
  test_server_started_ = embedded_test_server()->Start();

  return test_server_started_;
}
