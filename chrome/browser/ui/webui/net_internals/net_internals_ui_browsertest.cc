// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/net_internals/net_internals_ui_browsertest.h"

#include <algorithm>
#include <utility>

#include "base/command_line.h"
#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/current_thread.h"
#include "base/test/values_test_util.h"
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
#include "net/base/hash_value.h"
#include "net/base/net_errors.h"
#include "net/base/network_isolation_key.h"
#include "net/dns/mock_host_resolver.h"
#include "net/dns/public/host_resolver_source.h"
#include "net/dns/public/resolve_error_info.h"
#include "net/http/transport_security_state.h"
#include "services/network/public/cpp/request_destination.h"
#include "services/network/test/test_network_context.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using content::WebUIMessageHandler;

namespace {

base::Time ToTime(const char* time_string) {
  base::Time time;
  CHECK(base::Time::FromString(time_string, &time));
  return time;
}

std::vector<network::mojom::RequestDestination> ToRequestDestinationList(
    const base::Value::List* list) {
  std::vector<network::mojom::RequestDestination> result;
  if (!list) {
    return result;
  }
  for (const auto& value : *list) {
    CHECK(value.is_string());
    std::optional<network::mojom::RequestDestination> dest =
        network::RequestDestinationFromString(
            value.GetString(),
            network::EmptyRequestDestinationOption::kUseTheEmptyString);
    CHECK(dest);
    result.push_back(*dest);
  }
  std::sort(result.begin(), result.end());
  return result;
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
    receiver_.set_disconnect_handler(base::BindOnce(
        &DnsLookupClient::OnComplete, base::Unretained(this),
        net::ERR_NAME_NOT_RESOLVED, net::ResolveErrorInfo(net::ERR_FAILED),
        /*resolved_addresses=*/std::nullopt,
        /*endpoint_results_with_metadata=*/std::nullopt));
  }
  ~DnsLookupClient() override {}

