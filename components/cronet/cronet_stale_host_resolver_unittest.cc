// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/test/task_environment.h"
#include "base/test/test_timeouts.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/cronet/url_request_context_config.h"
#include "net/base/address_family.h"
#include "net/base/host_port_pair.h"
#include "net/base/ip_endpoint.h"
#include "net/base/mock_network_change_notifier.h"
#include "net/base/net_errors.h"
#include "net/base/network_anonymization_key.h"
#include "net/base/network_change_notifier.h"
#include "net/cert/cert_verifier.h"
#include "net/dns/context_host_resolver.h"
#include "net/dns/dns_config.h"
#include "net/dns/dns_hosts.h"
#include "net/dns/dns_test_util.h"
#include "net/dns/host_cache.h"
#include "net/dns/host_resolver_manager.h"
#include "net/dns/host_resolver_proc.h"
#include "net/dns/host_resolver_system_task.h"
#include "net/dns/public/dns_protocol.h"
#include "net/dns/public/dns_query_type.h"
#include "net/dns/public/host_resolver_source.h"
#include "net/dns/stale_host_resolver.h"
#include "net/http/http_network_session.h"
#include "net/log/net_log_with_source.h"
#include "net/proxy_resolution/proxy_config.h"
#include "net/proxy_resolution/proxy_config_service_fixed.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cronet {

namespace {

const char kHostname[] = "example.com";
const char kCacheAddress[] = "1.1.1.1";
const char kNetworkAddress[] = "2.2.2.2";
const int kCacheEntryTTLSec = 300;
const uint16_t kPort = 12345;
const int kAgeExpiredSec = kCacheEntryTTLSec * 2;

// How long to wait for resolve calls to return. If the tests are working
// correctly, we won't end up waiting this long -- it's just a backup.
const int kWaitTimeoutSec = 1;

std::vector<net::IPEndPoint> MakeEndpoints(const char* ip_address_str) {
  net::IPAddress address;
  bool rv = address.AssignFromIPLiteral(ip_address_str);
  DCHECK(rv);
  return std::vector<net::IPEndPoint>({{address, 0}});
}

net::AddressList MakeAddressList(const char* ip_address_str) {
  return net::AddressList(MakeEndpoints(ip_address_str));
}

class MockHostResolverProc : public net::HostResolverProc {
 public:
  // |result| is the net error code to return from resolution attempts.
  explicit MockHostResolverProc(int result)
      : HostResolverProc(nullptr), result_(result) {}

  int Resolve(const std::string& hostname,
              net::AddressFamily address_family,
              net::HostResolverFlags host_resolver_flags,
              net::AddressList* address_list,
              int* os_error) override {
    *address_list = MakeAddressList(kNetworkAddress);
    return result_;
  }

 protected:
  ~MockHostResolverProc() override = default;

 private:
  // Result code to return from Resolve().
  const int result_;
};

class CronetStaleHostResolverTest : public testing::Test {
 protected:
  CronetStaleHostResolverTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::IO),
        mock_network_change_notifier_(
            net::test::MockNetworkChangeNotifier::Create()),
        mock_proc_(new MockHostResolverProc(net::OK)),
        resolver_(nullptr) {
    // Make value clock not empty.
    tick_clock_.Advance(base::Microseconds(1));
  }

  ~CronetStaleHostResolverTest() override = default;

  void SetStaleDelay(int stale_delay_sec) {
    DCHECK(!resolver_);

    options_.delay = base::Seconds(stale_delay_sec);
  }

  void SetUseStaleOnNameNotResolved() {
    DCHECK(!resolver_);

    options_.use_stale_on_name_not_resolved = true;
  }

  void SetStaleUsability(int max_expired_time_sec,
                         int max_stale_uses,
                         bool allow_other_network) {
    DCHECK(!resolver_);

    options_.max_expired_time = base::Seconds(max_expired_time_sec);
    options_.max_stale_uses = max_stale_uses;
    options_.allow_other_network = allow_other_network;
  }

  void SetNetResult(int result) {
    DCHECK(!resolver_);

    mock_proc_ = new MockHostResolverProc(result);
  }

  std::unique_ptr<net::ContextHostResolver>
  CreateMockInnerResolverWithDnsClient(
      std::unique_ptr<net::DnsClient> dns_client,
      net::URLRequestContext* context = nullptr) {
    std::unique_ptr<net::ContextHostResolver> inner_resolver(
        net::HostResolver::CreateStandaloneContextResolver(nullptr));
    if (context) {
      inner_resolver->SetRequestContext(context);
    }

    net::HostResolverSystemTask::Params system_params(mock_proc_, 1u);
    inner_resolver->SetHostResolverSystemParamsForTest(system_params);
    if (dns_client) {
      inner_resolver->GetManagerForTesting()->SetDnsClientForTesting(
          std::move(dns_client));
      inner_resolver->GetManagerForTesting()->SetInsecureDnsClientEnabled(
          /*enabled=*/true,
          /*additional_dns_types_enabled=*/true);
    } else {
      inner_resolver->GetManagerForTesting()->SetInsecureDnsClientEnabled(
          /*enabled=*/false,
          /*additional_dns_types_enabled=*/false);
    }
    return inner_resolver;
  }

