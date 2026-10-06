// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/autofill_tapjacking_protector_impl.h"

#include "base/test/test_future.h"
#include "chrome/browser/ui/autofill/chrome_autofill_client.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/autofill/core/browser/ui/autofill_tapjacking_protector.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace {

class AutofillTapjackingProtectorImplTest
    : public ChromeRenderViewHostTestHarness {
 public:
  AutofillTapjackingProtectorImplTest() = default;
  ~AutofillTapjackingProtectorImplTest() override = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    ChromeAutofillClient::CreateForWebContents(web_contents());
  }

  ChromeAutofillClient* client() {
    return ChromeAutofillClient::FromWebContentsForTesting(web_contents());
  }
};

TEST_F(AutofillTapjackingProtectorImplTest, ShowInvokesCallback) {
  AutofillTapjackingProtectorImpl protector(client());
  base::test::TestFuture<AutofillTapjackingProtector::AuthorizationResult>
      result_future;

  protector.Show(AutofillTapjackingProtector::AuthorizationType::kPayments,
                 result_future.GetCallback());

  EXPECT_EQ(result_future.Get(),
            AutofillTapjackingProtector::AuthorizationResult::kUnknown);
}

}  // namespace
}  // namespace autofill
