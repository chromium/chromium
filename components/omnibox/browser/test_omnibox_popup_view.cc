// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/test_omnibox_popup_view.h"

static_assert(!BUILDFLAG(IS_IOS));

bool TestOmniboxPopupView::IsOpen() const {
  return false;
}
