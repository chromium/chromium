// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/translate/ios/browser/js_translate_web_frame_manager.h"

#import <Foundation/Foundation.h>

#include "base/check.h"
#import "base/logging.h"
#include "base/mac/bundle_locations.h"
#include "base/strings/stringprintf.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "ios/web/public/js_messaging/web_frame.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Returns an autoreleased string containing the JavaScript loaded from a
// bundled resource file with the given name (excluding extension).
NSString* GetPageScript(NSString* script_file_name) {
  DCHECK(script_file_name);
  NSString* path =
      [base::mac::FrameworkBundle() pathForResource:script_file_name
                                             ofType:@"js"];
  DCHECK(path) << "Script file not found: "
               << base::SysNSStringToUTF8(script_file_name) << ".js";
  NSError* error = nil;
  NSString* content = [NSString stringWithContentsOfFile:path
                                                encoding:NSUTF8StringEncoding
                                                   error:&error];
  DCHECK(!error) << "Error fetching script: "
                 << base::SysNSStringToUTF8(error.description);
  DCHECK(content);
  return content;
}

}  // namespace

JSTranslateWebFrameManager::JSTranslateWebFrameManager(web::WebFrame* web_frame)
    : web_frame_(web_frame) {
  DCHECK(web_frame);
}

JSTranslateWebFrameManager::~JSTranslateWebFrameManager() {}

void JSTranslateWebFrameManager::InjectTranslateScript(
    const std::string& translate_script) {
  // Prepend translate_ios.js
  NSString* translate_ios = GetPageScript(@"translate_ios");
  NSString* script = [translate_ios
      stringByAppendingString:base::SysUTF8ToNSString(translate_script)];

  // Reset translate state if previously injected.
  if (injected_) {
    NSString* resetScript = @"try {"
                             "  cr.googleTranslate.revert();"
                             "} catch (e) {"
                             "}";
    script = [resetScript stringByAppendingString:script];
  }

  injected_ = true;
  web_frame_->ExecuteJavaScript(base::SysNSStringToUTF16(script));
}

void JSTranslateWebFrameManager::StartTranslation(const std::string& source,
                                                  const std::string& target) {
  std::string script =
      base::StringPrintf("cr.googleTranslate.translate('%s','%s')",
                         source.c_str(), target.c_str());
  web_frame_->ExecuteJavaScript(base::UTF8ToUTF16(script));
}

void JSTranslateWebFrameManager::RevertTranslation() {
  if (!injected_)
    return;

  web_frame_->ExecuteJavaScript(u"cr.googleTranslate.revert()");
}

void JSTranslateWebFrameManager::HandleTranslateResponse(
    const std::string& url,
    int request_id,
    int response_code,
    const std::string status_text,
    const std::string& response_url,
    const std::string& response_text) {
  DCHECK(injected_);

  // Return the response details to function defined in translate_ios.js.
  std::string script = base::StringPrintf(
      "__gCrWeb.translate.handleResponse('%s', %d, %d, '%s', '%s', '%s')",
      url.c_str(), request_id, response_code, status_text.c_str(),
      response_url.c_str(), response_text.c_str());
  web_frame_->ExecuteJavaScript(base::UTF8ToUTF16(script));
}
