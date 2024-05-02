// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRONET_URL_REQUEST_CONTEXT_CONFIG_H_
#define COMPONENTS_CRONET_URL_REQUEST_CONTEXT_CONFIG_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/time/time.h"
#include "base/values.h"
#include "net/base/hash_value.h"
#include "net/base/network_handle.h"
#include "net/cert/cert_verifier.h"
#include "net/nqe/effective_connection_type.h"
#include "url/origin.h"

namespace net {
class CertVerifier;
struct HttpNetworkSessionParams;
struct QuicParams;
class URLRequestContextBuilder;
}  // namespace net

namespace cronet {

// Common configuration parameters used by Cronet to configure
// URLRequestContext.
// TODO(mgersh): This shouldn't be a struct, and experimental option parsing
// should be kept more separate from applying the configuration.
struct URLRequestContextConfig {
  // Type of HTTP cache.
  // GENERATED_JAVA_ENUM_PACKAGE: org.chromium.net.impl
  enum HttpCacheType {
    // No HTTP cache.
    DISABLED,
    // HTTP cache persisted to disk.
    DISK,
    // HTTP cache kept in memory.
    MEMORY,
  };

  // App-provided hint that server supports QUIC.
  struct QuicHint {
    QuicHint(const std::string& host, int port, int alternate_port);

    QuicHint(const QuicHint&) = delete;
    QuicHint& operator=(const QuicHint&) = delete;

    ~QuicHint();

    // Host name of the server that supports QUIC.
    const std::string host;
    // Port of the server that supports QUIC.
    const int port;
    // Alternate protocol port.
    const int alternate_port;
  };

  // Public-Key-Pinning configuration structure.
  struct Pkp {
    Pkp(const std::string& host,
        bool include_subdomains,
        const base::Time& expiration_date);

    Pkp(const Pkp&) = delete;
    Pkp& operator=(const Pkp&) = delete;

    ~Pkp();

    // Host name.
    const std::string host;
    // Pin hashes (currently SHA256 only).
    net::HashValueVector pin_hashes;
    // Indicates whether the pinning should apply to the pinned host subdomains.
    const bool include_subdomains;
    // Expiration date for the pins.
    const base::Time expiration_date;
  };

  // Simulated headers, used to preconfigure the Reporting API and Network Error
  // Logging before receiving those actual configuration headers from the
  // origins.
  struct PreloadedNelAndReportingHeader {
    PreloadedNelAndReportingHeader(const url::Origin& origin,
                                   std::string value);
    ~PreloadedNelAndReportingHeader();

    // Origin that is "sending" this header.
    const url::Origin origin;

    // Value of the header that is "sent".
    const std::string value;
  };

  URLRequestContextConfig(const URLRequestContextConfig&) = delete;
  URLRequestContextConfig& operator=(const URLRequestContextConfig&) = delete;

  ~URLRequestContextConfig();

  // Configures |context_builder| based on |this|.
  void ConfigureURLRequestContextBuilder(
      net::URLRequestContextBuilder* context_builder,
      net::handles::NetworkHandle bound_network =
          net::handles::kInvalidNetworkHandle);

  // Enable QUIC.
  const bool enable_quic;
  // Enable SPDY.
  const bool enable_spdy;
  // Enable Brotli.
  const bool enable_brotli;
  // Type of http cache.
  const HttpCacheType http_cache;
  // Max size of http cache in bytes.
  const int http_cache_max_size;
  // Disable caching for HTTP responses. Other information may be stored in
  // the cache.
  const bool load_disable_cache;
  // Storage path for http cache and cookie storage.
  const std::string storage_path;
  // Accept-Language request header field.
  const std::string accept_language;
  // User-Agent request header field.
  const std::string user_agent;

  // Certificate verifier for testing.
  std::unique_ptr<net::CertVerifier> mock_cert_verifier;

  // Enable Network Quality Estimator (NQE).
  const bool enable_network_quality_estimator;

  // Enable public key pinning bypass for local trust anchors.
  const bool bypass_public_key_pinning_for_local_trust_anchors;

  // App-provided list of servers that support QUIC.
  std::vector<std::unique_ptr<QuicHint>> quic_hints;

  // The list of public key pins.
  std::vector<std::unique_ptr<Pkp>> pkp_list;

  // Enable DNS cache persistence.
  bool enable_host_cache_persistence = false;

  // Minimum time in milliseconds between writing the HostCache contents to
  // prefs. Only relevant when |enable_host_cache_persistence| is true.
  int host_cache_persistence_delay_ms = 60000;

  // Experimental options that are recognized by the config parser.
  base::Value::Dict effective_experimental_options;
  base::Value::Dict experimental_options;

  // If set, forces NQE to return the set value as the effective connection
  // type.
  std::optional<net::EffectiveConnectionType>
      nqe_forced_effective_connection_type;

  // Preloaded Report-To headers, to preconfigure the Reporting API.
  std::vector<PreloadedNelAndReportingHeader> preloaded_report_to_headers;

  // Preloaded NEL headers, to preconfigure Network Error Logging.
  std::vector<PreloadedNelAndReportingHeader> preloaded_nel_headers;

  // Optional network thread priority.
  // On Android, corresponds to android.os.Process.setThreadPriority() values.
  const std::optional<int> network_thread_priority;

  // Whether the connection status of active bidirectional streams should be
  // monitored.
  bool bidi_stream_detect_broken_connection;
  // If |bidi_stream_detect_broken_connection_| is true, this suggests the
  // period of the heartbeat signal.
  base::TimeDelta heartbeat_interval;

  static bool ExperimentalOptionsParsingIsAllowedToFail() {
    return DCHECK_IS_ON();
  }

