// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/platform_browser_test.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

// TODO(crbug.com/40167066): Investigate why different macOS versions have
// different fingerprints.
#if BUILDFLAG(IS_MAC)
#define MAYBE_VerifyDynamicsCompressorFingerprint \
  DISABLED_VerifyDynamicsCompressorFingerprint
#else
#define MAYBE_VerifyDynamicsCompressorFingerprint \
  VerifyDynamicsCompressorFingerprint
#endif

namespace {

// This test runs on Android as well as desktop platforms.
class WebAudioBrowserTest : public PlatformBrowserTest {
 public:
  content::WebContents* web_contents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }
};

IN_PROC_BROWSER_TEST_F(WebAudioBrowserTest,
                       MAYBE_VerifyDynamicsCompressorFingerprint) {
  ASSERT_TRUE(embedded_test_server()->Start());
  content::DOMMessageQueue messages(web_contents());
  base::RunLoop run_loop;

  ASSERT_TRUE(content::NavigateToURL(
      web_contents(),
      embedded_test_server()->GetURL(
          "/webaudio/calls_dynamics_compressor_fingerprint.html")));

  // The document computes the DynamicsCompressor fingerprint and sends a
  // message back to the test. Receipt of the message indicates that the script
  // successfully completed.
  std::string fingerprint;
  ASSERT_TRUE(messages.WaitForMessage(&fingerprint));

  // NOTE: Changes to Web Audio code that alter the below fingerprints are
  // fine, and are cause for updating these expectations -- the issue is if
  // different devices return different fingerprints.
#if (BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_WIN)) && defined(ARCH_CPU_ARM64)
  // TODO(crbug.com/40160543): Investigate why this fingerprint is different.
  EXPECT_EQ("13.13046550525678", fingerprint);
#else
  EXPECT_EQ("13.130926895706125", fingerprint);
#endif  // (BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_WIN)) &&
        // defined(ARCH_CPU_ARM64)
}

}  // namespace