  void CreateResolverWithDnsClient(std::unique_ptr<net::DnsClient> dns_client) {
    DCHECK(!resolver_);

    stale_resolver_ = std::make_unique<net::StaleHostResolver>(
        CreateMockInnerResolverWithDnsClient(std::move(dns_client)), options_);
    stale_resolver_->SetTickClockForTesting(&tick_clock_);
    resolver_ = stale_resolver_.get();
  }

  void CreateResolver() { CreateResolverWithDnsClient(nullptr); }

  void DestroyResolver() {
    DCHECK(stale_resolver_);

    resolver_ = nullptr;
    stale_resolver_.reset();
  }

  void SetResolver(net::StaleHostResolver* stale_resolver,
                   net::URLRequestContext* context = nullptr) {
    DCHECK(!resolver_);
    stale_resolver->set_inner_resolver_for_testing(
        CreateMockInnerResolverWithDnsClient(nullptr /* dns_client */,
                                             context));
    resolver_ = stale_resolver;
  }

  void DropResolver() { resolver_ = nullptr; }

  // Creates a cache entry for |kHostname| that is |age_sec| seconds old.
  void CreateCacheEntry(int age_sec, int error) {
    DCHECK(resolver_);
    DCHECK(resolver_->GetHostCache());

    base::TimeDelta ttl(base::Seconds(kCacheEntryTTLSec));
    net::HostCache::Key key(kHostname, net::DnsQueryType::UNSPECIFIED, 0,
                            net::HostResolverSource::ANY,
                            net::NetworkAnonymizationKey());
    net::HostCache::Entry entry(
        error,
        error == net::OK ? MakeEndpoints(kCacheAddress)
                         : std::vector<net::IPEndPoint>(),
        /*aliases=*/{}, net::HostCache::Entry::SOURCE_UNKNOWN, ttl);
    base::TimeDelta age = base::Seconds(age_sec);
    base::TimeTicks then = tick_clock_.NowTicks() - age;
    resolver_->GetHostCache()->Set(key, entry, then, ttl);
  }

  void OnNetworkChange() {
    // Real network changes on Android will send both notifications.
    net::NetworkChangeNotifier::NotifyObserversOfIPAddressChangeForTests();
    net::NetworkChangeNotifier::NotifyObserversOfDNSChangeForTests();
    base::RunLoop().RunUntilIdle();  // Wait for notification.
  }

  void LookupStale() {
    DCHECK(resolver_);
    DCHECK(resolver_->GetHostCache());

    net::HostCache::Key key(kHostname, net::DnsQueryType::UNSPECIFIED, 0,
                            net::HostResolverSource::ANY,
                            net::NetworkAnonymizationKey());
    base::TimeTicks now = tick_clock_.NowTicks();
    net::HostCache::EntryStaleness stale;
    EXPECT_TRUE(resolver_->GetHostCache()->LookupStale(key, now, &stale));
    EXPECT_TRUE(stale.is_stale());
  }

  void Resolve(
      const std::optional<net::StaleHostResolver::ResolveHostParameters>&
          optional_parameters) {
    DCHECK(resolver_);
    EXPECT_FALSE(resolve_pending_);

    request_ = resolver_->CreateRequest(
        net::HostPortPair(kHostname, kPort), net::NetworkAnonymizationKey(),
        net::NetLogWithSource(), optional_parameters);
    resolve_pending_ = true;
    resolve_complete_ = false;
    resolve_error_ = net::ERR_UNEXPECTED;

    int rv = request_->Start(
        base::BindOnce(&CronetStaleHostResolverTest::OnResolveComplete,
                       base::Unretained(this)));
    if (rv != net::ERR_IO_PENDING) {
      resolve_pending_ = false;
      resolve_complete_ = true;
      resolve_error_ = rv;
    }
  }

