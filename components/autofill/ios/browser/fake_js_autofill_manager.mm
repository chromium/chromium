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
@synthesize lastClearedFormUniqueID = _lastClearedFormUniqueID;
@synthesize lastClearedFieldIdentifier = _lastClearedFieldIdentifier;
@synthesize lastClearedFieldUniqueID = _lastClearedFieldUniqueID;
@synthesize lastClearedFrameIdentifier = _lastClearedFrameIdentifier;

- (void)
    clearAutofilledFieldsForFormName:(NSString*)formName
                        formUniqueID:(autofill::FormRendererId)formUniqueID
                     fieldIdentifier:(NSString*)fieldIdentifier
                       fieldUniqueID:(autofill::FieldRendererId)fieldUniqueID
                             inFrame:(web::WebFrame*)frame
                   completionHandler:(void (^)(NSString*))completionHandler {
  base::PostTask(FROM_HERE, {web::WebThread::UI}, base::BindOnce(^{
                   _lastClearedFormName = [formName copy];
                   _lastClearedFormUniqueID = formUniqueID;
                   _lastClearedFieldIdentifier = [fieldIdentifier copy];
                   _lastClearedFieldUniqueID = fieldUniqueID;
                   _lastClearedFrameIdentifier =
                       frame ? base::SysUTF8ToNSString(frame->GetFrameId())
                             : nil;
                   completionHandler(@"");
                 }));
}

@end
