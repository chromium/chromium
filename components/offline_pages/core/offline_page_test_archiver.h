// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_OFFLINE_PAGE_TEST_ARCHIVER_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_OFFLINE_PAGE_TEST_ARCHIVER_H_

#include <stddef.h>
#include <stdint.h>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/task/single_thread_task_runner.h"
#include "components/offline_pages/core/offline_page_archiver.h"

class GURL;

namespace base {
class FilePath;
}  // namespace base

namespace offline_pages {

// A test archiver class, which allows for testing offline pages without a need
// for an actual web contents.
class OfflinePageTestArchiver : public OfflinePageArchiver {
 public:
  class Observer {
   public:
    virtual ~Observer() = default;
    virtual void SetLastPathCreatedByArchiver(
        const base::FilePath& file_path) = 0;
  };

  OfflinePageTestArchiver(
      Observer* observer,
      const GURL& url,
      ArchiverResult result,
      const std::u16string& result_title,
      int64_t size_to_report,
      const std::string& digest_to_report,
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner);

  OfflinePageTestArchiver(const OfflinePageTestArchiver&) = delete;
  OfflinePageTestArchiver& operator=(const OfflinePageTestArchiver&) = delete;

  ~OfflinePageTestArchiver() override;

  // OfflinePageArchiver implementation:
  void CreateArchive(const base::FilePath& archives_dir,
                     const CreateArchiveParams& create_archive_params,
                     content::WebContents* web_contents,
                     CreateArchiveCallback callback) override;

  // Completes the creation of archive. Should be used with |set_delayed| set to
  // true.
  void CompleteCreateArchive();

  // Set whether to check create_archive_called_ on destruction.
  void ExpectCreateArchiveCalled(bool expect) {
    expect_create_archive_called_ = expect;
  }

  // When set to true, |CompleteCreateArchive| should be called explicitly for
  // the process to finish.
  // TODO(fgorski): See if we can move this to the constructor.
  void set_delayed(bool delayed) { delayed_ = delayed; }

  // Allows to explicitly specify a file name for the tests.
  // TODO(fgorski): See if we can move this to the constructor.
  void set_filename(const base::FilePath& filename) { filename_ = filename; }

  const CreateArchiveParams& create_archive_params() const {
    return create_archive_params_;
  }

  bool create_archive_called() const { return create_archive_called_; }

 private:
  // Not owned. Outlives OfflinePageTestArchiver.
  raw_ptr<Observer> observer_;
  GURL url_;
  base::FilePath archives_dir_;
  CreateArchiveParams create_archive_params_;
  base::FilePath filename_;
  ArchiverResult result_;
  int64_t size_to_report_;
  bool expect_create_archive_called_;
  bool create_archive_called_;
  bool delayed_;
  std::u16string result_title_;
  std::string digest_to_report_;
  CreateArchiveCallback callback_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_OFFLINE_PAGE_TEST_ARCHIVER_H_
