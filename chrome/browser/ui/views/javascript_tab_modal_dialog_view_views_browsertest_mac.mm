// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Accessibility/Accessibility.h>

#include <string>

#include "base/functional/bind.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/javascript_tab_modal_dialog_view_views.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "ui/accessibility/platform/ax_platform_node_mac.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/bubble/bubble_frame_view.h"

using JavaScriptTabModalDialogViewViewsBrowserTestMac = InProcessBrowserTest;

IN_PROC_BROWSER_TEST_F(JavaScriptTabModalDialogViewViewsBrowserTestMac,
                       AlertDialogAccessibleNameDescriptionAndRole) {
  std::u16string title = u"Title";
  std::u16string message = u"The message";
  auto* dialog_views =
      JavaScriptTabModalDialogViewViews::CreateAlertDialogForTesting(
          browser(), title, message);

  // For a JavaScript alert dialog, VoiceOver speaks the accessible name of
  // the native window followed by the accessible name of the RootView. For
  // reasons detailed below, we have to make some adjustments on the Mac to
  // ensure that the window's name contains the title (e.g. "url.com says")
  // and that the RootView's name contains the message text. This test verifies
  // this exposure as well as exposure through other properties that VoiceOver
  // ignores.

  // The RootView of a JavaScript alert dialog should have the accessible role
  // of dialog. On the Mac, that is exposed as the subrole of a group.
  gfx::NativeViewAccessible native_dialog = dialog_views->GetWidget()
                                                ->GetRootView()
                                                ->GetViewAccessibility()
                                                .GetNativeObject();
  EXPECT_EQ(NSAccessibilityGroupRole, [native_dialog accessibilityRole]);
  EXPECT_TRUE([@"AXApplicationDialog"
      isEqualToString:(NSString*)[native_dialog accessibilitySubrole]]);

  // JavaScriptTabModalDialogViewViews sets the accessible description of the
  // RootView to the message contents. That description is set with
  // kDescriptionFrom set to kAriaDescription, which is exposed in
  // accessibilityCustomContent.
  NSString* description = nil;
  ASSERT_TRUE(
      [native_dialog conformsToProtocol:@protocol(AXCustomContentProvider)]);
  auto element_with_content =
      static_cast<id<AXCustomContentProvider>>(native_dialog);
  for (AXCustomContent* content in element_with_content
           .accessibilityCustomContent) {
    if ([content.label isEqualToString:@"description"]) {
      // There should be only one AXCustomContent with the label
      // "description".
      EXPECT_EQ(description, nil);
      description = content.value;
    }
  }
  EXPECT_EQ(message, base::SysNSStringToUTF16(description));

  // While some screen readers use the accessible description to know what to
  // present to the user, VoiceOver currently does not. Therefore, we set the
  // accessibilityLabel of the native window to contain both the title and
  // the message so that both are presented to the user.
  gfx::NativeViewAccessible native_window = [native_dialog accessibilityParent];
  EXPECT_EQ(NSAccessibilityWindowRole, [native_window accessibilityRole]);
  EXPECT_EQ(title + u", " + message,
            base::SysNSStringToUTF16([native_window accessibilityLabel]));
}
