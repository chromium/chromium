// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/language/ios/browser/ios_language_detection_tab_helper_observer_bridge.h"

#include <memory>

#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "components/translate/core/common/language_detection_details.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface TestIOSLanguageDetectionTabHelperObserver
    : NSObject <IOSLanguageDetectionTabHelperObserving>

@property(nonatomic, getter=isDidDetermineLanguageCalled)
    BOOL didDetermineLanguageCalled;

@property(nonatomic)
    translate::LanguageDetectionDetails languageDetectionDetails;

@end

@implementation TestIOSLanguageDetectionTabHelperObserver

#pragma mark - IOSLanguageDetectionTabHelperObserving

- (void)iOSLanguageDetectionTabHelper:
            (language::IOSLanguageDetectionTabHelper*)tabHelper
                 didDetermineLanguage:
                     (const translate::LanguageDetectionDetails&)details {
  self.didDetermineLanguageCalled = YES;
  self.languageDetectionDetails = details;
}

@end

class IOSLanguageDetectionTabHelperObserverBridgeTest : public PlatformTest {
 protected:
  IOSLanguageDetectionTabHelperObserverBridgeTest() {
    language::IOSLanguageDetectionTabHelper::CreateForWebState(
        &web_state_, /*url_language_histogram=*/nullptr);
    tab_helper_ =
        language::IOSLanguageDetectionTabHelper::FromWebState(&web_state_);
    observer_ = [[TestIOSLanguageDetectionTabHelperObserver alloc] init];
    oberserver_bridge_ =
        std::make_unique<language::IOSLanguageDetectionTabHelperObserverBridge>(
            tab_helper_, observer_);
  }

  language::IOSLanguageDetectionTabHelper* tab_helper() { return tab_helper_; }

  TestIOSLanguageDetectionTabHelperObserver* observer() { return observer_; }

 private:
  web::TestWebState web_state_;
  language::IOSLanguageDetectionTabHelper* tab_helper_;
  TestIOSLanguageDetectionTabHelperObserver* observer_;
  std::unique_ptr<language::IOSLanguageDetectionTabHelperObserverBridge>
      oberserver_bridge_;
};

// Tests that |OnLanguageDetermined| call is forwarded by the observer bridge.
TEST_F(IOSLanguageDetectionTabHelperObserverBridgeTest, OnLanguageDetermined) {
  const std::string kRootLanguage = "en";
  const std::string kContentLanguage = "fr";
  const std::string kAdoptedLanguage = "es";
  const std::string kUndefined = "und";
  const base::string16 kContents = base::ASCIIToUTF16("Bonjour");

  translate::LanguageDetectionDetails details;
  details.content_language = kContentLanguage;
  details.cld_language = kUndefined;
  details.is_cld_reliable = true;
  details.has_notranslate = true;
  details.html_root_language = kRootLanguage;
  details.adopted_language = kAdoptedLanguage;
  details.contents = kContents;
  tab_helper()->OnLanguageDetermined(details);

  EXPECT_TRUE(observer().isDidDetermineLanguageCalled);
  const translate::LanguageDetectionDetails& forwarded_details =
      observer().languageDetectionDetails;
  EXPECT_EQ(kContentLanguage, forwarded_details.content_language);
  EXPECT_EQ(kUndefined, forwarded_details.cld_language);
  EXPECT_TRUE(forwarded_details.is_cld_reliable);
  EXPECT_TRUE(forwarded_details.has_notranslate);
  EXPECT_EQ(kRootLanguage, forwarded_details.html_root_language);
  EXPECT_EQ(kAdoptedLanguage, forwarded_details.adopted_language);
  EXPECT_EQ(kContents, forwarded_details.contents);
}