  void WaitForResolve() {
    if (!resolve_pending_) {
      return;
    }

    base::RunLoop run_loop;

    // Run until resolve completes or timeout.
    resolve_closure_ = run_loop.QuitWhenIdleClosure();
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, resolve_closure_, base::Seconds(kWaitTimeoutSec));
    run_loop.Run();
  }

  void WaitForIdle() {
    base::RunLoop run_loop;

    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, run_loop.QuitWhenIdleClosure());
    run_loop.Run();
  }

  void WaitForNetworkResolveComplete() {
    // The stale host resolver cache is initially setup with |kCacheAddress|,
    // so getting that address means that network resolve is still pending.
    // The network resolve is guaranteed to return |kNetworkAddress| at some
    // point because inner resolver is using MockHostResolverProc that always
    // returns |kNetworkAddress|.
    while (resolve_error() != net::OK ||
           resolve_addresses()[0].ToStringWithoutPort() != kNetworkAddress) {
      Resolve(std::nullopt);
      WaitForResolve();
    }
  }

  void Cancel() {
    DCHECK(resolver_);
    EXPECT_TRUE(resolve_pending_);

    request_ = nullptr;

    resolve_pending_ = false;
  }

  void OnResolveComplete(int error) {
    EXPECT_TRUE(resolve_pending_);

    resolve_error_ = error;
    resolve_pending_ = false;
    resolve_complete_ = true;

    if (!resolve_closure_.is_null()) {
      std::move(resolve_closure_).Run();
    }
  }

  void AdvanceTickClock(base::TimeDelta delta) { tick_clock_.Advance(delta); }

  bool resolve_complete() const { return resolve_complete_; }
  int resolve_error() const { return resolve_error_; }
  const net::AddressList& resolve_addresses() const {
    DCHECK(resolve_complete_);
    return *request_->GetAddressResults();
  }

 private:
  // Needed for HostResolver to run HostResolverProc callbacks.
  base::test::TaskEnvironment task_environment_;
  base::SimpleTestTickClock tick_clock_;
  std::unique_ptr<net::test::MockNetworkChangeNotifier>
      mock_network_change_notifier_;

  scoped_refptr<MockHostResolverProc> mock_proc_;

  net::StaleHostResolver::StaleOptions options_;

  // Must outlive `resolver_`.
  std::unique_ptr<net::StaleHostResolver> stale_resolver_;

  raw_ptr<net::HostResolver> resolver_;

  base::TimeTicks now_;
  std::unique_ptr<net::HostResolver::ResolveHostRequest> request_;
  bool resolve_pending_{false};
  bool resolve_complete_{false};
  int resolve_error_;

  base::RepeatingClosure resolve_closure_;
};

TEST_F(CronetStaleHostResolverTest, CreatedByContext) {
  std::unique_ptr<URLRequestContextConfig> config =
      URLRequestContextConfig::CreateURLRequestContextConfig(
          // Enable QUIC.
          true,
          // Enable SPDY.
          true,
          // Enable Brotli.
          false,
          // Type of http cache.
          URLRequestContextConfig::HttpCacheType::DISK,
          // Max size of http cache in bytes.
          1024000,
          // Disable caching for HTTP responses. Other information may be stored
          // in the cache.
          false,
          // Storage path for http cache and cookie storage.
          "/data/data/org.chromium.net/app_cronet_test/test_storage",
          // Accept-Language request header field.
          "foreign-language",
          // User-Agent request header field.
          "fake agent",
          // JSON encoded experimental options.
          "{\"AsyncDNS\":{\"enable\":false},"
          "\"StaleDNS\":{\"enable\":true,"
          "\"delay_ms\":0,"
          "\"max_expired_time_ms\":0,"
          "\"max_stale_uses\":0}}",
          // MockCertVerifier to use for testing purposes.
          std::unique_ptr<net::CertVerifier>(),
          // Enable network quality estimator.
          false,
          // Enable Public Key Pinning bypass for local trust anchors.
          true,
          // Optional network thread priority.
          std::nullopt);

  net::URLRequestContextBuilder builder;
  config->ConfigureURLRequestContextBuilder(&builder);
  // Set a ProxyConfigService to avoid DCHECK failure when building.
  builder.set_proxy_config_service(
      std::make_unique<net::ProxyConfigServiceFixed>(
          net::ProxyConfigWithAnnotation::CreateDirect()));
  std::unique_ptr<net::URLRequestContext> context(builder.Build());

  // Experimental options ensure context's resolver is a net::StaleHostResolver.
  SetResolver(static_cast<net::StaleHostResolver*>(context->host_resolver()),
              context.get());
  // Note: Experimental config above sets 0ms stale delay.
  CreateCacheEntry(kAgeExpiredSec, net::OK);

  Resolve(std::nullopt);
  EXPECT_FALSE(resolve_complete());
  WaitForResolve();

  EXPECT_TRUE(resolve_complete());
  EXPECT_EQ(net::OK, resolve_error());
  EXPECT_EQ(1u, resolve_addresses().size());
  EXPECT_EQ(kCacheAddress, resolve_addresses()[0].ToStringWithoutPort());
  WaitForNetworkResolveComplete();

  // Drop reference to resolver owned by local `context` above before
  // it goes out-of-scope.
  DropResolver();
}

}  // namespace

}  // namespace cronet
