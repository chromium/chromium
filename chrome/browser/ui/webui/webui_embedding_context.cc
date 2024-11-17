// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/webui_embedding_context.h"

#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_user_data.h"

namespace webui {
namespace {

// Tracks changes to the wrapped `browser_window_interface_`, notifying clients
// of changes as approprirate.
class EmbeddingBrowserTracker {
 public:
  explicit EmbeddingBrowserTracker(base::RepeatingClosure browser_change_cb)
      : browser_change_cb_(std::move(browser_change_cb)) {}
  EmbeddingBrowserTracker(const EmbeddingBrowserTracker&) = delete;
  EmbeddingBrowserTracker& operator=(const EmbeddingBrowserTracker&) = delete;
  ~EmbeddingBrowserTracker() = default;

  // Updates `browser_window_interface_` and registers/notifies listeners if
  // appropriate.
  void SetBrowserWindowInterface(
      BrowserWindowInterface* browser_window_interface) {
    if (browser_window_interface_ == browser_window_interface) {
      return;
    }
    browser_did_close_subscription_.reset();
    browser_window_interface_ = browser_window_interface;

    if (browser_window_interface_) {
      browser_did_close_subscription_ =
          browser_window_interface_->RegisterBrowserDidClose(
              base::BindRepeating(&EmbeddingBrowserTracker::OnBrowserDidClose,
                                  base::Unretained(this)));
    }
    browser_change_cb_.Run();
  }

  BrowserWindowInterface* browser_window_interface() {
    return browser_window_interface_;
  }

 private:
  void OnBrowserDidClose(BrowserWindowInterface* browser_window_interface) {
    CHECK_EQ(browser_window_interface_, browser_window_interface);
    SetBrowserWindowInterface(nullptr);
  }

  // The browser interface currently embedding the host contents.
  raw_ptr<BrowserWindowInterface> browser_window_interface_ = nullptr;

  // Notifies this when `browser_window_interface_` has closed.
  std::optional<base::CallbackListSubscription> browser_did_close_subscription_;

  // Notifies clients of changes to `browser_window_interface_`.
  base::RepeatingClosure browser_change_cb_;
};

// Tracks changes to the wrapped `tab_interface_`, notifying clients of changes
// as approprirate.
class EmbeddingTabTracker {
 public:
  EmbeddingTabTracker(base::RepeatingClosure tab_change_cb,
                      base::RepeatingClosure browser_change_cb)
      : tab_change_cb_(std::move(tab_change_cb)),
        browser_change_cb_(std::move(browser_change_cb)) {}
  EmbeddingTabTracker(const EmbeddingTabTracker&) = delete;
  EmbeddingTabTracker& operator=(const EmbeddingTabTracker&) = delete;
  ~EmbeddingTabTracker() = default;

  // Updates `tab_interface_` and registers/notifies listeners if appropriate.
  void SetTabInterface(tabs::TabInterface* tab_interface) {
    if (tab_interface_ == tab_interface) {
      return;
    }
    tab_will_detach_subscription_.reset();
    tab_did_insert_subscription_.reset();
    tab_interface_ = tab_interface;

    if (tab_interface) {
      tab_will_detach_subscription_ =
          tab_interface_->RegisterWillDetach(base::BindRepeating(
              &EmbeddingTabTracker::OnTabWillDetach, base::Unretained(this)));
      tab_did_insert_subscription_ =
          tab_interface_->RegisterDidInsert(base::BindRepeating(
              &EmbeddingTabTracker::OnTabDidInsert, base::Unretained(this)));
    }

    // Both browser and tab changes should be propagated.
    browser_change_cb_.Run();
    tab_change_cb_.Run();
  }

  tabs::TabInterface* tab_interface() { return tab_interface_; }

 private:
  void OnTabWillDetach(tabs::TabInterface* tab_interface,
                       tabs::TabInterface::DetachReason detach_reason) {
    CHECK_EQ(tab_interface_, tab_interface);
    if (detach_reason == tabs::TabInterface::DetachReason::kDelete) {
      SetTabInterface(nullptr);
    }
  }

  void OnTabDidInsert(tabs::TabInterface* tab_interface) {
    CHECK_EQ(tab_interface_, tab_interface);
    browser_change_cb_.Run();
  }

  // The tab interface currently embedding the host contents.
  raw_ptr<tabs::TabInterface> tab_interface_ = nullptr;

  // Notifies this when `tab_interface_` will detach, tracked to invalidate
  // the tab before destruction.
  std::optional<base::CallbackListSubscription> tab_will_detach_subscription_;

  // Notifies this when `tab_interface_` is inserted into a new browser window,
  // tracked to ensure clients are notified of browser window changes.
  std::optional<base::CallbackListSubscription> tab_did_insert_subscription_;

  // Notifies clients of changes to `tab_interface_`.
  base::RepeatingClosure tab_change_cb_;

