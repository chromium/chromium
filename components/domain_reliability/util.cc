// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/domain_reliability/util.h"

#include <stddef.h>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/time/default_tick_clock.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "net/base/net_errors.h"

namespace domain_reliability {

namespace {

const struct NetErrorMapping {
  int net_error;
  const char* beacon_status;
} net_error_map[] = {
    {net::OK, "ok"},
    {net::ERR_ABORTED, "aborted"},
    {net::ERR_TIMED_OUT, "tcp.connection.timed_out"},
    {net::ERR_CONNECTION_CLOSED, "tcp.connection.closed"},
    {net::ERR_CONNECTION_RESET, "tcp.connection.reset"},
    {net::ERR_CONNECTION_REFUSED, "tcp.connection.refused"},
    {net::ERR_CONNECTION_ABORTED, "tcp.connection.aborted"},
    {net::ERR_CONNECTION_FAILED, "tcp.connection.failed"},
    {net::ERR_NAME_NOT_RESOLVED, "dns"},
    {net::ERR_SSL_PROTOCOL_ERROR, "ssl.protocol.error"},
    {net::ERR_ADDRESS_INVALID, "tcp.connection.address_invalid"},
    {net::ERR_ADDRESS_UNREACHABLE, "tcp.connection.address_unreachable"},
    {net::ERR_CONNECTION_TIMED_OUT, "tcp.connection.timed_out"},
    {net::ERR_NAME_RESOLUTION_FAILED, "dns"},
    {net::ERR_SSL_PINNED_KEY_NOT_IN_CERT_CHAIN,
     "ssl.cert.pinned_key_not_in_cert_chain"},
    {net::ERR_CERT_COMMON_NAME_INVALID, "ssl.cert.name_invalid"},
    {net::ERR_CERT_DATE_INVALID, "ssl.cert.date_invalid"},
    {net::ERR_CERT_AUTHORITY_INVALID, "ssl.cert.authority_invalid"},
    {net::ERR_CERT_KNOWN_INTERCEPTION_BLOCKED, "ssl.cert.authority_invalid"},
    {net::ERR_CERT_REVOKED, "ssl.cert.revoked"},
    {net::ERR_CERT_INVALID, "ssl.cert.invalid"},
    {net::ERR_EMPTY_RESPONSE, "http.response.empty"},
    {net::ERR_HTTP2_PING_FAILED, "spdy.ping_failed"},
    {net::ERR_HTTP2_PROTOCOL_ERROR, "spdy.protocol"},
    {net::ERR_QUIC_PROTOCOL_ERROR, "quic.protocol"},
    {net::ERR_DNS_MALFORMED_RESPONSE, "dns.protocol"},
    {net::ERR_DNS_SERVER_FAILED, "dns.server"},
    {net::ERR_DNS_TIMED_OUT, "dns.timed_out"},
    {net::ERR_INSECURE_RESPONSE, "ssl"},
    {net::ERR_CONTENT_LENGTH_MISMATCH, "http.response.content_length_mismatch"},
    {net::ERR_INCOMPLETE_CHUNKED_ENCODING,
     "http.response.incomplete_chunked_encoding"},
    {net::ERR_SSL_VERSION_OR_CIPHER_MISMATCH, "ssl.version_or_cipher_mismatch"},
    {net::ERR_BAD_SSL_CLIENT_AUTH_CERT, "ssl.bad_client_auth_cert"},
    {net::ERR_INVALID_CHUNKED_ENCODING,
     "http.response.invalid_chunked_encoding"},
    {net::ERR_RESPONSE_HEADERS_TRUNCATED, "http.response.headers.truncated"},
    {net::ERR_REQUEST_RANGE_NOT_SATISFIABLE,
     "http.request.range_not_satisfiable"},
    {net::ERR_INVALID_RESPONSE, "http.response.invalid"},
    {net::ERR_RESPONSE_HEADERS_MULTIPLE_CONTENT_DISPOSITION,
     "http.response.headers.multiple_content_disposition"},
    {net::ERR_RESPONSE_HEADERS_MULTIPLE_CONTENT_LENGTH,
     "http.response.headers.multiple_content_length"},
    {net::ERR_SSL_UNRECOGNIZED_NAME_ALERT, "ssl.unrecognized_name_alert"}};

bool CanReportFullBeaconURLToCollector(const GURL& beacon_url,
                                       const GURL& collector_url) {
  return beacon_url.DeprecatedGetOriginAsURL() ==
         collector_url.DeprecatedGetOriginAsURL();
}

}  // namespace

// static
bool GetDomainReliabilityBeaconStatus(
    int net_error,
    int http_response_code,
    std::string* beacon_status_out) {
  if (net_error == net::OK) {
    if (http_response_code >= 400 && http_response_code < 600) {
      *beacon_status_out = "http.error";
    } else {
      *beacon_status_out = "ok";
    }
    return true;
  }

  // TODO(juliatuttle): Consider sorting and using binary search?
  for (size_t i = 0; i < std::size(net_error_map); i++) {
    if (net_error_map[i].net_error == net_error) {
      *beacon_status_out = net_error_map[i].beacon_status;
      return true;
    }
  }
  return false;
}

// TODO(juliatuttle): Consider using ALPN instead, if there's a good way to
//                    differentiate HTTP and HTTPS.
std::string GetDomainReliabilityProtocol(
    net::HttpConnectionInfo connection_info,
    bool ssl_info_populated) {
  switch (net::HttpConnectionInfoToCoarse(connection_info)) {
    case net::HttpConnectionInfoCoarse::kHTTP1:
      return ssl_info_populated ? "HTTPS" : "HTTP";
    case net::HttpConnectionInfoCoarse::kHTTP2:
      return "SPDY";
    case net::HttpConnectionInfoCoarse::kQUIC:
      return "QUIC";
    case net::HttpConnectionInfoCoarse::kOTHER:
      return "";
  }
  NOTREACHED_IN_MIGRATION();
  return "";
}

DomainReliabilityUploader::UploadResult GetUploadResultFromResponseDetails(
    int net_error,
    int http_response_code,
    base::TimeDelta retry_after) {
  DomainReliabilityUploader::UploadResult result;
  if (net_error == net::OK && http_response_code == 200) {
    result.status = DomainReliabilityUploader::UploadResult::SUCCESS;
    return result;
  }

  if (net_error == net::OK && http_response_code == 503 &&
      !retry_after.is_zero()) {
    result.status = DomainReliabilityUploader::UploadResult::RETRY_AFTER;
    result.retry_after = retry_after;
    return result;
  }

  result.status = DomainReliabilityUploader::UploadResult::FAILURE;
  return result;
}

// N.B. This uses a std::vector<std::unique_ptr<>> because that's what
// JSONValueConverter uses for repeated fields of any type, and Config uses
// JSONValueConverter to parse JSON configs.
GURL SanitizeURLForReport(
    const GURL& beacon_url,
    const GURL& collector_url,
    const std::vector<std::unique_ptr<std::string>>& path_prefixes) {
  if (CanReportFullBeaconURLToCollector(beacon_url, collector_url))
    return beacon_url.GetAsReferrer();

  std::string path = beacon_url.path();
  const std::string empty_path;
  const std::string* longest_path_prefix = &empty_path;
  for (const auto& path_prefix : path_prefixes) {
    if (path.substr(0, path_prefix->length()) == *path_prefix &&
        path_prefix->length() > longest_path_prefix->length()) {
      longest_path_prefix = path_prefix.get();
    }
  }

  GURL::Replacements replacements;
  replacements.ClearUsername();
  replacements.ClearPassword();
  replacements.SetPathStr(*longest_path_prefix);
  replacements.ClearQuery();
  replacements.ClearRef();
  return beacon_url.ReplaceComponents(replacements);
}

namespace {

class ActualTimer : public MockableTime::Timer {
 public:
  ActualTimer() = default;
  ~ActualTimer() override = default;

  // MockableTime::Timer implementation:
  void Start(const base::Location& posted_from,
             base::TimeDelta delay,
             base::OnceClosure user_task) override {
    base_timer_.Start(posted_from, delay, std::move(user_task));
  }

  void Stop() override { base_timer_.Stop(); }

  bool IsRunning() override { return base_timer_.IsRunning(); }

 private:
  base::OneShotTimer base_timer_;
};

}  // namespace

MockableTime::Timer::~Timer() {}
MockableTime::Timer::Timer() {}

MockableTime::~MockableTime() = default;
MockableTime::MockableTime() = default;

ActualTime::ActualTime() = default;
ActualTime::~ActualTime() = default;

base::Time ActualTime::Now() const {
  return base::Time::Now();
}
base::TimeTicks ActualTime::NowTicks() const {
  return base::TimeTicks::Now();
}

std::unique_ptr<MockableTime::Timer> ActualTime::CreateTimer() {
  return std::unique_ptr<MockableTime::Timer>(new ActualTimer());
}

const base::TickClock* ActualTime::AsTickClock() const {
  return base::DefaultTickClock::GetInstance();
}

}  // namespace domain_reliability
