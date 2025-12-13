// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/autofill_ml_internals/autofill_ml_internals_page_handler.h"

#include "components/autofill/core/browser/ml_model/logging/ml_log_router.h"
#include "content/public/test/test_web_ui.h"
#include "testing/gtest/include/gtest/gtest.h"

class AutofillMlInternalsPageHandlerTest : public testing::Test {
 public:
  AutofillMlInternalsPageHandlerTest() = default;

 protected:
  autofill::MlLogRouter log_router_;
};

TEST_F(AutofillMlInternalsPageHandlerTest, RegisterAndUnregister) {
  EXPECT_FALSE(log_router_.HasReceivers());
  auto handler = std::make_unique<AutofillMlInternalsPageHandlerImpl>(
      mojo::PendingReceiver<autofill_ml_internals::mojom::PageHandler>(),
      &log_router_);
  EXPECT_TRUE(log_router_.HasReceivers());
  handler.reset();
  EXPECT_FALSE(log_router_.HasReceivers());
}
