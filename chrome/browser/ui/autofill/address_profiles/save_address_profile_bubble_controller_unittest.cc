// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/address_profiles/save_address_profile_bubble_controller.h"

#include "base/test/scoped_feature_list.h"
#include "components/autofill/core/common/autofill_features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

TEST(SaveAddressProfileBubbleControllerTest, NoCrash) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kAutofillAddressProfileSavePrompt);
  SaveAddressProfileBubbleController controller;
}

}  // namespace autofill
