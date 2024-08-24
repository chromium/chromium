// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toasts/toast_service.h"

#include "base/containers/enum_set.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/toasts/api/toast_id.h"
#include "chrome/browser/ui/toasts/api/toast_registry.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"

namespace {
using ToastIdEnumSet = base::EnumSet<ToastId, ToastId::kMin, ToastId::kMax>;
}

using ToastServiceBrowserTest = InProcessBrowserTest;

// Verifies that all ToastIds are registered with the toast registry owned by
// the toast service.
IN_PROC_BROWSER_TEST_F(ToastServiceBrowserTest, RegisterAllToastIds) {
  ToastService* const toast_service =
      browser()->browser_window_features()->toast_service();
  const ToastRegistry* const toast_registry = toast_service->toast_registry();

  for (ToastId id : ToastIdEnumSet::All()) {
    EXPECT_NE(toast_registry->GetToastSpecification(id), nullptr);
  }
}
