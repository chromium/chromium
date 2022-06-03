// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_VIEWS_CHROME_TEST_VIEWS_DELEGATE_H_
#define CHROME_TEST_VIEWS_CHROME_TEST_VIEWS_DELEGATE_H_

#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "ui/views/test/test_views_delegate.h"

// A TestViewsDelegate specific to Chrome tests.
template <class T = views::TestViewsDelegate>
class ChromeTestViewsDelegate : public T {
 public:
  ChromeTestViewsDelegate() {
    T::set_layout_provider(ChromeLayoutProvider::CreateLayoutProvider());
  }

  ~ChromeTestViewsDelegate() override = default;
};

#endif  // CHROME_TEST_VIEWS_CHROME_TEST_VIEWS_DELEGATE_H_
