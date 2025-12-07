// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_JAVASCRIPT_DIALOGS_IOS_JAVASCRIPT_DIALOG_VIEW_COORDINATOR_H_
#define COMPONENTS_JAVASCRIPT_DIALOGS_IOS_JAVASCRIPT_DIALOG_VIEW_COORDINATOR_H_

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#import "components/javascript_dialogs/ios/tab_modal_dialog_view_ios.h"
#import "content/public/common/javascript_dialog_type.h"

namespace javascript_dialogs {
class TabModalDialogViewIOS;
}

using content::JavaScriptDialogType;
using javascript_dialogs::TabModalDialogViewIOS;

@class UIViewController;

@interface JavascriptDialogViewCoordinator : NSObject

@property(nonatomic, readonly) UIAlertController* alertController;

// Initializer.
- (instancetype)initWithBaseViewController:(UIViewController*)baseViewController
                                dialogView:
                                    (javascript_dialogs::TabModalDialogViewIOS*)
                                        dialogView
                                dialogType:(JavaScriptDialogType)dialogType
                                     title:(NSString*)title
                               messageText:(NSString*)messageText
                         defaultPromptText:(NSString*)defaultPromptText;

- (std::u16string)promptText;

@end

#endif  // COMPONENTS_JAVASCRIPT_DIALOGS_IOS_JAVASCRIPT_DIALOG_VIEW_COORDINATOR_H_