  static std::unique_ptr<URLRequestContextConfig> CreateURLRequestContextConfig(
      // Enable QUIC.
      bool enable_quic,
      // Enable SPDY.
      bool enable_spdy,
      // Enable Brotli.
      bool enable_brotli,
      // Type of http cache.
      HttpCacheType http_cache,
      // Max size of http cache in bytes.
      int http_cache_max_size,
      // Disable caching for HTTP responses. Other information may be stored in
      // the cache.
      bool load_disable_cache,
      // Storage path for http cache and cookie storage.
      const std::string& storage_path,
      // Accept-Language request header field.
      const std::string& accept_language,
      // User-Agent request header field.
      const std::string& user_agent,
      // JSON encoded experimental options.
      const std::string& unparsed_experimental_options,
      // MockCertVerifier to use for testing purposes.
      std::unique_ptr<net::CertVerifier> mock_cert_verifier,
      // Enable network quality estimator.
      bool enable_network_quality_estimator,
      // Enable bypassing of public key pinning for local trust anchors
      bool bypass_public_key_pinning_for_local_trust_anchors,
      // Optional network thread priority.
      // On Android, corresponds to android.os.Process.setThreadPriority()
      // values. Do not specify for other targets.
      std::optional<int> network_thread_priority);

 private:
  URLRequestContextConfig(
      // Enable QUIC.
      bool enable_quic,
      // Enable SPDY.
      bool enable_spdy,
      // Enable Brotli.
      bool enable_brotli,
      // Type of http cache.
      HttpCacheType http_cache,
      // Max size of http cache in bytes.
      int http_cache_max_size,
      // Disable caching for HTTP responses. Other information may be stored in
      // the cache.
      bool load_disable_cache,
      // Storage path for http cache and cookie storage.
      const std::string& storage_path,
      // Accept-Language request header field.
      const std::string& accept_language,
      // User-Agent request header field.
      const std::string& user_agent,
      // Parsed experimental options.
      base::Value::Dict experimental_options,
      // MockCertVerifier to use for testing purposes.
      std::unique_ptr<net::CertVerifier> mock_cert_verifier,
      // Enable network quality estimator.
      bool enable_network_quality_estimator,
      // Enable bypassing of public key pinning for local trust anchors
      bool bypass_public_key_pinning_for_local_trust_anchors,
      // Optional network thread priority.
      // On Android, corresponds to android.os.Process.setThreadPriority()
      // values. Do not specify for other targets.
      std::optional<int> network_thread_priority);

  // Parses experimental options from their JSON format to the format used
  // internally.
  // Returns an empty optional if the operation was unsuccessful.
  static std::optional<base::Value::Dict> ParseExperimentalOptions(
      std::string unparsed_experimental_options);

  // Makes appropriate changes to settings in |this|.
  void SetContextConfigExperimentalOptions();

  // Makes appropriate changes to settings in the URLRequestContextBuilder.
  void SetContextBuilderExperimentalOptions(
      net::URLRequestContextBuilder* context_builder,
      net::HttpNetworkSessionParams* session_params,
      net::QuicParams* quic_params,
      net::handles::NetworkHandle bound_network);
};

// Stores intermediate state for URLRequestContextConfig.  Initializes with
// (mostly) sane defaults, then the appropriate member variables can be
// modified, and it can be finalized with Build().
struct URLRequestContextConfigBuilder {
  URLRequestContextConfigBuilder();

  URLRequestContextConfigBuilder(const URLRequestContextConfigBuilder&) =
      delete;
  URLRequestContextConfigBuilder& operator=(
      const URLRequestContextConfigBuilder&) = delete;

  ~URLRequestContextConfigBuilder();

  // Finalize state into a URLRequestContextConfig.  Must only be called once,
  // as once |mock_cert_verifier| is moved into a URLRequestContextConfig, it
  // cannot be used again.
  std::unique_ptr<URLRequestContextConfig> Build();

  // Enable QUIC.
  bool enable_quic = true;
  // Enable SPDY.
  bool enable_spdy = true;
  // Enable Brotli.
  bool enable_brotli = false;
  // Type of http cache.
  URLRequestContextConfig::HttpCacheType http_cache =
      URLRequestContextConfig::DISABLED;
  // Max size of http cache in bytes.
  int http_cache_max_size = 0;
  // Disable caching for HTTP responses. Other information may be stored in
  // the cache.
  bool load_disable_cache = false;
  // Storage path for http cache and cookie storage.
  std::string storage_path = "";
  // Accept-Language request header field.
  std::string accept_language = "";
  // User-Agent request header field.
  std::string user_agent = "";
  // Experimental options encoded as a string in a JSON format containing
  // experiments and their corresponding configuration options. The format
  // is a JSON object with the name of the experiment as the key, and the
  // configuration options as the value. An example:
  //   {"experiment1": {"option1": "option_value1", "option2": "option_value2",
  //    ...}, "experiment2: {"option3", "option_value3", ...}, ...}
  std::string experimental_options = "{}";

  // Certificate verifier for testing.
  std::unique_ptr<net::CertVerifier> mock_cert_verifier;

  // Enable network quality estimator.
  bool enable_network_quality_estimator = false;

  // Enable public key pinning bypass for local trust anchors.
  bool bypass_public_key_pinning_for_local_trust_anchors = true;

  // Optional network thread priority.
  // On Android, corresponds to android.os.Process.setThreadPriority() values.
  // Do not specify for other targets.
  std::optional<int> network_thread_priority;
};

}  // namespace cronet

#endif  // COMPONENTS_CRONET_URL_REQUEST_CONTEXT_CONFIG_H_
