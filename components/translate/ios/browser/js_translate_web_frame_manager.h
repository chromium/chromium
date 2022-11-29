// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRANSLATE_IOS_BROWSER_JS_TRANSLATE_WEB_FRAME_MANAGER_H_
#define COMPONENTS_TRANSLATE_IOS_BROWSER_JS_TRANSLATE_WEB_FRAME_MANAGER_H_

#import "ios/web/public/js_messaging/web_frame_user_data.h"

namespace web {
class WebFrame;
}  // namespace web

class JSTranslateWebFrameManager
    : public web::WebFrameUserData<JSTranslateWebFrameManager> {
 public:
  ~JSTranslateWebFrameManager() override;

  virtual void InjectTranslateScript(const std::string& script);
  virtual void StartTranslation(const std::string& source,
                                const std::string& target);
  virtual void RevertTranslation();
  virtual void HandleTranslateResponse(const std::string& url,
                                       int request_id,
                                       int response_code,
                                       const std::string status_text,
                                       const std::string& response_url,
                                       const std::string& response_text);

 protected:
  explicit JSTranslateWebFrameManager(web::WebFrame* web_frame);

  web::WebFrame* web_frame_ = nullptr;

 private:
  friend class web::WebFrameUserData<JSTranslateWebFrameManager>;
  friend class JSTranslateWebFrameManagerFactory;
  friend class TestJSTranslateWebFrameManager;
};

#endif  // COMPONENTS_TRANSLATE_IOS_BROWSER_JS_TRANSLATE_WEB_FRAME_MANAGER_H_
