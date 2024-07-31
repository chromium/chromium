// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEB_MODAL_WEB_CONTENTS_MODAL_DIALOG_MANAGER_H_
#define COMPONENTS_WEB_MODAL_WEB_CONTENTS_MODAL_DIALOG_MANAGER_H_

#include <memory>

#include "base/containers/circular_deque.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "build/build_config.h"
#include "components/web_modal/single_web_contents_dialog_manager.h"
#include "components/web_modal/web_modal_export.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "ui/gfx/native_widget_types.h"

namespace content {
enum class Visibility;
}  // namespace content

namespace web_modal {

class WebContentsModalDialogManagerDelegate;

// Per-WebContents class to manage WebContents-modal dialogs.
class WEB_MODAL_EXPORT WebContentsModalDialogManager
    : public SingleWebContentsDialogManagerDelegate,
      public content::WebContentsObserver,
      public content::WebContentsUserData<WebContentsModalDialogManager> {
 public:
  // Observes when web modal dialog is about to close as a result of a page
  // navigation.
  class CloseOnNavigationObserver : public base::CheckedObserver {
   public:
    CloseOnNavigationObserver(const CloseOnNavigationObserver&) = delete;
    CloseOnNavigationObserver& operator=(const CloseOnNavigationObserver&) =
        delete;

    virtual void OnWillClose() = 0;

   protected:
    CloseOnNavigationObserver() = default;
  };

  WebContentsModalDialogManager(const WebContentsModalDialogManager&) = delete;
  WebContentsModalDialogManager& operator=(
      const WebContentsModalDialogManager&) = delete;

  ~WebContentsModalDialogManager() override;

  WebContentsModalDialogManagerDelegate* delegate() const { return delegate_; }
  void SetDelegate(WebContentsModalDialogManagerDelegate* d);

  // Allow clients to supply their own native dialog manager. Suitable for
  // bubble clients.
  void ShowDialogWithManager(
      gfx::NativeWindow dialog,
      std::unique_ptr<SingleWebContentsDialogManager> manager);

  // Returns true if any dialogs are active and not closed.
  bool IsDialogActive() const;

  // Focus the topmost modal dialog.  IsDialogActive() must be true when calling
  // this function.
  void FocusTopmostDialog() const;

  // Manages observer for when dialogs are closed as a result of page
  // navigation.
  void AddCloseOnNavigationObserver(CloseOnNavigationObserver* observer);
  void RemoveCloseOnNavigationObserver(CloseOnNavigationObserver* observer);

  // SingleWebContentsDialogManagerDelegate:
  content::WebContents* GetWebContents() const override;
  void WillClose(gfx::NativeWindow dialog) override;

  // For testing.
  class TestApi {
   public:
    explicit TestApi(WebContentsModalDialogManager* manager)
        : manager_(manager) {}

    TestApi(const TestApi&) = delete;
    TestApi& operator=(const TestApi&) = delete;

    void CloseAllDialogs() { manager_->CloseAllDialogs(); }
    void WebContentsVisibilityChanged(content::Visibility visibility) {
      manager_->OnVisibilityChanged(visibility);
    }

   private:
    raw_ptr<WebContentsModalDialogManager> manager_;
  };

  // Closes all WebContentsModalDialogs.
  void CloseAllDialogs();

 private:
  explicit WebContentsModalDialogManager(content::WebContents* web_contents);
  friend class content::WebContentsUserData<WebContentsModalDialogManager>;

  struct DialogState {
    DialogState(gfx::NativeWindow dialog,
                std::unique_ptr<SingleWebContentsDialogManager> manager);
    DialogState(DialogState&& state);
    ~DialogState();

    gfx::NativeWindow dialog;
    std::unique_ptr<SingleWebContentsDialogManager> manager;
  };

  // Blocks/unblocks interaction with renderer process.
  void BlockWebContentsInteraction(bool blocked);

  bool IsWebContentsVisible() const;

  // Overridden from content::WebContentsObserver:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidGetIgnoredUIEvent() override;
  void OnVisibilityChanged(content::Visibility visibility) override;
  void WebContentsDestroyed() override;

  // Delegate for notifying our owner about stuff. Not owned by us.
  raw_ptr<WebContentsModalDialogManagerDelegate, AcrossTasksDanglingUntriaged>
      delegate_ = nullptr;

  // All active dialogs.
  base::circular_deque<DialogState> child_dialogs_;

  // The WebContents' visibility.
  content::Visibility web_contents_visibility_;

  // True while closing the dialogs on WebContents close.
  bool closing_all_dialogs_ = false;

  // Optional closure to re-enable input events, if we're ignored them.
  std::optional<content::WebContents::ScopedIgnoreInputEvents>
      scoped_ignore_input_events_;

  base::ObserverList<CloseOnNavigationObserver>
      close_on_navigation_observer_list_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace web_modal

#endif  // COMPONENTS_WEB_MODAL_WEB_CONTENTS_MODAL_DIALOG_MANAGER_H_