  // network::mojom::ResolveHostClient:
  void OnComplete(int32_t error,
                  const net::ResolveErrorInfo& resolve_error_info,
                  const std::optional<net::AddressList>& resolved_addresses,
                  const std::optional<net::HostResolverEndpointResults>&
                      endpoint_results_with_metadata) override {
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

class NetworkContextForTesting : public network::TestNetworkContext {
 public:
  NetworkContextForTesting() = default;
  ~NetworkContextForTesting() override = default;

  // This is a mock network context for testing.
  // Only "*.com" is registered to this resolver. And especially for
  // http2/http3/multihost.com, results include endpoint_results_with_metadata
  // as well as resolved_addresses.
  void ResolveHost(
      network::mojom::HostResolverHostPtr host,
      const net::NetworkAnonymizationKey& network_anonymization_key,
      network::mojom::ResolveHostParametersPtr optional_parameters,
      mojo::PendingRemote<network::mojom::ResolveHostClient>
          pending_response_client) override {
    mojo::Remote<network::mojom::ResolveHostClient> response_client(
        std::move(pending_response_client));

    auto hostname = host->get_scheme_host_port().host();

    if (!base::EndsWith(hostname, ".com", base::CompareCase::SENSITIVE)) {
      response_client->OnComplete(
          net::ERR_NAME_NOT_RESOLVED,
          net::ResolveErrorInfo(net::ERR_NAME_NOT_RESOLVED),
          /*resolved_addresses=*/std::nullopt,
          /*endpoint_results_with_metadata=*/std::nullopt);
    }

    const net::IPAddress first_localhost{127, 0, 0, 1};
    const net::IPAddress second_localhost{127, 0, 0, 2};
    const net::IPEndPoint first_ip_endpoint =
        net::IPEndPoint(first_localhost, 0);
    const net::IPEndPoint second_ip_endpoint =
        net::IPEndPoint(second_localhost, 0);
    net::ConnectionEndpointMetadata first_endpoint_metadata;
    net::ConnectionEndpointMetadata second_endpoint_metadata;

    if (hostname == "http2.com") {
      first_endpoint_metadata.supported_protocol_alpns = {"http/1.1", "h2"};
    } else if (hostname == "http3.com") {
      first_endpoint_metadata.supported_protocol_alpns = {"http/1.1", "h2",
                                                          "h3"};
    } else if (hostname == "multihost.com") {
      first_endpoint_metadata.supported_protocol_alpns = {"http/1.1", "h2"};
      second_endpoint_metadata.supported_protocol_alpns = {"http/1.1", "h2",
                                                           "h3"};
    } else if (hostname == "ech.com") {
      first_endpoint_metadata.supported_protocol_alpns = {"http/1.1", "h2"};
      first_endpoint_metadata.ech_config_list = {0x01, 0x02, 0x03, 0x04};
    } else {
      response_client->OnComplete(
          0, net::ResolveErrorInfo(net::OK),
          net::AddressList(first_ip_endpoint),
          /*endpoint_results_with_metadata=*/std::nullopt);
    }

    if (hostname == "multihost.com") {
      net::HostResolverEndpointResults endpoint_results(2);
      endpoint_results[0].ip_endpoints = {first_ip_endpoint};
      endpoint_results[0].metadata = first_endpoint_metadata;
      endpoint_results[1].ip_endpoints = {second_ip_endpoint};
      endpoint_results[1].metadata = second_endpoint_metadata;
      response_client->OnComplete(
          0, net::ResolveErrorInfo(net::OK),
          net::AddressList({first_ip_endpoint, second_ip_endpoint}),
          endpoint_results);
    } else {
      net::HostResolverEndpointResults endpoint_results(1);
      endpoint_results[0].ip_endpoints = {first_ip_endpoint};
      endpoint_results[0].metadata = first_endpoint_metadata;
      response_client->OnComplete(0, net::ResolveErrorInfo(net::OK),
                                  net::AddressList(first_ip_endpoint),
                                  endpoint_results);
    }
  }

  void ClearSharedDictionaryCache(
      base::Time start_time,
      base::Time end_time,
      network::mojom::ClearDataFilterPtr filter,
      ClearSharedDictionaryCacheCallback callback) override {
    // We just cleas all dictionary for testing.
    dictionaries_.clear();
    std::move(callback).Run();
  }

  void ClearSharedDictionaryCacheForIsolationKey(
      const net::SharedDictionaryIsolationKey& isolation_key,
      ClearSharedDictionaryCacheForIsolationKeyCallback callback) override {
    dictionaries_.erase(isolation_key);
    std::move(callback).Run();
  }

  void GetSharedDictionaryUsageInfo(
      GetSharedDictionaryUsageInfoCallback callback) override {
    std::vector<net::SharedDictionaryUsageInfo> info;
    for (const auto& it : dictionaries_) {
      uint64_t total_size_bytes = 0;
      for (const auto& it2 : it.second) {
        total_size_bytes += it2->size;
      }
      info.emplace_back(net::SharedDictionaryUsageInfo{
          .isolation_key = it.first, .total_size_bytes = total_size_bytes});
    }
    std::move(callback).Run(info);
  }

  void GetSharedDictionaryInfo(
      const net::SharedDictionaryIsolationKey& isolation_key,
      GetSharedDictionaryInfoCallback callback) override {
    auto it = dictionaries_.find(isolation_key);
    if (it == dictionaries_.end()) {
      std::move(callback).Run({});
      return;
    }
    std::vector<network::mojom::SharedDictionaryInfoPtr> dicts;
    for (const auto& it2 : it->second) {
      dicts.emplace_back(it2.Clone());
    }
    std::move(callback).Run(std::move(dicts));
  }

  void RegisterTestSharedDictionary(
      const net::SharedDictionaryIsolationKey& isolation_key,
      network::mojom::SharedDictionaryInfoPtr dictionary) {
    auto it = dictionaries_.find(isolation_key);
    if (it == dictionaries_.end()) {
      std::vector<network::mojom::SharedDictionaryInfoPtr> dicts;
      dicts.emplace_back(std::move(dictionary));
      dictionaries_.insert(std::make_pair(isolation_key, std::move(dicts)));
      return;
    }
    it->second.emplace_back(std::move(dictionary));
  }

 private:
  std::map<net::SharedDictionaryIsolationKey,
           std::vector<network::mojom::SharedDictionaryInfoPtr>>
      dictionaries_;
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

  // Performs a DNS lookup. Resolves a Javascript Promise with the host's IP
  // address or an error string.
  void DnsLookup(const base::Value::List& list);
  void DnsLookupFinished(std::string callback_id, base::Value* result);

  // Sets/resets a mock network context for testing.
  void SetNetworkContextForTesting(const base::Value::List& list);
  void ResetNetworkContextForTesting(const base::Value::List& list);

  // Register a test shared dictionary for testing.
  void RgisterTestSharedDictionary(const base::Value::List& list);

  Browser* browser() { return net_internals_test_->browser(); }

  raw_ptr<NetInternalsTest> net_internals_test_;

  NetworkContextForTesting network_context_for_testing_;

  // Single NetworkAnonymizationKey used for all DNS lookups, so repeated
  // lookups use the same cache key.
  net::NetworkAnonymizationKey network_anonymization_key_{
      net::NetworkAnonymizationKey::CreateTransient()};

  base::WeakPtrFactory<MessageHandler> weak_factory_{this};
};

NetInternalsTest::MessageHandler::MessageHandler(
    NetInternalsTest* net_internals_test)
    : net_internals_test_(net_internals_test) {}

void NetInternalsTest::MessageHandler::RegisterMessages() {
  RegisterMessage("dnsLookup", base::BindRepeating(
                                   &NetInternalsTest::MessageHandler::DnsLookup,
                                   weak_factory_.GetWeakPtr()));
  RegisterMessage(
      "setNetworkContextForTesting",
      base::BindRepeating(
          &NetInternalsTest::MessageHandler::SetNetworkContextForTesting,
          weak_factory_.GetWeakPtr()));
  RegisterMessage(
      "resetNetworkContextForTesting",
      base::BindRepeating(
          &NetInternalsTest::MessageHandler::ResetNetworkContextForTesting,
          weak_factory_.GetWeakPtr()));
  RegisterMessage(
      "registerTestSharedDictionary",
      base::BindRepeating(
          &NetInternalsTest::MessageHandler::RgisterTestSharedDictionary,
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
  handler.Run(data);
}

void NetInternalsTest::MessageHandler::DnsLookupFinished(
    std::string callback_id,
    base::Value* result) {
  ResolveJavascriptCallback(callback_id, *result);
}

void NetInternalsTest::MessageHandler::DnsLookup(
    const base::Value::List& list) {
  AllowJavascript();
  ASSERT_GE(3u, list.size());
  ASSERT_TRUE(list[0].is_string());
  ASSERT_TRUE(list[1].is_string());
  ASSERT_TRUE(list[2].is_bool());
  const std::string& callback_id = list[0].GetString();
  const std::string hostname = list[1].GetString();
  const bool local = list[2].GetBool();
  ASSERT_TRUE(browser());

  auto resolve_host_parameters = network::mojom::ResolveHostParameters::New();
  if (local)
    resolve_host_parameters->source = net::HostResolverSource::LOCAL_ONLY;
  mojo::PendingRemote<network::mojom::ResolveHostClient> client;
  // DnsLookupClient owns itself.
  new DnsLookupClient(
      client.InitWithNewPipeAndPassReceiver(),
      base::BindOnce(&NetInternalsTest::MessageHandler::DnsLookupFinished,
                     weak_factory_.GetWeakPtr(), callback_id));
  browser()
      ->profile()
      ->GetDefaultStoragePartition()
      ->GetNetworkContext()
      ->ResolveHost(network::mojom::HostResolverHost::NewHostPortPair(
                        net::HostPortPair(hostname, 80)),
                    network_anonymization_key_,
                    std::move(resolve_host_parameters), std::move(client));
}

void NetInternalsTest::MessageHandler::SetNetworkContextForTesting(
    const base::Value::List& list) {
  NetInternalsUI::SetNetworkContextForTesting(&network_context_for_testing_);
}

void NetInternalsTest::MessageHandler::ResetNetworkContextForTesting(
    const base::Value::List& list) {
  NetInternalsUI::SetNetworkContextForTesting(nullptr);
}

void NetInternalsTest::MessageHandler::RgisterTestSharedDictionary(
    const base::Value::List& list) {
  const std::string* dictionary_json_string = list[0].GetIfString();
  CHECK(dictionary_json_string);
  base::Value::Dict dict = base::test::ParseJsonDict(*dictionary_json_string);
  net::SHA256HashValue hash_value;
  base::HexStringToSpan(*dict.FindString("hash"), hash_value.data);
  const std::string* id_string = dict.FindString("id");
  network_context_for_testing_.RegisterTestSharedDictionary(
      net::SharedDictionaryIsolationKey(
          url::Origin::Create(GURL(*dict.FindString("frame_origin"))),
          net::SchemefulSite(GURL(*dict.FindString("top_frame_site")))),
      network::mojom::SharedDictionaryInfo::New(
          *dict.FindString("match"),
          ToRequestDestinationList(dict.FindList("match_dest")),
          id_string ? *id_string : "", GURL(*dict.FindString("dictionary_url")),
          ToTime(dict.FindString("last_fetch_time")->c_str()),
          ToTime(dict.FindString("response_time")->c_str()),
          base::Seconds(*dict.FindInt("expiration")),
          ToTime(dict.FindString("last_used_time")->c_str()),
          *dict.FindInt("size"), hash_value));
}

////////////////////////////////////////////////////////////////////////////////
// NetInternalsTest
////////////////////////////////////////////////////////////////////////////////

NetInternalsTest::NetInternalsTest() = default;

NetInternalsTest::~NetInternalsTest() = default;

void NetInternalsTest::SetUpOnMainThread() {
  WebUIMochaBrowserTest::SetUpOnMainThread();
  host_resolver()->AddRule("*.com", "127.0.0.1");
}

void NetInternalsTest::OnWebContentsAvailable(
    content::WebContents* web_contents) {
  content::WebUI* web_ui_instance = web_contents->GetWebUI();
  ASSERT_TRUE(web_ui_instance != nullptr);
  web_ui_instance->AddMessageHandler(std::make_unique<MessageHandler>(this));
}
