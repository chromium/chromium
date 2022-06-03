// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>

#include "bidirectional_stream_c.h"
#include "cronet.idl_c.h"
#include "cronet_c.h"
#include "cronet_export.h"

// Type of HTTP cache; public interface to private implementation defined in
// URLRequestContextConfig class.
typedef NS_ENUM(NSInteger, CRNHttpCacheType) {
  // Disabled HTTP cache.  Some data may still be temporarily stored in memory.
  CRNHttpCacheTypeDisabled,
  // Enable on-disk HTTP cache, including HTTP data.
  CRNHttpCacheTypeDisk,
  // Enable in-memory cache, including HTTP data.
  CRNHttpCacheTypeMemory,
};

/// Cronet error domain name.
FOUNDATION_EXPORT GRPC_SUPPORT_EXPORT NSString* const CRNCronetErrorDomain;

/// Enum of Cronet NSError codes.
NS_ENUM(NSInteger){
    CRNErrorInvalidArgument = 1001, CRNErrorUnsupportedConfig = 1002,
};

/// The corresponding value is a String object that contains the name of
/// an invalid argument inside the NSError userInfo dictionary.
FOUNDATION_EXPORT GRPC_SUPPORT_EXPORT NSString* const CRNInvalidArgumentKey;

// A block, that takes a request, and returns YES if the request should
// be handled.
typedef BOOL (^RequestFilterBlock)(NSURLRequest* request);

// Interface for installing Cronet.
// TODO(gcasto): Should this macro be separate from the one defined in
// bidirectional_stream_c.h?
GRPC_SUPPORT_EXPORT
@interface Cronet : NSObject

// Sets the HTTP Accept-Language header.  This method only has any effect before
// |start| is called.
+ (void)setAcceptLanguages:(NSString*)acceptLanguages;

// Sets whether HTTP/2 should be supported by CronetEngine. This method only has
// any effect before |start| is called.
+ (void)setHttp2Enabled:(BOOL)http2Enabled;

// Sets whether QUIC should be supported by CronetEngine. This method only has
// any effect before |start| is called.
+ (void)setQuicEnabled:(BOOL)quicEnabled;

// Sets whether Brotli should be supported by CronetEngine. This method only has
// any effect before |start| is called.
+ (void)setBrotliEnabled:(BOOL)brotliEnabled;

// Sets whether Metrics should be collected by CronetEngine. This method only
// has any effect before |start| is called.
+ (void)setMetricsEnabled:(BOOL)metricsEnabled;

// Set HTTP Cache type to be used by CronetEngine.  This method only has any
// effect before |start| is called.  See HttpCacheType enum for available
// options.
+ (void)setHttpCacheType:(CRNHttpCacheType)httpCacheType;

// Adds hint that host supports QUIC on altPort. This method only has any effect
// before |start| is called.  Returns NO if it fails to add hint (because the
// host is invalid).
+ (BOOL)addQuicHint:(NSString*)host port:(int)port altPort:(int)altPort;

// Set experimental Cronet options.  Argument is a JSON string; see
// |URLRequestContextConfig| for more details.  This method only has
// any effect before |start| is called.
+ (void)setExperimentalOptions:(NSString*)experimentalOptions;

// Sets the User-Agent request header string to be sent with all requests.
// If |partial| is set to YES, then actual user agent value is based on device
// model, OS version, and |userAgent| argument. For example "Foo/3.0.0.0" is
// sent as "Mozilla/5.0 (iPhone; CPU iPhone OS 9_3 like Mac OS X)
// AppleWebKit/601.1 (KHTML, like Gecko) Foo/3.0.0.0 Mobile/15G31
// Safari/601.1.46".
// If |partial| is set to NO, then |userAgent| value is complete value sent to
// the remote. For Example: "Foo/3.0.0.0" is sent as "Foo/3.0.0.0".
//
// This method only has any effect before |start| is called.
+ (void)setUserAgent:(NSString*)userAgent partial:(BOOL)partial;

// Sets SSLKEYLogFileName to export SSL key for Wireshark decryption of packet
// captures. This method only has any effect before |start| is called.
+ (void)setSslKeyLogFileName:(NSString*)sslKeyLogFileName;

