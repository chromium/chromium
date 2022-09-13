// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/translate/ios/browser/js_translate_manager.h"

#include "components/grit/components_resources.h"
#import "ios/web/public/test/web_test_with_web_state.h"
#import "testing/gtest_mac.h"
#include "ui/base/resource/resource_bundle.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace translate {
namespace {

typedef web::WebTestWithWebState JsTranslateManagerTest;

// Checks that cr.googleTranslate.libReady is available after the code has
// been injected in the page.
TEST_F(JsTranslateManagerTest, Inject) {
  LoadHtml(@"<html></html>");
  JsTranslateManager* manager =
      [[JsTranslateManager alloc] initWithWebState:web_state()];
  std::string script =
      ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
          IDR_TRANSLATE_JS);
  [manager injectWithTranslateScript:script + "('DummyKey');"];

  id result = ExecuteJavaScript(@"typeof cr.googleTranslate != 'undefined'");
  EXPECT_NSEQ(@YES, result);

  result = ExecuteJavaScript(@"cr.googleTranslate.libReady");
  EXPECT_NSEQ(@NO, result);
}

}  // namespace
}  // namespace translate
