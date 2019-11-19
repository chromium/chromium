// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_SEARCH_MOCK_EMBEDDED_SEARCH_CLIENT_H_
#define CHROME_COMMON_SEARCH_MOCK_EMBEDDED_SEARCH_CLIENT_H_

#include "chrome/common/search.mojom.h"
#include "chrome/common/search/instant_types.h"
#include "testing/gmock/include/gmock/gmock.h"

class MockEmbeddedSearchClient : public chrome::mojom::EmbeddedSearchClient {
 public:
  MockEmbeddedSearchClient();
  ~MockEmbeddedSearchClient() override;

  MOCK_METHOD1(AutocompleteResultChanged,
               void(chrome::mojom::AutocompleteResultPtr result));
  MOCK_METHOD1(SetPageSequenceNumber, void(int));
  MOCK_METHOD2(FocusChanged, void(OmniboxFocusState, OmniboxFocusChangeReason));
  MOCK_METHOD1(MostVisitedInfoChanged, void(const InstantMostVisitedInfo&));
  MOCK_METHOD1(SetInputInProgress, void(bool));
  MOCK_METHOD1(ThemeChanged, void(const NtpTheme&));
  MOCK_METHOD0(LocalBackgroundSelected, void());
};

#endif  // CHROME_COMMON_SEARCH_MOCK_EMBEDDED_SEARCH_CLIENT_H_
