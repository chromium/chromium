// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_UI_MOCK_AUTOFILL_TAPJACKING_PROTECTOR_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_UI_MOCK_AUTOFILL_TAPJACKING_PROTECTOR_H_

#include "base/functional/callback.h"
#include "components/autofill/core/browser/ui/autofill_tapjacking_protector.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill {

class MockAutofillTapjackingProtector : public AutofillTapjackingProtector {
 public:
  MockAutofillTapjackingProtector();
  ~MockAutofillTapjackingProtector() override;

  MockAutofillTapjackingProtector(const MockAutofillTapjackingProtector&) =
      delete;
  MockAutofillTapjackingProtector& operator=(
      const MockAutofillTapjackingProtector&) = delete;

  MOCK_METHOD(void,
              Show,
              (AuthorizationType authorization_type,
               AuthorizationCallback callback),
              (override));
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_UI_MOCK_AUTOFILL_TAPJACKING_PROTECTOR_H_
