// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/cronet/ios/Cronet.h"

#include <memory>
#include <vector>

#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/mac/bundle_locations.h"
#include "base/strings/sys_string_conversions.h"
#include "base/synchronization/lock.h"
#include "components/cronet/cronet_global_state.h"
#include "components/cronet/ios/accept_languages_table.h"
#include "components/cronet/ios/cronet_environment.h"
#include "components/cronet/ios/cronet_metrics.h"
#include "components/cronet/native/url_request.h"
#include "components/cronet/url_request_context_config.h"
#include "ios/net/crn_http_protocol_handler.h"
#include "ios/net/empty_nsurlcache.h"
#include "net/base/url_util.h"
#include "net/cert/cert_verifier.h"
#include "net/url_request/url_request_context_getter.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Cronet NSError constants.
NSString* const CRNCronetErrorDomain = @"CRNCronetErrorDomain";
NSString* const CRNInvalidArgumentKey = @"CRNInvalidArgumentKey";

namespace {

class CronetHttpProtocolHandlerDelegate;

using QuicHintVector =
    std::vector<std::unique_ptr<cronet::URLRequestContextConfig::QuicHint>>;
// Currently there is one and only one instance of CronetEnvironment,
// which is leaked at the shutdown. We should consider allowing multiple
// instances if that makes sense in the future.
base::LazyInstance<std::unique_ptr<cronet::CronetEnvironment>>::Leaky
    gChromeNet = LAZY_INSTANCE_INITIALIZER;

base::LazyInstance<std::unique_ptr<CronetHttpProtocolHandlerDelegate>>::Leaky
    gHttpProtocolHandlerDelegate = LAZY_INSTANCE_INITIALIZER;

base::LazyInstance<std::unique_ptr<cronet::CronetMetricsDelegate>>::Leaky
    gMetricsDelegate = LAZY_INSTANCE_INITIALIZER;

// See [Cronet initialize] method to set the default values of the global
// variables.
BOOL gHttp2Enabled;
BOOL gQuicEnabled;
BOOL gBrotliEnabled;
BOOL gMetricsEnabled;
cronet::URLRequestContextConfig::HttpCacheType gHttpCache;
QuicHintVector gQuicHints;
NSString* gExperimentalOptions;
NSString* gUserAgent;
BOOL gUserAgentPartial;
double gNetworkThreadPriority;
NSString* gSslKeyLogFileName;
std::vector<std::unique_ptr<cronet::URLRequestContextConfig::Pkp>> gPkpList;
RequestFilterBlock gRequestFilterBlock;
NSURLCache* gPreservedSharedURLCache;
BOOL gEnableTestCertVerifierForTesting;
std::unique_ptr<net::CertVerifier> gMockCertVerifier;
NSString* gAcceptLanguages;
BOOL gEnablePKPBypassForLocalTrustAnchors;
dispatch_once_t gSwizzleOnceToken;

// CertVerifier, which allows any certificates for testing.
class TestCertVerifier : public net::CertVerifier {
  int Verify(const RequestParams& params,
             net::CertVerifyResult* verify_result,
             net::CompletionOnceCallback callback,
             std::unique_ptr<Request>* out_req,
             const net::NetLogWithSource& net_log) override {
    verify_result->Reset();
    verify_result->verified_cert = params.certificate();
    verify_result->is_issued_by_known_root = true;
    return net::OK;
  }
  void SetConfig(const Config& config) override {}
};

// net::HTTPProtocolHandlerDelegate for Cronet.
class CronetHttpProtocolHandlerDelegate
    : public net::HTTPProtocolHandlerDelegate {
 public:
  CronetHttpProtocolHandlerDelegate(net::URLRequestContextGetter* getter,
                                    RequestFilterBlock filter)
      : getter_(getter), filter_(filter) {}

  void SetRequestFilterBlock(RequestFilterBlock filter) {
    base::AutoLock auto_lock(lock_);
    filter_ = filter;
  }

 private:
  // net::HTTPProtocolHandlerDelegate implementation:
  bool CanHandleRequest(NSURLRequest* request) override {
    base::AutoLock auto_lock(lock_);
    if (!IsRequestSupported(request))
      return false;
    if (filter_)
      return filter_(request);
    return true;
  }

  bool IsRequestSupported(NSURLRequest* request) override {
    NSString* scheme = [[request URL] scheme];
    if (!scheme)
      return false;
    return [scheme caseInsensitiveCompare:@"http"] == NSOrderedSame ||
           [scheme caseInsensitiveCompare:@"https"] == NSOrderedSame;
  }

  net::URLRequestContextGetter* GetDefaultURLRequestContext() override {
    return getter_.get();
  }

  scoped_refptr<net::URLRequestContextGetter> getter_;
  __strong RequestFilterBlock filter_;
  base::Lock lock_;
};

}  // namespace

