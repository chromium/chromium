// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/autofill/ios/browser/fake_js_autofill_manager.h"

#include "base/bind.h"
#include "base/strings/sys_string_conversions.h"
#include "base/task/post_task.h"
#include "ios/web/public/js_messaging/web_frame.h"
#include "ios/web/public/thread/web_task_traits.h"
#include "ios/web/public/thread/web_thread.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation FakeJSAutofillManager

@synthesize lastClearedFormName = _lastClearedFormName;
@synthesize lastClearedFieldIdentifier = _lastClearedFieldIdentifier;
@synthesize lastClearedFrameIdentifier = _lastClearedFrameIdentifier;

- (void)clearAutofilledFieldsForFormName:(NSString*)formName
                         fieldIdentifier:(NSString*)fieldIdentifier
                                 inFrame:(web::WebFrame*)frame
                       completionHandler:(ProceduralBlock)completionHandler {
  base::PostTask(FROM_HERE, {web::WebThread::UI}, base::BindOnce(^{
                   _lastClearedFormName = [formName copy];
                   _lastClearedFieldIdentifier = [fieldIdentifier copy];
                   _lastClearedFrameIdentifier =
                       frame ? base::SysUTF8ToNSString(frame->GetFrameId())
                             : nil;
                   completionHandler();
                 }));
}

@end