  // Notifies clients of changes to the `tab_interface_`'s browser.
  base::RepeatingClosure browser_change_cb_;
};

// Responsible for managing embedding interface changes for the hosted
// WebContents, notifing downstream clients of embedding context changes.
class EmbedderContextData
    : public content::WebContentsUserData<EmbedderContextData> {
 public:
  EmbedderContextData(const EmbedderContextData&) = delete;
  EmbedderContextData& operator=(const EmbedderContextData&) = delete;
  ~EmbedderContextData() override = default;

  static EmbedderContextData* GetOrCreate(content::WebContents* web_contents) {
    EmbedderContextData::CreateForWebContents(web_contents);
    return EmbedderContextData::FromWebContents(web_contents);
  }

  void SetBrowserWindowInterface(
      BrowserWindowInterface* browser_window_interface) {
    // Tabs will always belong to a browser and are tracked together, both
    // browser and tab trackers should not be set independently.
    CHECK(!tab_tracker_)
        << "Browser and tab trackers should not be set independently";
    if (!browser_tracker_) {
      browser_tracker_ = std::make_unique<EmbeddingBrowserTracker>(
          base::BindRepeating(&EmbedderContextData::NotifyBrowserChanged,
                              base::Unretained(this)));
    }
    browser_tracker_->SetBrowserWindowInterface(browser_window_interface);
  }

  void SetTabInterface(tabs::TabInterface* tab_interface) {
    // Tabs will always belong to a browser and are tracked together, both
    // browser and tab trackers should not be set independently.
    CHECK(!browser_tracker_)
        << "Browser and tab trackers should not be set independently";
    if (!tab_tracker_) {
      tab_tracker_ = std::make_unique<EmbeddingTabTracker>(
          base::BindRepeating(&EmbedderContextData::NotifyTabChanged,
                              base::Unretained(this)),
          base::BindRepeating(&EmbedderContextData::NotifyBrowserChanged,
                              base::Unretained(this)));
    }
    tab_tracker_->SetTabInterface(tab_interface);
  }

  BrowserWindowInterface* GetBrowserWindowInterface() {
    // Source the browser interface either directly from the tracked browser or
    // via the tracked tab.
    if (tab_tracker_) {
      return tab_tracker_->tab_interface()
                 ? tab_tracker_->tab_interface()->GetBrowserWindowInterface()
                 : nullptr;
    }
    if (browser_tracker_) {
      return browser_tracker_->browser_window_interface();
    }
    return nullptr;
  }

  tabs::TabInterface* GetTabInterface() {
    return tab_tracker_ ? tab_tracker_->tab_interface() : nullptr;
  }

  base::CallbackListSubscription RegisterBrowserWindowInterfaceChanged(
      base::RepeatingClosure browser_changed_cb) {
    return browser_change_callbacks_.Add(std::move(browser_changed_cb));
  }

  base::CallbackListSubscription RegisterTabInterfaceChanged(
      base::RepeatingClosure tab_change_cb) {
    return tab_change_callbacks_.Add(std::move(tab_change_cb));
  }

 private:
  friend class content::WebContentsUserData<EmbedderContextData>;

  explicit EmbedderContextData(content::WebContents* web_contents)
      : WebContentsUserData<EmbedderContextData>(*web_contents) {}

  // Notifies clients their embedding browser has changed.
  void NotifyBrowserChanged() { browser_change_callbacks_.Notify(); }

  // Notifies clients their embedding tab has changed.
  void NotifyTabChanged() { tab_change_callbacks_.Notify(); }

  // Defined only if actively tracking an embedding browser.
  std::unique_ptr<EmbeddingBrowserTracker> browser_tracker_;

  // Defined only if actively tracking an embedding tab.
  std::unique_ptr<EmbeddingTabTracker> tab_tracker_;

  // Client registrations for changes to the embedding browser.
  base::RepeatingCallbackList<void()> browser_change_callbacks_;

  // Client registrations for changes to the embedding tab.
  base::RepeatingCallbackList<void()> tab_change_callbacks_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

WEB_CONTENTS_USER_DATA_KEY_IMPL(EmbedderContextData);

}  // namespace

base::CallbackListSubscription InitEmbeddingContext(
    tabs::TabInterface* tab_interface) {
  // Set the initial tab interface and configure updates for discard changes.
  // TODO(crbug.com/371950942): Once the new discard implementation has landed
  // there is no need to re-set the interface on discard and this can be inlined
  // into TabModel.
  SetTabInterface(tab_interface->GetContents(), tab_interface);
  return tab_interface->RegisterWillDiscardContents(base::BindRepeating(
      [](tabs::TabInterface* tab, content::WebContents* old_contents,
         content::WebContents* new_contents) {
        SetTabInterface(new_contents, tab);
      }));
}

void SetBrowserWindowInterface(
    content::WebContents* host_contents,
    BrowserWindowInterface* browser_window_interface) {
  EmbedderContextData::GetOrCreate(host_contents)
      ->SetBrowserWindowInterface(browser_window_interface);
}

void SetTabInterface(content::WebContents* host_contents,
                     tabs::TabInterface* tab_interface) {
  EmbedderContextData::GetOrCreate(host_contents)
      ->SetTabInterface(tab_interface);
}

BrowserWindowInterface* GetBrowserWindowInterface(
    content::WebContents* host_contents) {
  return EmbedderContextData::GetOrCreate(host_contents)
      ->GetBrowserWindowInterface();
}

tabs::TabInterface* GetTabInterface(content::WebContents* host_contents) {
  return EmbedderContextData::GetOrCreate(host_contents)->GetTabInterface();
}

base::CallbackListSubscription RegisterBrowserWindowInterfaceChanged(
    content::WebContents* host_contents,
    base::RepeatingClosure context_changed_cb) {
  return EmbedderContextData::GetOrCreate(host_contents)
      ->RegisterBrowserWindowInterfaceChanged(std::move(context_changed_cb));
}

base::CallbackListSubscription RegisterTabInterfaceChanged(
    content::WebContents* host_contents,
    base::RepeatingClosure context_changed_cb) {
  return EmbedderContextData::GetOrCreate(host_contents)
      ->RegisterTabInterfaceChanged(std::move(context_changed_cb));
}

}  // namespace webui
