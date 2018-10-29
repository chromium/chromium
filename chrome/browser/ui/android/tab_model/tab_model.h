// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_TAB_MODEL_TAB_MODEL_H_
#define CHROME_BROWSER_UI_ANDROID_TAB_MODEL_TAB_MODEL_H_

#include <memory>

#include "base/macros.h"
#include "chrome/browser/ui/android/tab_model/android_live_tab_context.h"
#include "components/omnibox/browser/toolbar_model.h"
#include "components/omnibox/browser/toolbar_model_delegate.h"
#include "components/sessions/core/session_id.h"
#include "components/sync_sessions/synced_window_delegate.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

namespace browser_sync {
class SyncedWindowDelegateAndroid;
}

namespace content {
class WebContents;
}

namespace sync_sessions {
class SyncedWindowDelegate;
}

class Profile;
class TabAndroid;
class TabModelObserver;

// Abstract representation of a Tab Model for Android.  Since Android does
// not use Browser/BrowserList, this is required to allow Chrome to interact
// with Android's Tabs and Tab Model.
class TabModel : public content::NotificationObserver {
 public:
  // TODO(chrisha): Clean up these enums so that the Java ones are generated
  // from them.
  // https://chromium.googlesource.com/chromium/src/+/lkcr/docs/android_accessing_cpp_enums_in_java.md

  // Various ways tabs can be launched. See TabModel.java.
  enum class TabLaunchType {
    FROM_LINK,
    FROM_EXTERNAL_APP,
    FROM_CHROME_UI,
    FROM_RESTORE,
    FROM_LONGPRESS_FOREGROUND,
    FROM_LONGPRESS_BACKGROUND,
    FROM_REPARENTING,
    FROM_LAUNCHER_SHORTCUT,
    FROM_SPECULATIVE_BACKGROUND_CREATION,
    FROM_BROWSER_ACTIONS,

    // Must be last.
    SIZE
  };

  // Various ways tabs can be selected. See TabModel.java.
  enum class TabSelectionType {
    FROM_CLOSE,
    FROM_EXIT,
    FROM_NEW,
    FROM_USER,

    // Must be last.
    SIZE
  };

  virtual Profile* GetProfile() const;
  virtual bool IsOffTheRecord() const;
  virtual sync_sessions::SyncedWindowDelegate* GetSyncedWindowDelegate() const;
  virtual SessionID GetSessionId() const;
  virtual sessions::LiveTabContext* GetLiveTabContext() const;

  virtual int GetTabCount() const = 0;
  virtual int GetActiveIndex() const = 0;
  virtual content::WebContents* GetActiveWebContents() const;
  virtual content::WebContents* GetWebContentsAt(int index) const = 0;
  // This will return NULL if the tab has not yet been initialized.
  virtual TabAndroid* GetTabAt(int index) const = 0;

  virtual void SetActiveIndex(int index) = 0;
  virtual void CloseTabAt(int index) = 0;

  // Used for restoring tabs from synced foreign sessions.
  virtual void CreateTab(TabAndroid* parent,
                         content::WebContents* web_contents,
                         int parent_tab_id) = 0;

  // Used by Developer Tools to create a new tab with a given URL.
  // Replaces CreateTabForTesting.
  virtual content::WebContents* CreateNewTabForDevTools(const GURL& url) = 0;

  // Return true if we are currently restoring sessions asynchronously.
  virtual bool IsSessionRestoreInProgress() const = 0;

  // Return true if this class is the currently selected in the correspond
  // tab model selector.
  virtual bool IsCurrentModel() const = 0;

  // Adds an observer to this TabModel.
  virtual void AddObserver(TabModelObserver* observer) = 0;

  // Removes an observer from this TabModel.
  virtual void RemoveObserver(TabModelObserver* observer) = 0;

 protected:
  explicit TabModel(Profile* profile, bool is_tabbed_activity);
  ~TabModel() override;

  // Instructs the TabModel to broadcast a notification that all tabs are now
  // loaded from storage.
  void BroadcastSessionRestoreComplete();

  ToolbarModel* GetToolbarModel();

 private:
  // Determines how TabModel will interact with the profile.
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  // The profile associated with this TabModel.
  Profile* profile_;

  // The LiveTabContext associated with TabModel.
  // Used to restore closed tabs through the TabRestoreService.
  std::unique_ptr<AndroidLiveTabContext> live_tab_context_;

  // Describes if this TabModel contains an off-the-record profile.
  bool is_off_the_record_;

  // The SyncedWindowDelegate associated with this TabModel.
  std::unique_ptr<browser_sync::SyncedWindowDelegateAndroid>
      synced_window_delegate_;

  // Unique identifier of this TabModel for session restore. This id is only
  // unique within the current session, and is not guaranteed to be unique
  // across sessions.
  SessionID session_id_;

  // The Registrar used to register TabModel for notifications.
  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(TabModel);
};

#endif  // CHROME_BROWSER_UI_ANDROID_TAB_MODEL_TAB_MODEL_H_
