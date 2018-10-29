// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "chrome/browser/certificate_viewer.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/browser/web_contents.h"
#include "net/cert/x509_certificate.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#import "testing/gtest_mac.h"
#include "ui/base/cocoa/window_size_constants.h"

using web_modal::WebContentsModalDialogManager;

using SSLCertificateViewerMacTest = InProcessBrowserTest;

namespace {

scoped_refptr<net::X509Certificate> GetSampleCertificate() {
  return net::ImportCertFromFile(net::GetTestCertsDirectory(),
                                 "mit.davidben.der");
}

void CheckCertificateViewerVisibility(NSWindow* overlay_window,
                                      NSWindow* dialog_sheet,
                                      bool visible) {
  CGFloat alpha = visible ? 1.0 : 0.0;
  BOOL ignore_events = visible ? NO : YES;

  SCOPED_TRACE(testing::Message() << "visible=" << visible);
  // The overlay window underneath the certificate viewer should block mouse
  // events only if the certificate viewer is visible.
  EXPECT_EQ(ignore_events, [overlay_window ignoresMouseEvents]);
  // Check certificate viewer sheet visibility and if it accepts mouse events.
  EXPECT_EQ(alpha, [dialog_sheet alphaValue]);
  EXPECT_EQ(ignore_events, [dialog_sheet ignoresMouseEvents]);
}

}  // namespace

IN_PROC_BROWSER_TEST_F(SSLCertificateViewerMacTest, Basic) {
  scoped_refptr<net::X509Certificate> cert = GetSampleCertificate();
  ASSERT_TRUE(cert.get());
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  NSWindow* window =
      web_contents->GetTopLevelNativeWindow().GetNativeNSWindow();
  WebContentsModalDialogManager* web_contents_modal_dialog_manager =
      WebContentsModalDialogManager::FromWebContents(web_contents);
  EXPECT_FALSE(web_contents_modal_dialog_manager->IsDialogActive());

  ShowCertificateViewer(web_contents, window, cert.get());

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(web_contents_modal_dialog_manager->IsDialogActive());

  WebContentsModalDialogManager::TestApi test_api(
      web_contents_modal_dialog_manager);
  test_api.CloseAllDialogs();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(web_contents_modal_dialog_manager->IsDialogActive());
}

// Test that switching to another tab correctly hides the sheet.
IN_PROC_BROWSER_TEST_F(SSLCertificateViewerMacTest, HideShow) {
  scoped_refptr<net::X509Certificate> cert = GetSampleCertificate();
  ASSERT_TRUE(cert.get());
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  NSWindow* window =
      web_contents->GetTopLevelNativeWindow().GetNativeNSWindow();
  WebContentsModalDialogManager* web_contents_modal_dialog_manager =
      WebContentsModalDialogManager::FromWebContents(web_contents);

  // Account for any child windows that might be present before the certificate
  // viewer is open.
  NSUInteger num_child_windows = [[window childWindows] count];
  ShowCertificateViewer(web_contents, window, cert.get());
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(web_contents_modal_dialog_manager->IsDialogActive());

  EXPECT_EQ(num_child_windows + 1, [[window childWindows] count]);
  // Assume the last child is the overlay window that was added.
  NSWindow* overlay_window = [[window childWindows] lastObject];
  NSWindow* dialog_sheet = [overlay_window attachedSheet];
  EXPECT_TRUE(dialog_sheet);
  NSRect sheet_frame = [dialog_sheet frame];

  // Verify the certificate viewer is showing and accepts mouse events.
  CheckCertificateViewerVisibility(overlay_window, dialog_sheet, true);

  // Switch to another tab and verify that |overlay_window| and |dialog_sheet|
  // are not blocking mouse events, and |dialog_sheet| is hidden.
  AddBlankTabAndShow(browser());
  CheckCertificateViewerVisibility(overlay_window, dialog_sheet, false);
  EXPECT_NSEQ(sheet_frame, [dialog_sheet frame]);

  // Switch back and verify that the sheet is shown.
  chrome::SelectNumberedTab(browser(), 0);
  base::RunLoop().RunUntilIdle();
  CheckCertificateViewerVisibility(overlay_window, dialog_sheet, true);
}