@implementation Cronet

+ (void)configureCronetEnvironmentForTesting:
    (cronet::CronetEnvironment*)cronetEnvironment {
  if (gEnableTestCertVerifierForTesting) {
    std::unique_ptr<TestCertVerifier> test_cert_verifier =
        std::make_unique<TestCertVerifier>();
    cronetEnvironment->set_mock_cert_verifier(std::move(test_cert_verifier));
  }
  if (gMockCertVerifier) {
    gChromeNet.Get()->set_mock_cert_verifier(std::move(gMockCertVerifier));
  }
}

+ (NSString*)getAcceptLanguagesFromPreferredLanguages:
    (NSArray<NSString*>*)languages {
  NSMutableArray* acceptLanguages = [NSMutableArray new];
  for (NSString* lang_region in languages) {
    NSString* lang = [lang_region componentsSeparatedByString:@"-"][0];
    NSString* localeAcceptLangs = acceptLangs[lang_region] ?: acceptLangs[lang];
    if (localeAcceptLangs)
      [acceptLanguages
          addObjectsFromArray:[localeAcceptLangs
                                  componentsSeparatedByString:@","]];
  }

  NSString* acceptLanguageString =
      [[[NSOrderedSet orderedSetWithArray:acceptLanguages] array]
          componentsJoinedByString:@","];

  return [acceptLanguageString length] != 0 ? acceptLanguageString
                                            : @"en-US,en";
}

+ (NSString*)getAcceptLanguages {
  return [self
      getAcceptLanguagesFromPreferredLanguages:[NSLocale preferredLanguages]];
}

+ (void)setAcceptLanguages:(NSString*)acceptLanguages {
  [self checkNotStarted];
  gAcceptLanguages = acceptLanguages;
}

// TODO(lilyhoughton) this should either be removed, or made more sophisticated
+ (void)checkNotStarted {
  CHECK(!gChromeNet.Get()) << "Cronet is already started.";
}

+ (void)setHttp2Enabled:(BOOL)http2Enabled {
  [self checkNotStarted];
  gHttp2Enabled = http2Enabled;
}

+ (void)setQuicEnabled:(BOOL)quicEnabled {
  [self checkNotStarted];
  gQuicEnabled = quicEnabled;
}

+ (void)setBrotliEnabled:(BOOL)brotliEnabled {
  [self checkNotStarted];
  gBrotliEnabled = brotliEnabled;
}

+ (void)setMetricsEnabled:(BOOL)metricsEnabled {
  // https://crbug.com/878589
  // Don't collect NSURLSessionTaskMetrics until iOS 10.2 to avoid crash in iOS.
  if (@available(iOS 10.2, *)) {
    [self checkNotStarted];
    gMetricsEnabled = metricsEnabled;
  }
}

+ (BOOL)addQuicHint:(NSString*)host port:(int)port altPort:(int)altPort {
  [self checkNotStarted];

  std::string quic_host = base::SysNSStringToUTF8(host);

  url::CanonHostInfo host_info;
  std::string canon_host(net::CanonicalizeHost(quic_host, &host_info));
  if (!host_info.IsIPAddress() &&
      !net::IsCanonicalizedHostCompliant(canon_host)) {
    LOG(ERROR) << "Invalid QUIC hint host: " << quic_host;
    return NO;
  }

  gQuicHints.push_back(
      std::make_unique<cronet::URLRequestContextConfig::QuicHint>(
          quic_host, port, altPort));

  return YES;
}

+ (void)setExperimentalOptions:(NSString*)experimentalOptions {
  [self checkNotStarted];
  gExperimentalOptions = experimentalOptions;
}

+ (void)setUserAgent:(NSString*)userAgent partial:(BOOL)partial {
  [self checkNotStarted];
  gUserAgent = userAgent;
  gUserAgentPartial = partial;
}

+ (void)setSslKeyLogFileName:(NSString*)sslKeyLogFileName {
  [self checkNotStarted];
  gSslKeyLogFileName = [self getNetLogPathForFile:sslKeyLogFileName];
}

