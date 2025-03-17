// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef COMPONENTS_AUTOFILL_IOS_BROWSER_AUTOFILL_AGENT_H_
#define COMPONENTS_AUTOFILL_IOS_BROWSER_AUTOFILL_AGENT_H_

#import <Foundation/Foundation.h>

#import "components/autofill/ios/browser/autofill_client_ios_bridge.h"
#import "components/autofill/ios/browser/autofill_driver_ios_bridge.h"
#import "components/autofill/ios/browser/form_suggestion_provider.h"

class PrefService;

namespace web {
class WebState;
}

// Delegate for AutofillAgent.
@protocol AutofillAgentDelegate

// Shows a snackbar displaying a message with `messageText` and a button with
// `buttonText` which triggers `messageAction` on tap. `completionAction` will
// be called when the snackbar finishes presenting, BOOL is YES if the dismissal
// was caused by a user action and NO if not. It will use the Bottom toolbar
// height as bottom offset. Use this method if displaying a Snackbar while the
// Web content is visible. If there's no bottom toolbar offset will be 0.
- (void)showSnackbarWithMessage:(NSString*)messageText
                     buttonText:(NSString*)buttonText
                  messageAction:(void (^)(void))messageAction
               completionAction:(void (^)(BOOL))completionAction;

// Whether or not the Keyboard Accessory Upgrade feature is enabled.
@property(nonatomic, readonly) BOOL isKeyboardAccessoryUpgradeEnabled;

@end

// Handles autofill form suggestions. Reads forms from the page, sends them to
// BrowserAutofillManager for metrics and to retrieve suggestions, and fills
// forms in response to user interaction with suggestions. This is the iOS
// counterpart to the upstream class autofill::AutofillAgent.
@interface AutofillAgent : NSObject <AutofillClientIOSBridge,
                                     AutofillDriverIOSBridge,
                                     FormSuggestionProvider>

// Used to show a snackbar.
@property(nonatomic, weak) id<AutofillAgentDelegate> delegate;

// Designated initializer. Arguments |prefService| and |webState| should not be
// null.
- (instancetype)initWithPrefService:(PrefService*)prefService
                           webState:(web::WebState*)webState
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // COMPONENTS_AUTOFILL_IOS_BROWSER_AUTOFILL_AGENT_H_
