// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/permissions/ios/content/permission_prompt/permission_dialog_delegate.h"

#import "base/strings/sys_string_conversions.h"
#import "components/permissions/ios/content/permission_prompt/permission_prompt_ios.h"
#import "components/permissions/permission_util.h"
#import "content/public/browser/web_contents.h"
#import "ui/gfx/native_ui_types.h"

@interface PermissionDialogDelegate ()

@property(nonatomic, assign) permissions::PermissionPromptIOS* prompt;
@property(nonatomic, assign) content::WebContents* webContents;
@property(nonatomic, strong) UIAlertController* alertController;

@end

@implementation PermissionDialogDelegate

- (instancetype)initWithPrompt:(permissions::PermissionPromptIOS*)prompt
                   webContents:(content::WebContents*)webContents {
  self = [super init];
  if (self) {
    _prompt = prompt;
    _webContents = webContents;
    [self showDialog];
  }
  return self;
}

#pragma mark - Private Methods

- (void)showDialog {
  if (!_prompt) {
    return;
  }

  NSString* message =
      base::SysUTF16ToNSString(_prompt->GetAnnotatedMessageText().text);
  _alertController =
      [UIAlertController alertControllerWithTitle:nil
                                          message:message
                                   preferredStyle:UIAlertControllerStyleAlert];

  bool isOneTime = permissions::PermissionUtil::DoesSupportTemporaryGrants(
      _prompt->GetContentSettingType(0));

  __weak __typeof(self) weakSelf = self;

  // Add "Allow" button
  UIAlertAction* allowAction =
      [UIAlertAction actionWithTitle:_prompt->GetPositiveButtonText(isOneTime)
                               style:UIAlertActionStyleDefault
                             handler:^(UIAlertAction* action) {
                               [weakSelf handleAllowAction];
                             }];
  [_alertController addAction:allowAction];

  // Add "Allow This Time" button for supported permissions
  NSString* positiveEphemeralButtonText =
      _prompt->GetPositiveEphemeralButtonText(isOneTime);
  UIAlertAction* thisTimeAction =
      [UIAlertAction actionWithTitle:positiveEphemeralButtonText
                               style:UIAlertActionStyleDefault
                             handler:^(UIAlertAction* action) {
                               [weakSelf handleAllowThisTimeAction];
                             }];
  if (positiveEphemeralButtonText.length > 0) {
    [_alertController addAction:thisTimeAction];
  }

  // Add "Block" button
  UIAlertAction* blockAction =
      [UIAlertAction actionWithTitle:_prompt->GetNegativeButtonText(isOneTime)
                               style:UIAlertActionStyleCancel
                             handler:^(UIAlertAction* action) {
                               [weakSelf handleBlockAction];
                             }];
  [_alertController addAction:blockAction];

  // Present the dialog
  gfx::NativeWindow nativeWindow = _webContents->GetTopLevelNativeWindow();
  UIViewController* rootViewController = nativeWindow.Get().rootViewController;
  [rootViewController presentViewController:_alertController
                                   animated:YES
                                 completion:nil];
}

- (void)handleAllowAction {
  if (_prompt) {
    _prompt->Accept();
  }
  [self cleanup];
}

- (void)handleAllowThisTimeAction {
  if (_prompt) {
    _prompt->AcceptThisTime();
  }
  [self cleanup];
}

- (void)handleBlockAction {
  if (_prompt) {
    _prompt->Deny();
  }
  [self cleanup];
}

- (void)cleanup {
  _alertController = nil;
  _webContents = nil;
  _prompt = nil;
}

@end
