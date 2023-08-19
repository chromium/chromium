// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/translate/ios/browser/js_translate_web_frame_manager.h"

#include "components/grit/components_resources.h"
#import "components/translate/ios/browser/js_translate_web_frame_manager_factory.h"
#import "components/translate/ios/browser/translate_java_script_feature.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/test/web_test_with_web_state.h"
#import "testing/gtest_mac.h"
#include "ui/base/resource/resource_bundle.h"

namespace translate {
namespace {

typedef web::WebTestWithWebState JsTranslateWebFrameManagerTest;

// Checks that cr.googleTranslate.libReady is available after the code has
// been injected in the page.
TEST_F(JsTranslateWebFrameManagerTest, Inject) {
  LoadHtml(@"<html></html>");

  id result = ExecuteJavaScript(@"typeof cr == 'undefined'");
  ASSERT_NSEQ(@YES, result);

  TranslateJavaScriptFeature* feature =
      TranslateJavaScriptFeature::GetInstance();
  web::WebFrame* frame =
      feature->GetWebFramesManager(web_state())->GetMainWebFrame();
  JSTranslateWebFrameManager* manager =
      JSTranslateWebFrameManagerFactory::GetInstance()->FromWebFrame(frame);
  ASSERT_FALSE(manager);

  JSTranslateWebFrameManagerFactory::GetInstance()->CreateForWebFrame(frame);
  manager =
      JSTranslateWebFrameManagerFactory::GetInstance()->FromWebFrame(frame);
  ASSERT_TRUE(manager);

  std::string script =
      ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
          IDR_TRANSLATE_JS);
  manager->InjectTranslateScript(script + "('DummyKey');");

  result = ExecuteJavaScript(@"typeof cr.googleTranslate != 'undefined'");
  EXPECT_NSEQ(@YES, result);

  result = ExecuteJavaScript(@"cr.googleTranslate.libReady");
  EXPECT_NSEQ(@NO, result);
}

// Tests that injecting the translate script twice still leaves the translate
// lib ready.
TEST_F(JsTranslateWebFrameManagerTest, Reinject) {
  LoadHtml(@"<html></html>");
  TranslateJavaScriptFeature* feature =
      TranslateJavaScriptFeature::GetInstance();
  web::WebFrame* frame =
      feature->GetWebFramesManager(web_state())->GetMainWebFrame();
  JSTranslateWebFrameManagerFactory::GetInstance()->CreateForWebFrame(frame);
  JSTranslateWebFrameManager* manager =
      JSTranslateWebFrameManagerFactory::GetInstance()->FromWebFrame(frame);
  ASSERT_TRUE(manager);

  std::string script =
      ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
          IDR_TRANSLATE_JS);

  manager->InjectTranslateScript(script + "('DummyKey');");
  manager->InjectTranslateScript(script + "('DummyKey');");

  id result = ExecuteJavaScript(@"cr.googleTranslate.libReady");
  EXPECT_NSEQ(@NO, result);
}

// Tests that starting translation calls the appropriate API.
TEST_F(JsTranslateWebFrameManagerTest, Translate) {
  LoadHtml(@"<html></html>");
  TranslateJavaScriptFeature* feature =
      TranslateJavaScriptFeature::GetInstance();
  web::WebFrame* frame =
      feature->GetWebFramesManager(web_state())->GetMainWebFrame();
  JSTranslateWebFrameManagerFactory::GetInstance()->CreateForWebFrame(frame);
  JSTranslateWebFrameManager* manager =
      JSTranslateWebFrameManagerFactory::GetInstance()->FromWebFrame(frame);
  ASSERT_TRUE(manager);

  std::string script =
      ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
          IDR_TRANSLATE_JS);

  manager->InjectTranslateScript(script + "('DummyKey');");

  ExecuteJavaScript(@"translationApiCalled = false;"
                    @"var originalTranslate = cr.googleTranslate.translate;"
                    @"  cr.googleTranslate.translate = function() {"
                    @"  translationApiCalled = true;"
                    @"  return originalTranslate.apply(this, arguments);"
                    @"};");

  id result = ExecuteJavaScript(@"translationApiCalled");
  EXPECT_NSEQ(@NO, result);

  manager->StartTranslation("en", "es");

  result = ExecuteJavaScript(@"translationApiCalled");
  EXPECT_NSEQ(@YES, result);
}

// Tests that reverting translation calls the appropriate API.
TEST_F(JsTranslateWebFrameManagerTest, Revert) {
  LoadHtml(@"<html></html>");
  TranslateJavaScriptFeature* feature =
      TranslateJavaScriptFeature::GetInstance();
  web::WebFrame* frame =
      feature->GetWebFramesManager(web_state())->GetMainWebFrame();
  JSTranslateWebFrameManagerFactory::GetInstance()->CreateForWebFrame(frame);
  JSTranslateWebFrameManager* manager =
      JSTranslateWebFrameManagerFactory::GetInstance()->FromWebFrame(frame);
  ASSERT_TRUE(manager);

  std::string script =
      ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
          IDR_TRANSLATE_JS);
  manager->InjectTranslateScript(script + "('DummyKey');");

  ExecuteJavaScript(@"revertApiCalled = false;"
                    @"var originalRevert = cr.googleTranslate.revert;"
                    @"  cr.googleTranslate.revert = function() {"
                    @"  revertApiCalled = true;"
                    @"  return originalRevert.apply(this, arguments);"
                    @"};");

  id result = ExecuteJavaScript(@"revertApiCalled");
  EXPECT_NSEQ(@NO, result);

  manager->RevertTranslation();

  result = ExecuteJavaScript(@"revertApiCalled");
  EXPECT_NSEQ(@YES, result);
}

}  // namespace
}  // namespace translate
