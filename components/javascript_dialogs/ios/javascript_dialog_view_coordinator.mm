// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/javascript_dialogs/ios/javascript_dialog_view_coordinator.h"

#import "base/notreached.h"
#import "base/strings/sys_string_conversions.h"
#import "components/strings/grit/components_strings.h"
#import "ui/base/l10n/l10n_util.h"

@implementation JavascriptDialogViewCoordinator

- (instancetype)initWithBaseViewController:(UIViewController*)baseViewController
                                dialogView:
                                    (javascript_dialogs::TabModalDialogViewIOS*)
                                        dialogView
                                dialogType:(JavaScriptDialogType)dialogType
                                     title:(NSString*)title
                               messageText:(NSString*)messageText
                         defaultPromptText:(NSString*)defaultPromptText {
  UIAlertController* alertDialog =
      [UIAlertController alertControllerWithTitle:title
                                          message:messageText
                                   preferredStyle:UIAlertControllerStyleAlert];

  UIAlertAction* okAction = [UIAlertAction
      actionWithTitle:l10n_util::GetNSString(IDS_OK)
                style:UIAlertActionStyleDefault
              handler:^(UIAlertAction* _Nonnull action) {
                UITextField* promptTextField =
                    alertDialog.textFields.firstObject;
                std::u16string promptText =
                    promptTextField.text
                        ? base::SysNSStringToUTF16(promptTextField.text)
                        : std::u16string();
                dialogView->Accept(promptText);
              }];

  UIAlertAction* cancelAction =
      [UIAlertAction actionWithTitle:l10n_util::GetNSString(IDS_CANCEL)
                               style:UIAlertActionStyleCancel
                             handler:^(UIAlertAction* _Nonnull action) {
                               dialogView->Cancel();
                             }];

  switch (dialogType) {
    case content::JAVASCRIPT_DIALOG_TYPE_ALERT: {
      [alertDialog addAction:okAction];
      break;
    }
    case content::JAVASCRIPT_DIALOG_TYPE_CONFIRM: {
      [alertDialog addAction:cancelAction];
      [alertDialog addAction:okAction];
      break;
    }
    case content::JAVASCRIPT_DIALOG_TYPE_PROMPT: {
      [alertDialog addTextFieldWithConfigurationHandler:^(
                       UITextField* _Nonnull textField) {
        textField.placeholder = defaultPromptText;
      }];

      [alertDialog addAction:cancelAction];
      [alertDialog addAction:okAction];
      break;
    }
    default:
      NOTREACHED_IN_MIGRATION();
  }

  [baseViewController presentViewController:alertDialog
                                   animated:YES
                                 completion:nil];

  return self;
}

@end