+ (void)setHttpCacheType:(CRNHttpCacheType)httpCacheType {
  [self checkNotStarted];
  switch (httpCacheType) {
    case CRNHttpCacheTypeDisabled:
      gHttpCache = cronet::URLRequestContextConfig::HttpCacheType::DISABLED;
      break;
    case CRNHttpCacheTypeDisk:
      gHttpCache = cronet::URLRequestContextConfig::HttpCacheType::DISK;
      break;
    case CRNHttpCacheTypeMemory:
      gHttpCache = cronet::URLRequestContextConfig::HttpCacheType::MEMORY;
      break;
    default:
      DCHECK(NO) << "Invalid HTTP cache type: " << httpCacheType;
  }
}

+ (void)setRequestFilterBlock:(RequestFilterBlock)block {
  if (gHttpProtocolHandlerDelegate.Get().get())
    gHttpProtocolHandlerDelegate.Get().get()->SetRequestFilterBlock(block);
  else
    gRequestFilterBlock = block;
}

+ (BOOL)addPublicKeyPinsForHost:(NSString*)host
                      pinHashes:(NSSet<NSData*>*)pinHashes
              includeSubdomains:(BOOL)includeSubdomains
                 expirationDate:(NSDate*)expirationDate
                          error:(NSError**)outError {
  [self checkNotStarted];

  // Pinning a key only makes sense if pin bypassing has been disabled
  if (gEnablePKPBypassForLocalTrustAnchors) {
    if (outError != nil) {
      *outError =
          [self createUnsupportedConfigurationError:
                    @"Cannot pin keys while public key pinning is bypassed"];
    }
    return NO;
  }

  auto pkp = std::make_unique<cronet::URLRequestContextConfig::Pkp>(
      base::SysNSStringToUTF8(host), includeSubdomains,
      base::Time::FromCFAbsoluteTime(
          [expirationDate timeIntervalSinceReferenceDate]));

  for (NSData* hash in pinHashes) {
    net::SHA256HashValue hashValue = net::SHA256HashValue();
    if (sizeof(hashValue.data) != hash.length) {
      *outError =
          [self createIllegalArgumentErrorWithArgument:@"pinHashes"
                                                reason:
                                                    @"The length of PKP SHA256 "
                                                    @"hash should be 256 bits"];
      return NO;
    }
    memcpy((void*)(hashValue.data), [hash bytes], sizeof(hashValue.data));
    pkp->pin_hashes.push_back(net::HashValue(hashValue));
  }
  gPkpList.push_back(std::move(pkp));
  if (outError) {
    *outError = nil;
  }
  return YES;
}

+ (void)setEnablePublicKeyPinningBypassForLocalTrustAnchors:(BOOL)enable {
  gEnablePKPBypassForLocalTrustAnchors = enable;
}

+ (base::SingleThreadTaskRunner*)getFileThreadRunnerForTesting {
  return gChromeNet.Get()->GetFileThreadRunnerForTesting();
}

+ (base::SingleThreadTaskRunner*)getNetworkThreadRunnerForTesting {
  return gChromeNet.Get()->GetNetworkThreadRunnerForTesting();
}

