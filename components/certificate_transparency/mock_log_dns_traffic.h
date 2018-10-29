// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CERTIFICATE_TRANSPARENCY_MOCK_LOG_DNS_TRAFFIC_H_
#define COMPONENTS_CERTIFICATE_TRANSPARENCY_MOCK_LOG_DNS_TRAFFIC_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/strings/string_piece.h"
#include "net/dns/dns_client.h"
#include "net/socket/socket_test_util.h"

namespace net {
struct DnsConfig;
}

namespace certificate_transparency {

// Mocks DNS requests and responses for a Certificate Transparency (CT) log.
// This is implemented using mock sockets. Call the CreateDnsClient() method to
// get a net::DnsClient wired up to these mock sockets.
// The Expect*() methods must be called from within a GTest test case.
//
// Example Usage:
// // net::DnsClient requires an I/O message loop for async operations.
// base::MessageLoopForIO message_loop;
//
// // Create a mock NetworkChangeNotifier to propagate DNS config.
// std::unique_ptr<net::NetworkChangeNotifier> net_change_notifier(
//     net::NetworkChangeNotifier::CreateMock());
//
// MockLogDnsTraffic mock_dns;
// mock_dns.InitializeDnsConfig();
// // Use the Expect* methods to define expected DNS requests and responses.
// mock_dns.ExpectLeafIndexRequestAndResponse(
//     "D4S6DSV2J743QJZEQMH4UYHEYK7KRQ5JIQOCPMFUHZVJNFGHXACA.hash.ct.test.",
//     "123456");
//
// LogDnsClient log_client(mock_dns.CreateDnsClient(), ...);
// log_client.QueryAuditProof("ct.test", ..., base::Bind(...));
class MockLogDnsTraffic {
 public:
  MockLogDnsTraffic();
  ~MockLogDnsTraffic();

  // Expect a CT DNS request for the domain |qname|.
  // Such a request will receive a DNS response indicating that the error
  // specified by |rcode| occurred. See RFC1035, Section 4.1.1 for |rcode|
  // values.
  // Returns false if any of the arguments are invalid.
  WARN_UNUSED_RESULT
  bool ExpectRequestAndErrorResponse(base::StringPiece qname, uint8_t rcode);

  // Expect a CT DNS request for the domain |qname|.
  // Such a request will trigger a socket error of type |error|.
  // Returns false if any of the arguments are invalid.
  WARN_UNUSED_RESULT
  bool ExpectRequestAndSocketError(base::StringPiece qname, net::Error error);

  // Expect a CT DNS request for the domain |qname|.
  // Such a request will timeout.
  // This will reduce the DNS timeout to minimize test duration.
  // Returns false if |qname| is invalid.
  WARN_UNUSED_RESULT
  bool ExpectRequestAndTimeout(base::StringPiece qname);

  // Expect a CT DNS request for the domain |qname|.
  // Such a request will receive a DNS TXT response containing |txt_strings|.
  // Returns false if any of the arguments are invalid.
  WARN_UNUSED_RESULT
  bool ExpectRequestAndResponse(
      base::StringPiece qname,
      const std::vector<base::StringPiece>& txt_strings);

  // Expect a CT DNS request for the domain |qname|.
  // Such a request will receive a DNS response containing |leaf_index|.
  // A description of such a request and response can be seen here:
  // https://github.com/google/certificate-transparency-rfcs/blob/c8844de6bd0b5d3d16bac79865e6edef533d760b/dns/draft-ct-over-dns.md#hash-query-hashquery
  // Returns false if any of the arguments are invalid.
  WARN_UNUSED_RESULT
  bool ExpectLeafIndexRequestAndResponse(base::StringPiece qname,
                                         uint64_t leaf_index);

  // Expect a CT DNS request for the domain |qname|.
  // Such a request will receive a DNS response containing the inclusion proof
  // nodes between |audit_path_start| and |audit_path_end|.
  // A description of such a request and response can be seen here:
  // https://github.com/google/certificate-transparency-rfcs/blob/c8844de6bd0b5d3d16bac79865e6edef533d760b/dns/draft-ct-over-dns.md#tree-query-treequery
  // Returns false if any of the arguments are invalid.
  WARN_UNUSED_RESULT
  bool ExpectAuditProofRequestAndResponse(
      base::StringPiece qname,
      std::vector<std::string>::const_iterator audit_path_start,
      std::vector<std::string>::const_iterator audit_path_end);

  // Sets the initial DNS config appropriate for testing.
  // Requires that net::NetworkChangeNotifier is initialized first.
  // The DNS config is propogated to NetworkChangeNotifier::DNSObservers
  // asynchronously.
  void InitializeDnsConfig();

  // Sets the DNS config to |config|.
  // Requires that net::NetworkChangeNotifier is initialized first.
  // The DNS config is propogated to NetworkChangeNotifier::DNSObservers
  // asynchronously.
  void SetDnsConfig(const net::DnsConfig& config);

  // Creates a DNS client that uses mock sockets.
  // It is this DNS client that the expectations will be tested against.
  std::unique_ptr<net::DnsClient> CreateDnsClient();

 private:
  // Allows tests to change socket read mode. Only the LogDnsClient tests should
  // need to do so, to ensure consistent behaviour regardless of mode.
  friend class LogDnsClientTest;

  class MockSocketData;

  // Sets whether mock reads should complete synchronously or asynchronously.
  // By default, they complete asynchronously.
  void SetSocketReadMode(net::IoMode read_mode) {
    socket_read_mode_ = read_mode;
  }

  // Constructs MockSocketData from |args| and adds it to |socket_factory_|.
  template <typename... Args>
  void EmplaceMockSocketData(Args&&... args);

  // Sets the timeout used for DNS queries.
  // Requires that net::NetworkChangeNotifier is initialized first.
  // The new timeout is propogated to NetworkChangeNotifier::DNSObservers
  // asynchronously.
  void SetDnsTimeout(const base::TimeDelta& timeout);

  // One MockSocketData for each socket that is created. This corresponds to one
  // for each DNS request sent.
  std::vector<std::unique_ptr<MockSocketData>> mock_socket_data_;
  // Provides as many mock sockets as there are entries in |mock_socket_data_|.
  net::MockClientSocketFactory socket_factory_;
  // Controls whether mock socket reads are asynchronous.
  net::IoMode socket_read_mode_;

  DISALLOW_COPY_AND_ASSIGN(MockLogDnsTraffic);
};

}  // namespace certificate_transparency

#endif  // COMPONENTS_CERTIFICATE_TRANSPARENCY_MOCK_LOG_DNS_TRAFFIC_H_
