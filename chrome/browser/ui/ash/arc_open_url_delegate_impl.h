// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_ARC_OPEN_URL_DELEGATE_IMPL_H_
#define CHROME_BROWSER_UI_ASH_ARC_OPEN_URL_DELEGATE_IMPL_H_

#include <string>

#include "ash/components/arc/mojom/intent_helper.mojom.h"
#include "base/containers/flat_map.h"
#include "components/arc/intent_helper/open_url_delegate.h"

// Implements arc::OpenUrlDelegate to inject dependency.
// This has dependency to ChromeNewWindowClient, so needs to be initialized
// after it, and destroy before its destruction.
class ArcOpenUrlDelegateImpl : public arc::OpenUrlDelegate {
 public:
  ArcOpenUrlDelegateImpl();
  ArcOpenUrlDelegateImpl(const ArcOpenUrlDelegateImpl&) = delete;
  ArcOpenUrlDelegateImpl& operator=(const ArcOpenUrlDelegateImpl&) = delete;
  ~ArcOpenUrlDelegateImpl() override;

  // Returns the global instance only for testing purpose.
  static ArcOpenUrlDelegateImpl* GetForTesting();

  // arc::OpenUrlDelegate:
  void OpenUrlFromArc(const GURL& url) override;
  void OpenWebAppFromArc(const GURL& url) override;
  void OpenArcCustomTab(
      const GURL& url,
      int32_t task_id,
      arc::mojom::IntentHelperHost::OnOpenCustomTabCallback callback) override;
  void OpenChromePageFromArc(arc::mojom::ChromePage page) override;
  void OpenAppWithIntent(const GURL& start_url,
                         arc::mojom::LaunchIntentPtr intent) override;

 private:
  // TODO(hidehiko): Move these out form members.
  const base::flat_map<arc::mojom::ChromePage, std::string> os_settings_pages_;
  const base::flat_map<arc::mojom::ChromePage, std::string>
      browser_settings_pages_;
  const base::flat_map<arc::mojom::ChromePage, std::string> about_pages_;
};

#endif  // CHROME_BROWSER_UI_ASH_ARC_OPEN_URL_DELEGATE_IMPL_H_
