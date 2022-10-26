// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/language/ios/browser/ios_language_detection_tab_helper_observer_bridge.h"

#include <memory>
#include <string>

#include "base/strings/utf_string_conversions.h"
#import "base/test/task_environment.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/translate/core/browser/translate_pref_names.h"
#include "components/translate/core/common/language_detection_details.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
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
    pref_service_.registry()->RegisterBooleanPref(
        translate::prefs::kOfferTranslateEnabled, true);

    language::IOSLanguageDetectionTabHelper::CreateForWebState(
        &web_state_, /*url_language_histogram=*/nullptr,
        /*language_detection_model=*/nullptr, &pref_service_);
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
  base::test::SingleThreadTaskEnvironment task_environment_;
  TestingPrefServiceSimple pref_service_;
  web::FakeWebState web_state_;
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
  const std::u16string kContents = u"Bonjour";

  translate::LanguageDetectionDetails details;
  details.content_language = kContentLanguage;
  details.model_detected_language = kUndefined;
  details.is_model_reliable = true;
  details.has_notranslate = true;
  details.html_root_language = kRootLanguage;
  details.adopted_language = kAdoptedLanguage;
  details.contents = kContents;
  tab_helper()->OnLanguageDetermined(details);

  EXPECT_TRUE(observer().isDidDetermineLanguageCalled);
  const translate::LanguageDetectionDetails& forwarded_details =
      observer().languageDetectionDetails;
  EXPECT_EQ(kContentLanguage, forwarded_details.content_language);
  EXPECT_EQ(kUndefined, forwarded_details.model_detected_language);
  EXPECT_TRUE(forwarded_details.is_model_reliable);
  EXPECT_TRUE(forwarded_details.has_notranslate);
  EXPECT_EQ(kRootLanguage, forwarded_details.html_root_language);
  EXPECT_EQ(kAdoptedLanguage, forwarded_details.adopted_language);
  EXPECT_EQ(kContents, forwarded_details.contents);
}
