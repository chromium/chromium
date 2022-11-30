// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/translate/ios/browser/js_translate_web_frame_manager.h"

#import "base/no_destructor.h"
#import "components/translate/ios/browser/js_translate_web_frame_manager_factory.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// static
JSTranslateWebFrameManagerFactory*
JSTranslateWebFrameManagerFactory::GetInstance() {
  static base::NoDestructor<JSTranslateWebFrameManagerFactory> instance;
  return instance.get();
}

JSTranslateWebFrameManager* JSTranslateWebFrameManagerFactory::FromWebFrame(
    web::WebFrame* web_frame) {
  return JSTranslateWebFrameManager::FromWebFrame(web_frame);
}

void JSTranslateWebFrameManagerFactory::CreateForWebFrame(
    web::WebFrame* web_frame) {
  JSTranslateWebFrameManager::CreateForWebFrame(web_frame);
}

JSTranslateWebFrameManagerFactory::~JSTranslateWebFrameManagerFactory() {}
