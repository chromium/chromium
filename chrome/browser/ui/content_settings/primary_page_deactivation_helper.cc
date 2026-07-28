// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/content_settings/primary_page_deactivation_helper.h"

#include "base/check.h"
#include "base/functional/callback.h"
#include "content/public/browser/page.h"
#include "content/public/browser/page_user_data.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"

namespace chrome {

namespace {

class PageDeactivationHelper
    : public content::PageUserData<PageDeactivationHelper>,
      public content::WebContentsObserver {
 public:
  void AddCallback(base::OnceClosure callback) {
    callbacks_.push_back(std::move(callback));
  }

  PageDeactivationHelper(const PageDeactivationHelper&) = delete;
  PageDeactivationHelper& operator=(const PageDeactivationHelper&) = delete;

  ~PageDeactivationHelper() override { RunCallbacks(); }

 private:
  friend PageUserData;
  PAGE_USER_DATA_KEY_DECL();

  explicit PageDeactivationHelper(content::Page& page)
      : content::PageUserData<PageDeactivationHelper>(page),
        content::WebContentsObserver(content::WebContents::FromRenderFrameHost(
            &page.GetMainDocument())) {
    CHECK(content::WebContents::FromRenderFrameHost(&page.GetMainDocument()));
    CHECK(page.IsPrimary());
  }

  // WebContentsObserver:
  void PrimaryPageWillBeDeactivated(content::Page& page_param) override {
    if (&page_param == &page()) {
      RunCallbacks();
      DeleteForPage(page());
    }
  }

  void RunCallbacks() {
    for (auto& callback : callbacks_) {
      std::move(callback).Run();
    }
    callbacks_.clear();
  }

  std::vector<base::OnceClosure> callbacks_;
};

PAGE_USER_DATA_KEY_IMPL(PageDeactivationHelper);

}  // namespace

void RegisterPrimaryPageDeactivationCallback(content::Page& page,
                                             base::OnceClosure callback) {
  PageDeactivationHelper::GetOrCreateForPage(page)->AddCallback(
      std::move(callback));
}

}  // namespace chrome