/// Pins a set of public keys for a given host. This method only has any effect
/// before |start| is called. By pinning a set of public keys, |pinHashes|,
/// communication with |host| is required to authenticate with a certificate
/// with a public key from the set of pinned ones.
/// An app can pin the public key of the root certificate, any of the
/// intermediate certificates or the end-entry certificate. Authentication will
/// fail and secure communication will not be established if none of the public
/// keys is present in the host's certificate chain, even if the host attempts
/// to authenticate with a certificate allowed by the device's trusted store of
/// certificates.
///
/// Calling this method multiple times with the same host name overrides the
/// previously set pins for the host.
///
/// More information about the public key pinning can be found in
/// [RFC 7469](https://tools.ietf.org/html/rfc7469).
///
/// @param host name of the host to which the public keys should be pinned.
///             A host that consists only of digits and the dot character
///             is treated as invalid.
/// @param pinHashes a set of pins. Each pin is the SHA-256 cryptographic
///                  hash of the DER-encoded ASN.1 representation of the
///                  Subject Public Key Info (SPKI) of the host's X.509
///                  certificate. Although, the method does not mandate the
///                  presence of the backup pin that can be used if the control
///                  of the primary private key has been lost, it is highly
///                  recommended to supply one.
/// @param includeSubdomains indicates whether the pinning policy should be
///                          applied to subdomains of |host|.
/// @param expirationDate specifies the expiration date for the pins.
/// @param outError on return, if the pin cannot be added, a pointer to an
///                 error object that encapsulates the reason for the error.
/// @return returns |YES| if the pins were added successfully; |NO|, otherwise.
+ (BOOL)addPublicKeyPinsForHost:(NSString*)host
                      pinHashes:(NSSet<NSData*>*)pinHashes
              includeSubdomains:(BOOL)includeSubdomains
                 expirationDate:(NSDate*)expirationDate
                          error:(NSError**)outError;

// Sets the block used to determine whether or not Cronet should handle the
// request. If the block is not set, Cronet will handle all requests. Cronet
// retains strong reference to the block, which can be released by calling this
// method with nil block.
+ (void)setRequestFilterBlock:(RequestFilterBlock)block;

// Starts CronetEngine. It is recommended to call this method on the application
// main thread. If the method is called on any thread other than the main one,
// the method will internally try to execute synchronously using the main GCD
// queue. Please make sure that the main thread is not blocked by a job
// that calls this method; otherwise, a deadlock can occur.
+ (void)start;

// Registers Cronet as HttpProtocol Handler. Once registered, Cronet intercepts
// and handles all requests made through NSURLConnection and shared
// NSURLSession.
// This method must be called after |start|.
+ (void)registerHttpProtocolHandler;

// Unregister Cronet as HttpProtocol Handler. This means that Cronet will stop
// intercepting requests, however, it won't tear down the Cronet environment.
// This method must be called after |start|.
+ (void)unregisterHttpProtocolHandler;

// Installs Cronet into NSURLSessionConfiguration so that all
// NSURLSessions created with this configuration will use the Cronet stack.
// Note that all Cronet settings are global and are shared between
// all NSURLSessions & NSURLConnections that use the Cronet stack.
// This method must be called after |start|.
+ (void)installIntoSessionConfiguration:(NSURLSessionConfiguration*)config;

// Returns the absolute path that startNetLogToFile:fileName will actually
// write to.
+ (NSString*)getNetLogPathForFile:(NSString*)fileName;

// Starts net-internals logging to a file named |fileName|. Where fileName is
// relative to the application documents directory. |fileName| must not be
// empty. Log level is determined by |logBytes| - if YES then LOG_ALL otherwise
// LOG_ALL_BUT_BYTES. If the file exists it is truncated before starting. If
// actively logging the call is ignored.
+ (BOOL)startNetLogToFile:(NSString*)fileName logBytes:(BOOL)logBytes;

// Stop net-internals logging and flush file to disk. If a logging session is
// not in progress this call is ignored.
+ (void)stopNetLog;

// Returns the full user-agent that will be used unless it is overridden on the
// NSURLRequest used.
+ (NSString*)getUserAgent;

// Sets priority of the network thread. The |priority| should be a
// floating point number between 0.0 to 1.0, where 1.0 is highest priority.
// This method can be called multiple times before or after |start| method.
+ (void)setNetworkThreadPriority:(double)priority;

// Get a pointer to global instance of cronet_engine for GRPC C API.
+ (stream_engine*)getGlobalEngine;

// Returns differences in metrics collected by Cronet since the last call to
// getGlobalMetricsDeltas, serialized as a [protobuf]
// (https://developers.google.com/protocol-buffers).
//
// Cronet starts collecting these metrics after the first call to
// getGlobalMetricsDeltras, so the first call returns no
// useful data as no metrics have yet been collected.
+ (NSData*)getGlobalMetricsDeltas;

// Sets Host Resolver Rules for testing.
// This method must be called after |start| has been called.
+ (void)setHostResolverRulesForTesting:(NSString*)hostResolverRulesForTesting;

// Enables TestCertVerifier which accepts all certificates for testing.
// This method only has any effect before |start| is called.
+ (void)enableTestCertVerifierForTesting;

@end
