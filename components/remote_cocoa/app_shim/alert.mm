// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/remote_cocoa/app_shim/alert.h"

#import "base/apple/foundation_util.h"
#include "base/functional/bind.h"
#include "base/i18n/rtl.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/strings/sys_string_conversions.h"
#include "ui/accelerated_widget_mac/window_resize_helper_mac.h"
#include "ui/base/l10n/l10n_util_mac.h"

using remote_cocoa::mojom::AlertBridgeInitParams;
using remote_cocoa::mojom::AlertDisposition;

namespace {

const int kSlotsPerLine = 50;
const int kMessageTextMaxSlots = 2000;

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// AlertBridgeHelper:

// Helper object that receives the notification that the dialog/sheet is
// going away. Is responsible for cleaning itself up.
@interface AlertBridgeHelper : NSObject <NSAlertDelegate> {
 @private
  NSAlert* __strong _alert;
  // This field is not a raw_ptr<> because it requires @property rewrite.
  RAW_PTR_EXCLUSION remote_cocoa::AlertBridge* _alertBridge;  // Weak.
  NSTextField* __strong _textField;
}
@property(assign, nonatomic) remote_cocoa::AlertBridge* alertBridge;

// Returns the underlying alert.
- (NSAlert*)alert;

// Set a blank icon for dialogs with text provided by the page.
- (void)setBlankIcon;

// Add a text field to the alert.
- (void)addTextFieldWithPrompt:(NSString*)prompt;

// Presents an AppKit blocking dialog.
- (void)showAlert;
@end

@implementation AlertBridgeHelper
@synthesize alertBridge = _alertBridge;

- (void)initAlert:(AlertBridgeInitParams*)params {
  _alert = [[NSAlert alloc] init];
  _alert.delegate = self;

  if (params->hide_application_icon)
    [self setBlankIcon];
  if (params->text_field_text) {
    [self addTextFieldWithPrompt:base::SysUTF16ToNSString(
                                     *params->text_field_text)];
  }
  NSString* informative_text = base::SysUTF16ToNSString(params->message_text);

  // Truncate long JS alerts - crbug.com/331219
  NSCharacterSet* newline_char_set = [NSCharacterSet newlineCharacterSet];
  for (size_t index = 0, slots_count = 0; index < informative_text.length;
       ++index) {
    unichar current_char = [informative_text characterAtIndex:index];
    if ([newline_char_set characterIsMember:current_char])
      slots_count += kSlotsPerLine;
    else
      slots_count++;
    if (slots_count > kMessageTextMaxSlots) {
      std::u16string info_text = base::SysNSStringToUTF16(informative_text);
      informative_text = base::SysUTF16ToNSString(
          gfx::TruncateString(info_text, index, gfx::WORD_BREAK));
      break;
    }
  }

  _alert.informativeText = informative_text;
  NSString* message_text = l10n_util::FixUpWindowsStyleLabel(params->title);
  _alert.messageText = message_text;
  [_alert addButtonWithTitle:l10n_util::FixUpWindowsStyleLabel(
                                 params->primary_button_text)];

  if (params->secondary_button_text) {
    NSButton* other =
        [_alert addButtonWithTitle:l10n_util::FixUpWindowsStyleLabel(
                                       *params->secondary_button_text)];
    other.keyEquivalent = @"\e";
  }
  if (params->check_box_text) {
    _alert.showsSuppressionButton = YES;
    NSString* suppression_title =
        l10n_util::FixUpWindowsStyleLabel(*params->check_box_text);
    [_alert.suppressionButton setTitle:suppression_title];
  }

  // Fix RTL dialogs.
  //
  // macOS will always display NSAlert strings as LTR. A workaround is to
  // manually set the text as attributed strings in the implementing
  // NSTextFields. This is a basic correctness issue.
  //
  // In addition, for readability, the overall alignment is set based on the
  // directionality of the first strongly-directional character.
  //
  // If the dialog fields are selectable then they will scramble when clicked.
  // Therefore, selectability is disabled.
  //
  // See http://crbug.com/70806 for more details.

  bool message_has_rtl =
      base::i18n::StringContainsStrongRTLChars(params->title);
  bool informative_has_rtl =
      base::i18n::StringContainsStrongRTLChars(params->message_text);

  NSTextField* message_text_field = nil;
  NSTextField* informative_text_field = nil;
  if (message_has_rtl || informative_has_rtl) {
    // Force layout of the dialog. NSAlert leaves its dialog alone once laid
    // out; if this is not done then all the modifications that are to come will
    // be un-done when the dialog is finally displayed.
    [_alert layout];

    // Locate the NSTextFields that implement the text display. These are
    // actually available as the ivars |_messageField| and |_informationField|
    // of the NSAlert, but it is safer (and more forward-compatible) to search
    // for them in the subviews.
    for (NSView* view in _alert.window.contentView.subviews) {
      NSTextField* text_field = base::apple::ObjCCast<NSTextField>(view);
      if ([text_field.stringValue isEqualTo:message_text]) {
        message_text_field = text_field;
      } else if ([text_field.stringValue isEqualTo:informative_text]) {
        informative_text_field = text_field;
      }
    }

    // This may fail in future OS releases, but it will still work for shipped
    // versions of Chromium.
    DCHECK(message_text_field);
    DCHECK(informative_text_field);
  }

  if (message_has_rtl && message_text_field) {
    NSMutableParagraphStyle* alignment =
        [NSParagraphStyle.defaultParagraphStyle mutableCopy];
    alignment.alignment = NSTextAlignmentRight;

    NSDictionary* alignment_attributes =
        @{NSParagraphStyleAttributeName : alignment};
    NSAttributedString* attr_string =
        [[NSAttributedString alloc] initWithString:message_text
                                        attributes:alignment_attributes];

    message_text_field.attributedStringValue = attr_string;
    message_text_field.selectable = NO;
  }

  if (informative_has_rtl && informative_text_field) {
    base::i18n::TextDirection direction =
        base::i18n::GetFirstStrongCharacterDirection(params->message_text);
    NSMutableParagraphStyle* alignment =
        [NSParagraphStyle.defaultParagraphStyle mutableCopy];
    alignment.alignment = direction == base::i18n::RIGHT_TO_LEFT
                              ? NSTextAlignmentRight
                              : NSTextAlignmentLeft;

    NSDictionary* alignment_attributes =
        @{NSParagraphStyleAttributeName : alignment};
    NSAttributedString* attr_string =
        [[NSAttributedString alloc] initWithString:informative_text
                                        attributes:alignment_attributes];

    informative_text_field.attributedStringValue = attr_string;
    informative_text_field.selectable = NO;
  }
}

- (void)setBlankIcon {
  NSImage* image = [[NSImage alloc] initWithSize:NSMakeSize(1, 1)];
  _alert.icon = image;
}

- (NSAlert*)alert {
  return _alert;
}

- (void)addTextFieldWithPrompt:(NSString*)prompt {
  DCHECK(!_textField);
  _textField = [[NSTextField alloc] initWithFrame:NSMakeRect(0, 0, 300, 22)];
  _textField.cell.lineBreakMode = NSLineBreakByTruncatingTail;
  self.alert.accessoryView = _textField;
  _alert.window.initialFirstResponder = _textField;

  [_textField setStringValue:prompt];
}

// |contextInfo| is the JavaScriptAppModalDialogCocoa that owns us.
- (void)alertDidEnd:(NSAlert*)alert
         returnCode:(int)returnCode
        contextInfo:(void*)contextInfo {
  switch (returnCode) {
    case NSAlertFirstButtonReturn:  // OK
      _alertBridge->SendResultAndDestroy(AlertDisposition::PRIMARY_BUTTON);
      break;
    case NSAlertSecondButtonReturn:  // Cancel
      _alertBridge->SendResultAndDestroy(AlertDisposition::SECONDARY_BUTTON);
      break;
    case NSModalResponseStop:  // Window was closed underneath us
      _alertBridge->SendResultAndDestroy(AlertDisposition::CLOSE);
      break;
    default:
      NOTREACHED_IN_MIGRATION();
  }
}

- (void)showAlert {
  DCHECK(_alertBridge);
  _alertBridge->SetAlertHasShown();
  NSAlert* alert = [self alert];
  [alert layout];
  [alert.window recalculateKeyViewLoop];
  // TODO(crbug.com/40575730): Migrate to `[NSWindow
  // beginSheetModalForWindow:completionHandler:]` instead.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
  [alert beginSheetModalForWindow:nil  // nil here makes it app-modal
                    modalDelegate:self
                   didEndSelector:@selector(alertDidEnd:returnCode:contextInfo:)
                      contextInfo:nullptr];
#pragma clang diagnostic pop
}

- (void)closeWindow {
  DCHECK(_alertBridge);
  [NSApp endSheet:self.alert.window];
}

- (std::u16string)input {
  if (_textField)
    return base::SysNSStringToUTF16(_textField.stringValue);
  return std::u16string();
}

- (bool)shouldSuppress {
  if ([[self alert] showsSuppressionButton])
    return [[[self alert] suppressionButton] state] == NSControlStateValueOn;
  return false;
}

@end

namespace remote_cocoa {

////////////////////////////////////////////////////////////////////////////////
// AlertBridge:

AlertBridge::AlertBridge(
    mojo::PendingReceiver<mojom::AlertBridge> bridge_receiver)
    : weak_factory_(this) {
  if (bridge_receiver.is_valid()) {
    mojo_receiver_.Bind(std::move(bridge_receiver),
                        ui::WindowResizeHelperMac::Get()->task_runner());
    mojo_receiver_.set_disconnect_handler(base::BindOnce(
        &AlertBridge::OnMojoDisconnect, weak_factory_.GetWeakPtr()));
  }
}

AlertBridge::~AlertBridge() {
  helper_.alertBridge = nil;
  [NSObject cancelPreviousPerformRequestsWithTarget:helper_];
}

void AlertBridge::OnMojoDisconnect() {
  // If the alert has been shown, then close the window, and |this| will delete
  // itself after the window is closed. Otherwise, just delete |this|
  // immediately.
  if (alert_shown_)
    [helper_ closeWindow];
  else
    delete this;
}

void AlertBridge::SendResultAndDestroy(AlertDisposition disposition) {
  if (!alert_dismissed_) {
    DCHECK(callback_);
    std::move(callback_).Run(disposition, [helper_ input],
                             [helper_ shouldSuppress]);
  }
  delete this;
}

void AlertBridge::SetAlertHasShown() {
  DCHECK(!alert_shown_);
  alert_shown_ = true;
}

////////////////////////////////////////////////////////////////////////////////
// AlertBridge, mojo::AlertBridge:

void AlertBridge::Show(mojom::AlertBridgeInitParamsPtr params,
                       ShowCallback callback) {
  callback_ = std::move(callback);

  // Create a helper which will receive the sheet ended selector.
  helper_ = [[AlertBridgeHelper alloc] init];
  helper_.alertBridge = this;
  [helper_ initAlert:params.get()];

  // Dispatch the method to show the alert back to the top of the CFRunLoop.
  // This fixes an interaction bug with NSSavePanel. http://crbug.com/375785
  // When this object is destroyed, outstanding performSelector: requests
  // should be cancelled.
  [helper_ performSelector:@selector(showAlert) withObject:nil afterDelay:0];
}

void AlertBridge::Dismiss() {
  alert_dismissed_ = true;
  OnMojoDisconnect();
}

}  // namespace remote_cocoa
