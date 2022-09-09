// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_FILEICON_SOURCE_H_
#define CHROME_BROWSER_UI_WEBUI_FILEICON_SOURCE_H_

#include <string>

#include "base/files/file_path.h"
#include "base/task/cancelable_task_tracker.h"
#include "chrome/browser/icon_manager.h"
#include "content/public/browser/url_data_source.h"

namespace gfx {
class Image;
}

// FileIconSource is the gateway between network-level chrome:
// requests for favicons and the history backend that serves these.
class FileIconSource : public content::URLDataSource {
 public:
  FileIconSource();

  FileIconSource(const FileIconSource&) = delete;
  FileIconSource& operator=(const FileIconSource&) = delete;

  ~FileIconSource() override;

  // content::URLDataSource implementation.
  std::string GetSource() override;
  void StartDataRequest(
      const GURL& url,
      const content::WebContents::Getter& wc_getter,
      content::URLDataSource::GotDataCallback callback) override;
  std::string GetMimeType(const GURL&) override;
  bool AllowCaching() override;

 protected:
  // Once the |path| and |icon_size| has been determined from the request, this
  // function is called to perform the actual fetch. Declared as virtual for
  // testing.
  virtual void FetchFileIcon(const base::FilePath& path,
                             float scale_factor,
                             IconLoader::IconSize icon_size,
                             content::URLDataSource::GotDataCallback callback);

 private:
  // Contains the necessary information for completing an icon fetch request.
  struct IconRequestDetails {
    IconRequestDetails();

    IconRequestDetails(IconRequestDetails&& other);
    IconRequestDetails& operator=(IconRequestDetails&& other);

    ~IconRequestDetails();

    // The callback to run with the response.
    content::URLDataSource::GotDataCallback callback;

    // The requested scale factor to respond with.
    float scale_factor = 1;
  };

  // Called when favicon data is available from the history backend.
  void OnFileIconDataAvailable(IconRequestDetails details, gfx::Image icon);

  // Tracks tasks requesting file icons.
  base::CancelableTaskTracker cancelable_task_tracker_;
};
#endif  // CHROME_BROWSER_UI_WEBUI_FILEICON_SOURCE_H_
