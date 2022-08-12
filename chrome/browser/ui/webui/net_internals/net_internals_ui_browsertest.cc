// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/net_internals/net_internals_ui_browsertest.h"

#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/current_thread.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/net/system_network_context_manager.h"
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
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "net/base/address_list.h"
#include "net/base/net_errors.h"
#include "net/base/network_isolation_key.h"
#include "net/dns/mock_host_resolver.h"
#include "net/dns/public/host_resolver_source.h"
#include "net/dns/public/resolve_error_info.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/request_handler_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

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

// Notifies the NetInternalsTest.Task JS object of the DNS lookup result once
// it's complete. Owns itself.
class DnsLookupClient : public network::mojom::ResolveHostClient {
 public:
  using Callback = base::OnceCallback<void(base::Value*)>;

  DnsLookupClient(
      mojo::PendingReceiver<network::mojom::ResolveHostClient> receiver,
      Callback callback)
      : receiver_(this, std::move(receiver)), callback_(std::move(callback)) {
    receiver_.set_disconnect_handler(
        base::BindOnce(&DnsLookupClient::OnComplete, base::Unretained(this),
                       net::ERR_NAME_NOT_RESOLVED,
                       net::ResolveErrorInfo(net::ERR_FAILED), absl::nullopt));
  }
  ~DnsLookupClient() override {}

  // network::mojom::ResolveHostClient:
  void OnComplete(
      int32_t error,
      const net::ResolveErrorInfo& resolve_error_info,
      const absl::optional<net::AddressList>& resolved_addresses) override {
    std::string result;
    if (error == net::OK) {
      CHECK(resolved_addresses->size() == 1);
      result = resolved_addresses.value()[0].ToStringWithoutPort();
    } else {
      result = net::ErrorToString(resolve_error_info.error);
    }
    base::Value value(result);
    std::move(callback_).Run(&value);
    delete this;
  }
  void OnTextResults(const std::vector<std::string>& text_results) override {}
  void OnHostnameResults(const std::vector<net::HostPortPair>& hosts) override {
  }

 private:
  mojo::Receiver<network::mojom::ResolveHostClient> receiver_;
  Callback callback_;
};

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// NetInternalsTest::MessageHandler
////////////////////////////////////////////////////////////////////////////////

// Class to handle messages from the renderer needed by certain tests.
class NetInternalsTest::MessageHandler : public content::WebUIMessageHandler {
 public:
  explicit MessageHandler(NetInternalsTest* net_internals_test);

  MessageHandler(const MessageHandler&) = delete;
  MessageHandler& operator=(const MessageHandler&) = delete;

 private:
  void RegisterMessages() override;

  void RegisterMessage(const std::string& message,
                       const content::WebUI::MessageCallback& handler);

  void HandleMessage(const content::WebUI::MessageCallback& handler,
                     const base::Value::List& data);

  // Runs NetInternalsTest.callback with the given value.
  void RunJavascriptCallback(base::Value* value);

  // Takes a string and provides the corresponding URL from the test server,
  // which must already have been started.
  void GetTestServerURL(const base::Value::List& list);

  // Sets up the test server to receive test Expect-CT reports. Calls the
  // Javascript callback to return the test server URI.
  void SetUpTestReportURI(const base::Value::List& list);

  // Performs a DNS lookup. Calls the Javascript callback with the host's IP
  // address or an error string.
  void DnsLookup(const base::Value::List& list);

  Browser* browser() { return net_internals_test_->browser(); }

  raw_ptr<NetInternalsTest> net_internals_test_;

  // Single NetworkIsolationKey used for all DNS lookups, so repeated lookups
  // use the same cache key.
  net::NetworkIsolationKey network_isolation_key_{
      net::NetworkIsolationKey::CreateTransient()};

  base::WeakPtrFactory<MessageHandler> weak_factory_{this};
};

NetInternalsTest::MessageHandler::MessageHandler(
    NetInternalsTest* net_internals_test)
    : net_internals_test_(net_internals_test) {}

