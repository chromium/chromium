// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/autofill/ios/common/features.h"

// Features that are exlusive to iOS go here in alphabetical order.

// Controls whether to dynamically load the address input fields in the save
// flow and settings based on the country value.
// TODO(crbug.com/40281788): Remove once launched.
BASE_FEATURE(kAutofillDynamicallyLoadsFieldsForAddressInput,
             "AutofillDynamicallyLoadsFieldsForAddressInput",
             base::FEATURE_DISABLED_BY_DEFAULT);

// LINT.IfChange(autofill_isolated_content_world)
// Controls whether to use the isolated content world instead of the page
// content world for the Autofill JS feature scripts.
// TODO(crbug.com/40747550) Remove once the isolated content world is launched
// for Autofill.
BASE_FEATURE(kAutofillIsolatedWorldForJavascriptIos,
             "AutofillIsolatedWorldForJavascriptIos",
             base::FEATURE_DISABLED_BY_DEFAULT);
// LINT.ThenChange(/components/autofill/ios/form_util/resources/autofill_form_features.ts:autofill_isolated_content_world)

// Makes the autofill and password infobars sticky on iOS. The sticky infobar
// sticks there until navigating from an explicit user gesture (e.g. reload or
// load a new page from the omnibox). This includes the infobar UI and the
// badge. The badge may remain there after the infobar UI is dismissed from
// timeout but will be dismissed once navigating from an explicit user gesture.
BASE_FEATURE(kAutofillStickyInfobarIos,
             "AutofillStickyInfobarIos",
             base::FEATURE_ENABLED_BY_DEFAULT);
