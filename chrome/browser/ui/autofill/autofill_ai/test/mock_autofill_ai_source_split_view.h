// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_AI_TEST_MOCK_AUTOFILL_AI_SOURCE_SPLIT_VIEW_H_
#define CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_AI_TEST_MOCK_AUTOFILL_AI_SOURCE_SPLIT_VIEW_H_

#include "base/functional/callback.h"
#include "chrome/browser/ui/autofill/autofill_ai/autofill_ai_source_split_view.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/gurl.h"

namespace autofill {

class MockAutofillAiSourceSplitView : public AutofillAiSourceSplitView {
 public:
  MockAutofillAiSourceSplitView();
  MockAutofillAiSourceSplitView(const MockAutofillAiSourceSplitView&) = delete;
  MockAutofillAiSourceSplitView& operator=(
      const MockAutofillAiSourceSplitView&) = delete;
  ~MockAutofillAiSourceSplitView() override;

  MOCK_METHOD(bool, CanOpen, (const GURL& url), (const, override));
  MOCK_METHOD(bool,
              Open,
              (const GURL& url, base::OnceClosure on_closed),
              (override));
  MOCK_METHOD(void, Close, (), (override));
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_AI_TEST_MOCK_AUTOFILL_AI_SOURCE_SPLIT_VIEW_H_
