// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WIN_JUMPLIST_UPDATER_H_
#define CHROME_BROWSER_WIN_JUMPLIST_UPDATER_H_

#include <shobjidl.h>
#include <windows.h>

#include <stddef.h>
#include <wrl/client.h>

#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "ui/gfx/image/image_skia.h"

// Represents a class used for creating an IShellLink object.
// Even though an IShellLink also needs the absolute path to an application to
// be executed, this class does not have any variables for it because current
// users always use "chrome.exe" as the application.
class ShellLinkItem : public base::RefCountedThreadSafe<ShellLinkItem> {
 public:
  ShellLinkItem();

  ShellLinkItem(const ShellLinkItem&) = delete;
  ShellLinkItem& operator=(const ShellLinkItem&) = delete;

  const std::u16string& title() const { return title_; }
  const base::FilePath& icon_path() const { return icon_path_; }
  const std::string& url() const { return url_; }
  int icon_index() const { return icon_index_; }
  const gfx::ImageSkia& icon_image() const { return icon_image_; }

  std::wstring GetArguments() const;
  base::CommandLine* GetCommandLine();

  void set_title(const std::u16string& title) { title_ = title; }
  void set_icon(const base::FilePath& path, int index) {
    icon_path_ = path;
    icon_index_ = index;
  }
  void set_url(const std::string& url) { url_ = url; }

  void set_icon_image(const gfx::ImageSkia& image) {
    icon_image_ = image;
  }

 private:
  friend class base::RefCountedThreadSafe<ShellLinkItem>;
  ~ShellLinkItem();

  // Used for storing and appending command-line arguments.
  base::CommandLine command_line_;

  // The string to be displayed in a JumpList.
  std::u16string title_;

  // The absolute path to an icon to be displayed in a JumpList.
  base::FilePath icon_path_;

  // The tab URL corresponding to this link's favicon.
  std::string url_;

  // The icon index in the icon file. If an icon file consists of two or more
  // icons, set this value to identify the icon. If an icon file consists of
  // one icon, this value is 0.
  int icon_index_;

  // Icon image. Used by the browser JumpList.
  // Note that an icon path must be supplied to IShellLink, so users of this
  // class must save icon data to disk.
  gfx::ImageSkia icon_image_;
};

typedef std::vector<scoped_refptr<ShellLinkItem> > ShellLinkItemList;


// A utility class that hides the boilerplate for updating Windows JumpLists.
// Note that JumpLists are available in Windows 7 and later only.
//
// Example of usage:
//
//    JumpListUpdater updater(app_id);
//    if (updater.BeginUpdate()) {
//      updater.AddTasks(...);
//      updater.AddCustomCategory(...);
//      updater.CommitUpdate();
//    }
//
// Note:
// - Each JumpListUpdater instance is expected to be used once only.
// - The JumpList must be updated in its entirety, i.e. even if a category has
//   not changed, all its items must be added in each update.
class JumpListUpdater {
 public:
  explicit JumpListUpdater(const std::wstring& app_user_model_id);

  JumpListUpdater(const JumpListUpdater&) = delete;
  JumpListUpdater& operator=(const JumpListUpdater&) = delete;

  ~JumpListUpdater();

  // Returns true if JumpLists are enabled on this OS.
  static bool IsEnabled();

  // Returns the current user setting for the maximum number of items to display
  // in JumpLists. The setting is retrieved in BeginUpdate().
  size_t user_max_items() const { return user_max_items_; }

  // Starts a transaction that updates the JumpList of this application.
  // This must be called prior to updating the JumpList. If this function
  // returns false, this instance should not be used.
  bool BeginUpdate();

  // Commits the update.
  bool CommitUpdate();

  // Updates the predefined "Tasks" category of the JumpList.
  bool AddTasks(const ShellLinkItemList& link_items);

  // Updates an unregistered category of the JumpList.
  // This function cannot update registered categories (such as "Tasks")
  // because special steps are required for updating them.
  // |max_items| specifies the maximum number of items from |link_items| to add
  // to the JumpList.
  bool AddCustomCategory(const std::u16string& category_name,
                         const ShellLinkItemList& link_items,
                         size_t max_items);

  // Removes the Windows JumpList for an app with given app_user_model_id.
  static bool DeleteJumpList(const std::wstring& app_user_model_id);

 private:
  // The app ID.
  std::wstring app_user_model_id_;

  // Windows API interface used to modify JumpLists.
  Microsoft::WRL::ComPtr<ICustomDestinationList> destination_list_;

  // The current user setting for "Number of recent items to display in Jump
  // Lists" option in the "Taskbar and Start Menu Properties".
  size_t user_max_items_;
};

#endif  // CHROME_BROWSER_WIN_JUMPLIST_UPDATER_H_