void NetInternalsTest::MessageHandler::RegisterMessages() {
  RegisterMessage(
      "getTestServerURL",
      base::BindRepeating(&NetInternalsTest::MessageHandler::GetTestServerURL,
                          weak_factory_.GetWeakPtr()));
  RegisterMessage(
      "setUpTestReportURI",
      base::BindRepeating(&NetInternalsTest::MessageHandler::SetUpTestReportURI,
                          weak_factory_.GetWeakPtr()));
  RegisterMessage("dnsLookup", base::BindRepeating(
                                   &NetInternalsTest::MessageHandler::DnsLookup,
                                   weak_factory_.GetWeakPtr()));
}

void NetInternalsTest::MessageHandler::RegisterMessage(
    const std::string& message,
    const content::WebUI::MessageCallback& handler) {
  web_ui()->RegisterMessageCallback(
      message,
      base::BindRepeating(&NetInternalsTest::MessageHandler::HandleMessage,
                          weak_factory_.GetWeakPtr(), handler));
}

void NetInternalsTest::MessageHandler::HandleMessage(
    const content::WebUI::MessageCallback& handler,
    const base::Value::List& data) {
  // The handler might run a nested loop to wait for something.
  base::CurrentThread::ScopedNestableTaskAllower nestable_task_allower;
  handler.Run(data);
}

void NetInternalsTest::MessageHandler::RunJavascriptCallback(
    base::Value* value) {
  web_ui()->CallJavascriptFunctionUnsafe("NetInternalsTest.callback", *value);
}

void NetInternalsTest::MessageHandler::GetTestServerURL(
    const base::Value::List& list) {
  ASSERT_TRUE(net_internals_test_->StartTestServer());
  const std::string& path = list[0].GetString();
  GURL url = net_internals_test_->embedded_test_server()->GetURL(path);
  base::Value url_value(url.spec());
  RunJavascriptCallback(&url_value);
}

void NetInternalsTest::MessageHandler::SetUpTestReportURI(
    const base::Value::List& list) {
  net_internals_test_->embedded_test_server()->RegisterRequestHandler(
      base::BindRepeating(&HandleExpectCTReportPreflight));
  ASSERT_TRUE(net_internals_test_->embedded_test_server()->Start());
  base::Value report_uri_value(
      net_internals_test_->embedded_test_server()->GetURL("/").spec());
  RunJavascriptCallback(&report_uri_value);
}

void NetInternalsTest::MessageHandler::DnsLookup(
    const base::Value::List& list) {
  ASSERT_GE(2u, list.size());
  ASSERT_TRUE(list[0].is_string());
  ASSERT_TRUE(list[1].is_bool());
  const std::string hostname = list[0].GetString();
  const bool local = list[1].GetBool();
  ASSERT_TRUE(browser());

  auto resolve_host_parameters = network::mojom::ResolveHostParameters::New();
  if (local)
    resolve_host_parameters->source = net::HostResolverSource::LOCAL_ONLY;
  mojo::PendingRemote<network::mojom::ResolveHostClient> client;
  // DnsLookupClient owns itself.
  new DnsLookupClient(client.InitWithNewPipeAndPassReceiver(),
                      base::BindOnce(&MessageHandler::RunJavascriptCallback,
                                     weak_factory_.GetWeakPtr()));
  browser()
      ->profile()
      ->GetDefaultStoragePartition()
      ->GetNetworkContext()
      ->ResolveHost(net::HostPortPair(hostname, 80), network_isolation_key_,
                    std::move(resolve_host_parameters), std::move(client));
}

////////////////////////////////////////////////////////////////////////////////
// NetInternalsTest
////////////////////////////////////////////////////////////////////////////////

NetInternalsTest::NetInternalsTest()
    : test_server_started_(false) {
  message_handler_ = std::make_unique<MessageHandler>(this);
}

NetInternalsTest::~NetInternalsTest() {
}

void NetInternalsTest::SetUpOnMainThread() {
  WebUIBrowserTest::SetUpOnMainThread();
  host_resolver()->AddRule("*.com", "127.0.0.1");
  // TODO(crbug.com/1351249): Enable this test after making
  // RuleBasedHostResolverProc support multiple addresses.
  // host_resolver()->AddRule("multihost.org", "127.0.0.2,127.0.0.3,127.0.0.4");
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
