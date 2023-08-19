// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/translate/ios/browser/js_translate_web_frame_manager.h"

#import <Foundation/Foundation.h>

#include "base/apple/bundle_locations.h"
#include "base/check.h"
#import "base/logging.h"
#include "base/strings/stringprintf.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "ios/web/public/js_messaging/web_frame.h"

namespace {

// Returns an autoreleased string containing the JavaScript loaded from a
// bundled resource file with the given name (excluding extension).
NSString* GetPageScript(NSString* script_file_name) {
  DCHECK(script_file_name);
  NSString* path =
      [base::apple::FrameworkBundle() pathForResource:script_file_name
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

constexpr char16_t kResetScript[] = u"try {"
                                    "  cr.googleTranslate.revert();"
                                    "} catch (e) {"
                                    "}";

}  // namespace

JSTranslateWebFrameManager::JSTranslateWebFrameManager(web::WebFrame* web_frame)
    : web_frame_(web_frame) {
  DCHECK(web_frame);
}

JSTranslateWebFrameManager::~JSTranslateWebFrameManager() {}

void JSTranslateWebFrameManager::InjectTranslateScript(
    const std::string& translate_script) {
  // Always prepend reset script since this page could have been loaded from the
  // WebKit page cache.
  NSString* script = [NSString
      stringWithFormat:@"%@%@%@", base::SysUTF16ToNSString(kResetScript),
                       GetPageScript(@"translate_ios"),
                       base::SysUTF8ToNSString(translate_script)];
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
  web_frame_->ExecuteJavaScript(kResetScript);
}

void JSTranslateWebFrameManager::HandleTranslateResponse(
    const std::string& url,
    int request_id,
    int response_code,
    const std::string status_text,
    const std::string& response_url,
    const std::string& response_text) {
  // Return the response details to function defined in translate_ios.js.
  std::string script = base::StringPrintf(
      "__gCrWeb.translate.handleResponse(%d, %d, '%s', '%s', '%s')", request_id,
      response_code, status_text.c_str(), response_url.c_str(),
      response_text.c_str());
  web_frame_->ExecuteJavaScript(base::UTF8ToUTF16(script));
}