+ (void)startInternal {
  std::string user_agent = base::SysNSStringToUTF8(gUserAgent);

  gChromeNet.Get().reset(
      new cronet::CronetEnvironment(user_agent, gUserAgentPartial));

  gChromeNet.Get()->set_accept_language(
      base::SysNSStringToUTF8(gAcceptLanguages ?: [self getAcceptLanguages]));

  gChromeNet.Get()->set_http2_enabled(gHttp2Enabled);
  gChromeNet.Get()->set_quic_enabled(gQuicEnabled);
  gChromeNet.Get()->set_brotli_enabled(gBrotliEnabled);
  gChromeNet.Get()->set_experimental_options(
      base::SysNSStringToUTF8(gExperimentalOptions));
  gChromeNet.Get()->set_http_cache(gHttpCache);
  gChromeNet.Get()->set_ssl_key_log_file_name(
      base::SysNSStringToUTF8(gSslKeyLogFileName));
  gChromeNet.Get()->set_pkp_list(std::move(gPkpList));
  gChromeNet.Get()
      ->set_enable_public_key_pinning_bypass_for_local_trust_anchors(
          gEnablePKPBypassForLocalTrustAnchors);
  if (gNetworkThreadPriority !=
      cronet::CronetEnvironment::kKeepDefaultThreadPriority) {
    gChromeNet.Get()->SetNetworkThreadPriority(gNetworkThreadPriority);
  }
  for (const auto& quicHint : gQuicHints) {
    gChromeNet.Get()->AddQuicHint(quicHint->host, quicHint->port,
                                  quicHint->alternate_port);
  }

  [self configureCronetEnvironmentForTesting:gChromeNet.Get().get()];
  gChromeNet.Get()->Start();
  gHttpProtocolHandlerDelegate.Get().reset(
      new CronetHttpProtocolHandlerDelegate(
          gChromeNet.Get()->GetURLRequestContextGetter(), gRequestFilterBlock));
  net::HTTPProtocolHandlerDelegate::SetInstance(
      gHttpProtocolHandlerDelegate.Get().get());

  if (gMetricsEnabled) {
    gMetricsDelegate.Get().reset(new cronet::CronetMetricsDelegate());
    net::MetricsDelegate::SetInstance(gMetricsDelegate.Get().get());

    dispatch_once(&gSwizzleOnceToken, ^{
      cronet::SwizzleSessionWithConfiguration();
    });
  } else {
    net::MetricsDelegate::SetInstance(nullptr);
  }

  gRequestFilterBlock = nil;
}

+ (void)start {
  cronet::EnsureInitialized();
  [self startInternal];
}

+ (void)unswizzleForTesting {
  if (gSwizzleOnceToken)
    cronet::SwizzleSessionWithConfiguration();
  gSwizzleOnceToken = 0;
}

+ (void)shutdownForTesting {
  [Cronet unswizzleForTesting];
  [Cronet initialize];
}

+ (void)registerHttpProtocolHandler {
  if (gPreservedSharedURLCache == nil) {
    gPreservedSharedURLCache = [NSURLCache sharedURLCache];
  }
  // Disable the default cache.
  [NSURLCache setSharedURLCache:[EmptyNSURLCache emptyNSURLCache]];
  // Register the chrome http protocol handler to replace the default one.
  BOOL success =
      [NSURLProtocol registerClass:[CRNHTTPProtocolHandler class]];
  DCHECK(success);
}

+ (void)unregisterHttpProtocolHandler {
  // Set up SharedURLCache preserved in registerHttpProtocolHandler.
  if (gPreservedSharedURLCache != nil) {
    [NSURLCache setSharedURLCache:gPreservedSharedURLCache];
    gPreservedSharedURLCache = nil;
  }
  [NSURLProtocol unregisterClass:[CRNHTTPProtocolHandler class]];
}

+ (void)installIntoSessionConfiguration:(NSURLSessionConfiguration*)config {
  config.protocolClasses = @[ [CRNHTTPProtocolHandler class] ];
}

+ (NSString*)getNetLogPathForFile:(NSString*)fileName {
  return [[[[[NSFileManager defaultManager] URLsForDirectory:NSDocumentDirectory
                                                   inDomains:NSUserDomainMask]
      lastObject] URLByAppendingPathComponent:fileName] path];
}

+ (BOOL)startNetLogToFile:(NSString*)fileName logBytes:(BOOL)logBytes {
  if (gChromeNet.Get().get() && [fileName length] &&
      ![fileName isAbsolutePath]) {
    return gChromeNet.Get()->StartNetLog(
        base::SysNSStringToUTF8([self getNetLogPathForFile:fileName]),
        logBytes);
  }

  return NO;
}

+ (void)stopNetLog {
  if (gChromeNet.Get().get()) {
    gChromeNet.Get()->StopNetLog();
  }
}

+ (NSString*)getUserAgent {
  if (!gChromeNet.Get().get()) {
    return nil;
  }

  return [NSString stringWithCString:gChromeNet.Get()->user_agent().c_str()
                            encoding:[NSString defaultCStringEncoding]];
}

+ (void)setNetworkThreadPriority:(double)priority {
  gNetworkThreadPriority = priority;
  if (gChromeNet.Get()) {
    gChromeNet.Get()->SetNetworkThreadPriority(priority);
  };
}

+ (stream_engine*)getGlobalEngine {
  DCHECK(gChromeNet.Get().get());
  if (gChromeNet.Get().get()) {
    static stream_engine engine;
    engine.obj = gChromeNet.Get()->GetURLRequestContextGetter();
    return &engine;
  }
  return nil;
}

