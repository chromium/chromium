// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/cr_components/searchbox/contextual_searchbox_tab_favicon_helper.h"

#include <utility>
#include <vector>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/timer/timer.h"
#include "components/favicon/content/content_favicon_driver.h"
#include "components/favicon/core/favicon_driver_observer.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/gfx/image/image.h"

namespace {

std::optional<GURL> GetFaviconDataUrl(content::WebContents* web_contents) {
  if (!web_contents) {
    return std::nullopt;
  }
  auto* driver = favicon::ContentFaviconDriver::FromWebContents(web_contents);
  if (!driver || !driver->FaviconIsValid()) {
    return std::nullopt;
  }
  gfx::Image favicon = driver->GetFavicon();
  if (favicon.IsEmpty()) {
    return std::nullopt;
  }
  SkBitmap bitmap = favicon.AsBitmap();
  if (bitmap.isNull()) {
    return std::nullopt;
  }
  return GURL(webui::GetBitmapDataUrl(bitmap));
}

}  // namespace

class ContextualSearchboxTabFaviconHelper::Waiter
    : public content::WebContentsObserver,
      public favicon::FaviconDriverObserver {
 public:
  Waiter(ContextualSearchboxTabFaviconHelper* owner,
         tabs::TabInterface* tab,
         base::OnceCallback<void(const std::optional<GURL>&)> callback)
      : content::WebContentsObserver(tab->GetContents()),
        owner_(owner),
        callback_(std::move(callback)) {
    if (web_contents()) {
      if (auto* driver =
              favicon::ContentFaviconDriver::FromWebContents(web_contents())) {
        driver->AddObserver(this);
      }
    }
    will_detach_subscription_ = tab->RegisterWillDetach(
        base::BindRepeating(&Waiter::OnWillDetach, base::Unretained(this)));
    timer_.Start(FROM_HERE, base::Seconds(10),
                 base::BindOnce(&Waiter::OnTimeout, base::Unretained(this)));
  }

  ~Waiter() override {
    if (web_contents()) {
      if (auto* driver =
              favicon::ContentFaviconDriver::FromWebContents(web_contents())) {
        driver->RemoveObserver(this);
      }
    }
    if (callback_) {
      std::move(callback_).Run(std::nullopt);
    }
  }

  void OnWillDetach(tabs::TabInterface* tab,
                    tabs::TabInterface::DetachReason reason) {
    if (reason == tabs::TabInterface::DetachReason::kDelete) {
      RunCallbackAndRemoveWaiter(std::nullopt);
    }
  }

  void DidFinishLoad(content::RenderFrameHost* render_frame_host,
                     const GURL& validated_url) override {
    if (!render_frame_host->IsInPrimaryMainFrame()) {
      return;
    }
    CheckFavicon();
  }

  void OnFaviconUpdated(favicon::FaviconDriver* favicon_driver,
                        NotificationIconType notification_icon_type,
                        const GURL& icon_url,
                        bool icon_url_changed,
                        const gfx::Image& image) override {
    if (notification_icon_type != FaviconDriverObserver::NON_TOUCH_16_DIP) {
      return;
    }
    CheckFavicon();
  }

 private:
  void OnTimeout() { RunCallbackAndRemoveWaiter(std::nullopt); }

  void CheckFavicon() {
    if (auto data_url = GetFaviconDataUrl(web_contents())) {
      RunCallbackAndRemoveWaiter(std::move(*data_url));
    }
  }

  void RunCallbackAndRemoveWaiter(std::optional<GURL> data_url) {
    if (!callback_) {
      return;
    }
    std::move(callback_).Run(std::move(data_url));
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&ContextualSearchboxTabFaviconHelper::RemoveWaiter,
                       owner_->weak_ptr_factory_.GetWeakPtr(), this));
  }

  raw_ptr<ContextualSearchboxTabFaviconHelper> owner_;
  base::OnceCallback<void(const std::optional<GURL>&)> callback_;
  base::OneShotTimer timer_;
  base::CallbackListSubscription will_detach_subscription_;
};

ContextualSearchboxTabFaviconHelper::ContextualSearchboxTabFaviconHelper() =
    default;

ContextualSearchboxTabFaviconHelper::~ContextualSearchboxTabFaviconHelper() =
    default;

void ContextualSearchboxTabFaviconHelper::WaitForTabFaviconLoad(
    int32_t tab_id,
    base::OnceCallback<void(const std::optional<GURL>&)> callback) {
  const tabs::TabHandle handle = tabs::TabHandle(tab_id);
  tabs::TabInterface* const tab = handle.Get();
  if (!tab || !tab->GetContents()) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  content::WebContents* web_contents = tab->GetContents();
  if (!web_contents->IsLoading()) {
    if (auto data_url = GetFaviconDataUrl(web_contents)) {
      std::move(callback).Run(std::move(*data_url));
      return;
    }
  }

  auto waiter = std::make_unique<Waiter>(this, tab, std::move(callback));
  active_waiters_.push_back(std::move(waiter));
}

void ContextualSearchboxTabFaviconHelper::RemoveWaiter(Waiter* waiter) {
  std::erase_if(active_waiters_, [waiter](const std::unique_ptr<Waiter>& w) {
    return w.get() == waiter;
  });
}
