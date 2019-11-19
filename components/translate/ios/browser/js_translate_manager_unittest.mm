// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/translate/ios/browser/js_translate_manager.h"

#include "base/strings/sys_string_conversions.h"
#include "components/grit/components_resources.h"
#import "ios/web/public/deprecated/crw_test_js_injection_receiver.h"
#import "ios/web/public/test/js_test_util.h"
#import "testing/gtest_mac.h"
#include "testing/platform_test.h"
#include "ui/base/resource/resource_bundle.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

class JsTranslateManagerTest : public PlatformTest {
 protected:
  JsTranslateManagerTest() {
    receiver_ = [[CRWTestJSInjectionReceiver alloc] init];
    manager_ = [[JsTranslateManager alloc] initWithReceiver:receiver_];
    std::string script =
        ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
            IDR_TRANSLATE_JS);
    [manager_ setScript:base::SysUTF8ToNSString(script + "('DummyKey');")];
  }

  bool IsDefined(NSString* name) {
    NSString* script =
        [NSString stringWithFormat:@"typeof %@ != 'undefined'", name];
    return [web::test::ExecuteJavaScript(receiver_, script) boolValue];
  }

  CRWTestJSInjectionReceiver* receiver_;
  JsTranslateManager* manager_;
};

// Checks that cr.googleTranslate.libReady is available after the code has
// been injected in the page.
TEST_F(JsTranslateManagerTest, Inject) {
  [manager_ inject];
  EXPECT_TRUE([manager_ hasBeenInjected]);
  EXPECT_EQ(nil, [manager_ script]);
  EXPECT_TRUE(IsDefined(@"cr.googleTranslate"));
  EXPECT_NSEQ(@NO, web::test::ExecuteJavaScript(
                       manager_, @"cr.googleTranslate.libReady"));
}
