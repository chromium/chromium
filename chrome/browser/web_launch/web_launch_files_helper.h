// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_LAUNCH_WEB_LAUNCH_FILES_HELPER_H_
#define CHROME_BROWSER_WEB_LAUNCH_WEB_LAUNCH_FILES_HELPER_H_

#include <vector>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/file_system_access_entry_factory.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_directory_handle.mojom-forward.h"
#include "third_party/blink/public/mojom/web_launch/web_launch.mojom.h"
#include "url/gurl.h"

namespace content {

class WebContents;
class NavigationHandle;

}  // namespace content

namespace web_launch {

// A helper for sending launch paths to the renderer process.
// Launch files cannot be sent immediately because the data is stored on a
// document for |launch_url_|, which is not created until |launch_url_| is
// committed.
//
// Note: The lifetime of this class is tied to the WebContents it is attached
// to. However, in general it will be destroyed before the WebContents, when the
// helper sends the FileSystemAccessEntries to the renderer.
class WebLaunchFilesHelper
    : public content::WebContentsObserver,
      public content::WebContentsUserData<WebLaunchFilesHelper> {
 public:
  WEB_CONTENTS_USER_DATA_KEY_DECL();
  static void SetLaunchPaths(content::WebContents* web_contents,
                             const GURL& launch_url,
                             std::vector<base::FilePath> launch_paths);

  // System Web Apps Only. |launch_dir| is prepended to |launch_entries_| and
  // sent to the JavaScript side.
  static void SetLaunchDirectoryAndLaunchPaths(
      content::WebContents* web_contents,
      const GURL& launch_url,
      base::FilePath launch_dir,
      std::vector<base::FilePath> launch_paths);

  WebLaunchFilesHelper(content::WebContents* web_contents,
                       const GURL& launch_url,
                       std::vector<base::FilePath> launch_paths);

  // System Web Apps Only.
  WebLaunchFilesHelper(content::WebContents* web_contents,
                       const GURL& launch_url,
                       base::FilePath launch_dir,
                       std::vector<base::FilePath> launch_paths);

  ~WebLaunchFilesHelper() override;

  // content::WebContentsObserver:
  void DidFinishNavigation(content::NavigationHandle* handle) override;

 private:
  // Sends the launch entries to the renderer if they have been created and the
  // renderer is ready to receive them.
  void MaybeSendLaunchEntries();

  // After a permission check, tries to send the launch entries to the renderer.
  void MaybeSendLaunchEntriesWithPermission(ContentSetting content_setting);

  // The entries causing the launch (may be empty).
  std::vector<blink::mojom::FileSystemAccessEntryPtr> launch_entries_;

  // The url the launch entries are for.
  GURL launch_url_;

  base::WeakPtrFactory<WebLaunchFilesHelper> weak_ptr_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(WebLaunchFilesHelper);
};

}  // namespace web_launch

#endif  // CHROME_BROWSER_WEB_LAUNCH_WEB_LAUNCH_FILES_HELPER_H_