+ (NSData*)getGlobalMetricsDeltas {
  if (!gChromeNet.Get().get()) {
    return nil;
  }
  std::vector<uint8_t> deltas(gChromeNet.Get()->GetHistogramDeltas());
  return [NSData dataWithBytes:deltas.data() length:deltas.size()];
}

+ (void)enableTestCertVerifierForTesting {
  gEnableTestCertVerifierForTesting = YES;
}

+ (void)setMockCertVerifierForTesting:
    (std::unique_ptr<net::CertVerifier>)certVerifier {
  gMockCertVerifier = std::move(certVerifier);
}

+ (void)setHostResolverRulesForTesting:(NSString*)hostResolverRulesForTesting {
  DCHECK(gChromeNet.Get().get());
  gChromeNet.Get()->SetHostResolverRules(
      base::SysNSStringToUTF8(hostResolverRulesForTesting));
}

// This is a private dummy method that prevents the linker from stripping out
// the otherwise unreferenced methods from 'bidirectional_stream.cc'.
+ (void)preventStrippingCronetBidirectionalStream {
  bidirectional_stream_create(NULL, 0, 0);
}

// This is a private dummy method that prevents the linker from stripping out
// the otherwise unreferenced modules from 'native'.
+ (void)preventStrippingNativeCronetModules {
  Cronet_Buffer_Create();
  Cronet_Engine_Create();
  Cronet_UrlRequest_Create();
}

+ (NSError*)createIllegalArgumentErrorWithArgument:(NSString*)argumentName
                                            reason:(NSString*)reason {
  NSMutableDictionary* errorDictionary =
      [[NSMutableDictionary alloc] initWithDictionary:@{
        NSLocalizedDescriptionKey :
            [NSString stringWithFormat:@"Invalid argument: %@", argumentName],
        CRNInvalidArgumentKey : argumentName
      }];
  if (reason) {
    errorDictionary[NSLocalizedFailureReasonErrorKey] = reason;
  }
  return [self createCronetErrorWithCode:CRNErrorInvalidArgument
                                userInfo:errorDictionary];
}

+ (NSError*)createUnsupportedConfigurationError:(NSString*)contradiction {
  NSMutableDictionary* errorDictionary =
      [[NSMutableDictionary alloc] initWithDictionary:@{
        NSLocalizedDescriptionKey : @"Unsupported configuration",
        NSLocalizedRecoverySuggestionErrorKey :
            @"Try disabling Public Key Pinning Bypass before pinning keys.",
        NSLocalizedFailureReasonErrorKey : @"Pinning public keys while local "
                                           @"anchor bypass is enabled is "
                                           @"currently not supported.",
      }];
  if (contradiction) {
    errorDictionary[NSLocalizedFailureReasonErrorKey] = contradiction;
  }

  return [self createCronetErrorWithCode:CRNErrorUnsupportedConfig
                                userInfo:errorDictionary];
}

+ (NSError*)createCronetErrorWithCode:(int)errorCode
                             userInfo:(NSDictionary*)userInfo {
  return [NSError errorWithDomain:CRNCronetErrorDomain
                             code:errorCode
                         userInfo:userInfo];
}

// Used by tests to query the size of the map that contains metrics for
// individual NSURLSession tasks.
+ (size_t)getMetricsMapSize {
  return cronet::CronetMetricsDelegate::GetMetricsMapSize();
}

// Static class initializer.
+ (void)initialize {
  gChromeNet.Get().reset();
  gHttp2Enabled = YES;
  gQuicEnabled = NO;
  gBrotliEnabled = NO;
  gMetricsEnabled = NO;
  gHttpCache = cronet::URLRequestContextConfig::HttpCacheType::DISK;
  gQuicHints.clear();
  gExperimentalOptions = @"{}";
  gUserAgent = nil;
  gUserAgentPartial = NO;
  gNetworkThreadPriority =
      cronet::CronetEnvironment::kKeepDefaultThreadPriority;
  gSslKeyLogFileName = nil;
  gPkpList.clear();
  gRequestFilterBlock = nil;
  gHttpProtocolHandlerDelegate.Get().reset(nullptr);
  gMetricsDelegate.Get().reset(nullptr);
  gPreservedSharedURLCache = nil;
  gEnableTestCertVerifierForTesting = NO;
  gMockCertVerifier.reset(nullptr);
  gAcceptLanguages = nil;
  gEnablePKPBypassForLocalTrustAnchors = YES;
}

@end
