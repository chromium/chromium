// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRANSLATE_IOS_BROWSER_JS_TRANSLATE_WEB_FRAME_MANAGER_FACTORY_H_
#define COMPONENTS_TRANSLATE_IOS_BROWSER_JS_TRANSLATE_WEB_FRAME_MANAGER_FACTORY_H_

class JSTranslateWebFrameManager;

namespace web {
class WebFrame;
}  // namespace web

class JSTranslateWebFrameManagerFactory {
 public:
  // static
  static JSTranslateWebFrameManagerFactory* GetInstance();

  virtual JSTranslateWebFrameManager* FromWebFrame(web::WebFrame* web_frame);
  virtual void CreateForWebFrame(web::WebFrame* web_frame);

 protected:
  ~JSTranslateWebFrameManagerFactory();
};

#endif  // COMPONENTS_TRANSLATE_IOS_BROWSER_JS_TRANSLATE_WEB_FRAME_MANAGER_FACTORY_H_
