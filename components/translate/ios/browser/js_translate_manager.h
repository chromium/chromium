// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRANSLATE_IOS_BROWSER_JS_TRANSLATE_MANAGER_H_
#define COMPONENTS_TRANSLATE_IOS_BROWSER_JS_TRANSLATE_MANAGER_H_

#import "ios/web/public/deprecated/crw_js_injection_manager.h"

#include <string>

#include "base/time/time.h"

@class NSString;

// Manager for the injection of the Translate JavaScript.
// Replicates functionality from TranslateHelper in
// chrome/renderer/translate/translate_helper.cc.
// JsTranslateManager injects the script in the page and calls it, but is not
// responsible for loading it or caching it.
@interface JsTranslateManager : CRWJSInjectionManager

// The translation script. Must be set before |-inject| is called, and is reset
// after the injection.
@property(nonatomic, copy) NSString* script;

// Starts translation of the page from |source| language to |target| language.
// Equivalent to TranslateHelper::StartTranslation().
- (void)startTranslationFrom:(const std::string&)source
                          to:(const std::string&)target;

// Reverts the translation. Assumes that no navigation happened since the page
// has been translated.
- (void)revertTranslation;

@end

#endif  // COMPONENTS_TRANSLATE_IOS_BROWSER_JS_TRANSLATE_MANAGER_H_
