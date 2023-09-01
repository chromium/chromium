// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/autofill/ios/common/features.h"

// Features that are exlusive to iOS go here in alphabetical order.

// Controls whether to use the isolated content world instead of the page
// content world for the Autofill JS feature scripts.
// TODO(crbug.com/1175793) Remove once the isolated content world is launched
// for Autofill.
BASE_FEATURE(kAutofillIsolatedWorldForJavascriptIos,
             "AutofillIsolatedWorldForJavascriptIos",
             base::FEATURE_DISABLED_BY_DEFAULT);
