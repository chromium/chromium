// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/profile_management_types.h"

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/frame/browser_view.h"

PostHostClearedCallback CombinePostHostClearedCallbacks(
    PostHostClearedCallback first_post_host_cleared_callback,
    PostHostClearedCallback second_post_host_cleared_callback) {
  return PostHostClearedCallback(base::BindOnce(
      [](PostHostClearedCallback cb1, PostHostClearedCallback cb2,
         Browser* browser) {
        if (!cb1->is_null()) {
          std::move(*cb1).Run(browser);
        }
        if (!cb2->is_null()) {
          std::move(*cb2).Run(browser);
        }
      },
      std::move(first_post_host_cleared_callback),
      std::move(second_post_host_cleared_callback)));
}
