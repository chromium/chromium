// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <memory>

#include "base/macros.h"
#include "base/test/task_environment.h"
#include "components/omnibox/browser/autocomplete_controller.h"
#include "components/omnibox/browser/autocomplete_provider.h"
#include "components/omnibox/browser/omnibox_client.h"
#include "components/omnibox/browser/omnibox_controller.h"
#include "components/omnibox/browser/test_omnibox_client.h"
#include "components/sessions/core/session_id.h"
#include "testing/gtest/include/gtest/gtest.h"

class OmniboxControllerTest : public testing::Test {
 protected:
  OmniboxControllerTest();
  ~OmniboxControllerTest() override;

  void CreateController();
  void AssertProviders(int expected_providers);

  const AutocompleteController::Providers& GetAutocompleteProviders() const {
    return omnibox_controller_->autocomplete_controller()->providers();
  }

 private:
  // testing::Test:
  void SetUp() override;
  void TearDown() override;

  base::test::SingleThreadTaskEnvironment task_environment_;
  std::unique_ptr<TestOmniboxClient> omnibox_client_;
  std::unique_ptr<OmniboxController> omnibox_controller_;

  DISALLOW_COPY_AND_ASSIGN(OmniboxControllerTest);
};

OmniboxControllerTest::OmniboxControllerTest() {}

OmniboxControllerTest::~OmniboxControllerTest() {}

void OmniboxControllerTest::CreateController() {
  DCHECK(omnibox_client_);
  omnibox_controller_ =
      std::make_unique<OmniboxController>(nullptr, omnibox_client_.get());
}

// Checks that the list of autocomplete providers used by the OmniboxController
// matches the one in the |expected_providers| bit field.
void OmniboxControllerTest::AssertProviders(int expected_providers) {
  const AutocompleteController::Providers& providers =
      GetAutocompleteProviders();

  for (size_t i = 0; i < providers.size(); ++i) {
    // Ensure this is a provider we wanted.
    int type = providers[i]->type();
    ASSERT_TRUE(expected_providers & type);

    // Remove it from expectations so we fail if it's there twice.
    expected_providers &= ~type;
  }

  // Ensure we saw all the providers we expected.
  ASSERT_EQ(0, expected_providers);
}

void OmniboxControllerTest::SetUp() {
  omnibox_client_ = std::make_unique<TestOmniboxClient>();
}

void OmniboxControllerTest::TearDown() {
  omnibox_controller_.reset();
  omnibox_client_.reset();
}

TEST_F(OmniboxControllerTest, CheckDefaultAutocompleteProviders) {
  CreateController();
  // First collect the basic providers.
  int observed_providers = 0;
  const AutocompleteController::Providers& providers =
      GetAutocompleteProviders();
  for (size_t i = 0; i < providers.size(); ++i)
    observed_providers |= providers[i]->type();
  // Ensure we have at least one provider.
  ASSERT_NE(0, observed_providers);

  // Ensure instant extended includes all the provides in classic Chrome.
  int providers_with_instant_extended = observed_providers;
  // TODO(beaudoin): remove TYPE_SEARCH once it's no longer needed to pass
  // the Instant suggestion through via FinalizeInstantQuery.
  CreateController();
  AssertProviders(providers_with_instant_extended);
}
