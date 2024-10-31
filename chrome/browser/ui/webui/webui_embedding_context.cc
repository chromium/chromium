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

// Responsible for holding the BrowserWindowInterface for the embedded
// WebContents and notifing downstream clients of context changes.
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
              base::BindRepeating(&EmbedderContextData::OnBrowserDidClose,
                                  base::Unretained(this)));
    }
    context_change_callbacks_.Notify();
  }

  BrowserWindowInterface* browser_window_interface() {
    return browser_window_interface_;
  }

  base::CallbackListSubscription RegisterBrowserWindowInterfaceChanged(
      base::RepeatingClosure context_changed_cb) {
    return context_change_callbacks_.Add(std::move(context_changed_cb));
  }

 private:
  friend class content::WebContentsUserData<EmbedderContextData>;

  explicit EmbedderContextData(content::WebContents* web_contents)
      : WebContentsUserData<EmbedderContextData>(*web_contents) {}

  void OnBrowserDidClose(BrowserWindowInterface* browser_window_interface) {
    CHECK_EQ(browser_window_interface_, browser_window_interface);
    SetBrowserWindowInterface(nullptr);
  }

  raw_ptr<BrowserWindowInterface> browser_window_interface_ = nullptr;

  // Notifies this when `browser_window_interface_` has closed.
  std::optional<base::CallbackListSubscription> browser_did_close_subscription_;

  // Client registrations for changes to `browser_window_interface_`.
  base::RepeatingCallbackList<void()> context_change_callbacks_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

WEB_CONTENTS_USER_DATA_KEY_IMPL(EmbedderContextData);

}  // namespace

base::CallbackListSubscription InitEmbeddingContext(
    tabs::TabInterface* tab_interface) {
  // Set the initial browser context and configure updates for browser changes.
  const auto update_browser_context = [](tabs::TabInterface* tab) {
    CHECK(tab->GetBrowserWindowInterface());
    SetBrowserWindowInterface(tab->GetContents(),
                              tab->GetBrowserWindowInterface());
  };
  update_browser_context(tab_interface);
  return tab_interface->RegisterDidInsert(
      base::BindRepeating(update_browser_context));
}

void SetBrowserWindowInterface(
    content::WebContents* host_contents,
    BrowserWindowInterface* browser_window_interface) {
  EmbedderContextData::GetOrCreate(host_contents)
      ->SetBrowserWindowInterface(browser_window_interface);
}

BrowserWindowInterface* GetBrowserWindowInterface(
    content::WebContents* host_contents) {
  return EmbedderContextData::GetOrCreate(host_contents)
      ->browser_window_interface();
}

base::CallbackListSubscription RegisterBrowserWindowInterfaceChanged(
    content::WebContents* host_contents,
    base::RepeatingClosure context_changed_cb) {
  return EmbedderContextData::GetOrCreate(host_contents)
      ->RegisterBrowserWindowInterfaceChanged(std::move(context_changed_cb));
}

}  // namespace webui
