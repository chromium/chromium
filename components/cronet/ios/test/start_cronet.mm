// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cronet/Cronet.h>

#include "components/cronet/ios/test/start_cronet.h"

#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace cronet {

void StartCronet(int port) {
  [Cronet setUserAgent:@"CronetTest/1.0.0.0" partial:NO];
  [Cronet setHttp2Enabled:true];
  [Cronet setQuicEnabled:true];
  [Cronet setAcceptLanguages:@"en-US,en"];
  [Cronet addQuicHint:@"test.example.com" port:443 altPort:443];
  [Cronet enableTestCertVerifierForTesting];
  [Cronet setHttpCacheType:CRNHttpCacheTypeDisabled];

  [Cronet start];

  NSString* rules = base::SysUTF8ToNSString(
      base::StringPrintf("MAP test.example.com 127.0.0.1:%d,"
                         "MAP notfound.example.com ~NOTFOUND",
                         port));
  [Cronet setHostResolverRulesForTesting:rules];
}

}  // namespace cronet
