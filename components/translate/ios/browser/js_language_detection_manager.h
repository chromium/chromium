// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRANSLATE_IOS_BROWSER_JS_LANGUAGE_DETECTION_MANAGER_H_
#define COMPONENTS_TRANSLATE_IOS_BROWSER_JS_LANGUAGE_DETECTION_MANAGER_H_

#import <Foundation/Foundation.h>
#include <stddef.h>

#include "base/callback_forward.h"
#include "base/strings/string16.h"
#import "ios/web/public/deprecated/crw_js_injection_manager.h"

namespace language_detection {

// Maximum length of the extracted text returned by |-extractTextContent|.
// Matches desktop implementation.
extern const size_t kMaxIndexChars;

// Type for the callback called when the buffered text is retrieved.
using BufferedTextCallback = base::Callback<void(const base::string16&)>;

}  // namespace language_detection

// JsLanguageDetectionManager manages the scripts related to language detection.
@interface JsLanguageDetectionManager : CRWJSInjectionManager

// Retrieves the cached text content of the page from the JS side. Calls
// |callback| with the page's text contents. The cache is purged on the JS side
// after this call. |callback| must be non null.
- (void)retrieveBufferedTextContent:
        (const language_detection::BufferedTextCallback&)callback;

// Starts detecting the language of the page.
- (void)startLanguageDetection;

@end

#endif  // COMPONENTS_TRANSLATE_IOS_BROWSER_JS_LANGUAGE_DETECTION_MANAGER_H_
