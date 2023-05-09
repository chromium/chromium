// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// These tests are somewhat dependent on the exact contents of the
// accept languages table generated at build-time.

#import <Cronet/Cronet.h>
#import <Foundation/Foundation.h>

#include "testing/gtest/include/gtest/gtest.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface Cronet (ExposedForTesting)
+ (NSString*)getAcceptLanguagesFromPreferredLanguages:
    (NSArray<NSString*>*)languages;
@end

namespace cronet {

#define EXPECT_NSEQ(a, b) EXPECT_TRUE([(a) isEqual:(b)])

TEST(AcceptLangTest, Region) {
  NSString* acceptLangs =
      [Cronet getAcceptLanguagesFromPreferredLanguages:@[ @"en-GB" ]];

  EXPECT_NSEQ(acceptLangs, @"en-GB,en-US,en");
}

TEST(AcceptLangTest, Lang) {
  NSString* acceptLangs =
      [Cronet getAcceptLanguagesFromPreferredLanguages:@[ @"ja-JP" ]];

  EXPECT_NSEQ(acceptLangs, @"ja,en-US,en");
}

TEST(AcceptLangTest, Default) {
  NSString* acceptLangs =
      [Cronet getAcceptLanguagesFromPreferredLanguages:@[ @"lol-LOL" ]];

  EXPECT_NSEQ(acceptLangs, @"en-US,en");
}

TEST(AcceptLangTest, Append) {
  NSString* acceptLangs =
      [Cronet getAcceptLanguagesFromPreferredLanguages:@[ @"ja-JP", @"en-GB" ]];

  EXPECT_NSEQ(acceptLangs, @"ja,en-US,en,en-GB");
}

TEST(AcceptLangTest, NoDefaultAppend) {
  NSString* acceptLangs = [Cronet
      getAcceptLanguagesFromPreferredLanguages:@[ @"en-GB", @"lol-LOL" ]];

  NSLog(@"%@", acceptLangs);
  EXPECT_NSEQ(acceptLangs, @"en-GB,en-US,en");
}
}
